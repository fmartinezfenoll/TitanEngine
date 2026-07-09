#include "Renderer/GizmoRenderer.h"
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/OpenGLShader.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;

std::vector<glm::vec3> BuildWireSphere(float radius, int segments)
{
    std::vector<glm::vec3> lines;

    // Three orthogonal circles (XY, XZ, YZ planes) approximate a wire sphere.
    for (int ring = 0; ring < 3; ++ring) {
        for (int i = 0; i < segments; ++i) {
            float a0 = (2.0f * kPi * i) / segments;
            float a1 = (2.0f * kPi * (i + 1)) / segments;

            glm::vec3 p0, p1;
            switch (ring) {
                case 0: // XY plane
                    p0 = { radius * cos(a0), radius * sin(a0), 0.0f };
                    p1 = { radius * cos(a1), radius * sin(a1), 0.0f };
                    break;
                case 1: // XZ plane
                    p0 = { radius * cos(a0), 0.0f, radius * sin(a0) };
                    p1 = { radius * cos(a1), 0.0f, radius * sin(a1) };
                    break;
                default: // YZ plane
                    p0 = { 0.0f, radius * cos(a0), radius * sin(a0) };
                    p1 = { 0.0f, radius * cos(a1), radius * sin(a1) };
                    break;
            }

            lines.push_back(p0);
            lines.push_back(p1);
        }
    }

    return lines;
}

std::vector<glm::vec3> BuildWireFrustum(float depth)
{
    // Apex at origin, looking down -Z, matching the engine's forward convention.
    glm::vec3 apex(0.0f, 0.0f, 0.0f);
    float halfW = depth * 0.6f;
    float halfH = depth * 0.4f;

    glm::vec3 baseTL(-halfW,  halfH, -depth);
    glm::vec3 baseTR( halfW,  halfH, -depth);
    glm::vec3 baseBL(-halfW, -halfH, -depth);
    glm::vec3 baseBR( halfW, -halfH, -depth);

    std::vector<glm::vec3> lines = {
        // Apex to base corners
        apex, baseTL,
        apex, baseTR,
        apex, baseBL,
        apex, baseBR,
        // Base rectangle
        baseTL, baseTR,
        baseTR, baseBR,
        baseBR, baseBL,
        baseBL, baseTL,
    };

    return lines;
}

std::vector<glm::vec3> BuildWireCube()
{
    // Unit cube, corners at +/-0.5, 12 edges.
    glm::vec3 c[8] = {
        {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f},
        {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
    };

    return {
        // Back face
        c[0], c[1], c[1], c[2], c[2], c[3], c[3], c[0],
        // Front face
        c[4], c[5], c[5], c[6], c[6], c[7], c[7], c[4],
        // Connecting edges
        c[0], c[4], c[1], c[5], c[2], c[6], c[3], c[7],
    };
}

std::vector<glm::vec3> BuildWireArrow(float length)
{
    // Shaft along +X, small splayed-line arrowhead near the tip.
    glm::vec3 origin(0.0f, 0.0f, 0.0f);
    glm::vec3 tip(length, 0.0f, 0.0f);
    float headBack = length * 0.85f;
    float headSize = length * 0.08f;

    glm::vec3 headBaseUp(headBack, headSize, 0.0f);
    glm::vec3 headBaseDown(headBack, -headSize, 0.0f);
    glm::vec3 headBaseFwd(headBack, 0.0f, headSize);
    glm::vec3 headBaseBack(headBack, 0.0f, -headSize);

    return {
        origin, tip,
        tip, headBaseUp,
        tip, headBaseDown,
        tip, headBaseFwd,
        tip, headBaseBack,
    };
}

std::vector<glm::vec3> BuildWireShaft(float length)
{
    // Plain line along +X, no arrowhead -- used for the Scale gizmo's shaft.
    return { glm::vec3(0.0f), glm::vec3(length, 0.0f, 0.0f) };
}

std::vector<glm::vec3> BuildWireCircle(float radius, int segments)
{
    // Single ring in the XY plane.
    std::vector<glm::vec3> lines;
    for (int i = 0; i < segments; ++i) {
        float a0 = (2.0f * kPi * i) / segments;
        float a1 = (2.0f * kPi * (i + 1)) / segments;
        lines.push_back({ radius * cos(a0), radius * sin(a0), 0.0f });
        lines.push_back({ radius * cos(a1), radius * sin(a1), 0.0f });
    }
    return lines;
}

unsigned int UploadLineVAO(const std::vector<glm::vec3>& vertices, unsigned int& outVBO)
{
    unsigned int vao = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &outVBO);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, outVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    return vao;
}

} // namespace

void GizmoRenderer::Init()
{
    ResourceManager::LoadShader("gizmo");

    auto sphereVerts = BuildWireSphere(kLightGizmoRadius, 24);
    s_SphereVAO = UploadLineVAO(sphereVerts, s_SphereVBO);
    s_SphereVertexCount = static_cast<int>(sphereVerts.size());

    auto frustumVerts = BuildWireFrustum(kCameraGizmoRadius);
    s_FrustumVAO = UploadLineVAO(frustumVerts, s_FrustumVBO);
    s_FrustumVertexCount = static_cast<int>(frustumVerts.size());

    auto cubeVerts = BuildWireCube();
    s_CubeVAO = UploadLineVAO(cubeVerts, s_CubeVBO);
    s_CubeVertexCount = static_cast<int>(cubeVerts.size());

    auto arrowVerts = BuildWireArrow(kGizmoArmLength);
    s_ArrowVAO = UploadLineVAO(arrowVerts, s_ArrowVBO);
    s_ArrowVertexCount = static_cast<int>(arrowVerts.size());

    auto ringVerts = BuildWireCircle(kGizmoRingRadius, 32);
    s_RingVAO = UploadLineVAO(ringVerts, s_RingVBO);
    s_RingVertexCount = static_cast<int>(ringVerts.size());

    auto shaftVerts = BuildWireShaft(kGizmoArmLength);
    s_ShaftVAO = UploadLineVAO(shaftVerts, s_ShaftVBO);
    s_ShaftVertexCount = static_cast<int>(shaftVerts.size());
}

void GizmoRenderer::Shutdown()
{
    glDeleteVertexArrays(1, &s_SphereVAO);
    glDeleteBuffers(1, &s_SphereVBO);
    glDeleteVertexArrays(1, &s_FrustumVAO);
    glDeleteBuffers(1, &s_FrustumVBO);
    glDeleteVertexArrays(1, &s_CubeVAO);
    glDeleteBuffers(1, &s_CubeVBO);
    glDeleteVertexArrays(1, &s_ArrowVAO);
    glDeleteBuffers(1, &s_ArrowVBO);
    glDeleteVertexArrays(1, &s_RingVAO);
    glDeleteBuffers(1, &s_RingVBO);
    glDeleteVertexArrays(1, &s_ShaftVAO);
    glDeleteBuffers(1, &s_ShaftVBO);
}

void GizmoRenderer::DrawLightGizmo(const glm::vec3& worldPos, const glm::vec3& color,
                                   const glm::mat4& view, const glm::mat4& projection)
{
    auto shader = ResourceManager::GetShader("gizmo");
    if (!shader) return;

    glm::mat4 model = glm::translate(glm::mat4(1.0f), worldPos);

    shader->Bind();
    shader->SetMat4("model", model);
    shader->SetMat4("view", view);
    shader->SetMat4("projection", projection);
    shader->SetVec3("color", color);

    glBindVertexArray(s_SphereVAO);
    glDrawArrays(GL_LINES, 0, s_SphereVertexCount);
}

void GizmoRenderer::DrawCameraGizmo(const glm::mat4& cameraModelMatrix,
                                    const glm::mat4& view, const glm::mat4& projection)
{
    auto shader = ResourceManager::GetShader("gizmo");
    if (!shader) return;

    shader->Bind();
    shader->SetMat4("model", cameraModelMatrix);
    shader->SetMat4("view", view);
    shader->SetMat4("projection", projection);
    shader->SetVec3("color", glm::vec3(0.9f, 0.9f, 0.2f));

    glBindVertexArray(s_FrustumVAO);
    glDrawArrays(GL_LINES, 0, s_FrustumVertexCount);
}

void GizmoRenderer::DrawSelectionBox(const glm::vec3& worldCenter, const glm::vec3& worldExtents,
                                     const glm::mat4& view, const glm::mat4& projection)
{
    auto shader = ResourceManager::GetShader("gizmo");
    if (!shader) return;

    glm::mat4 model = glm::translate(glm::mat4(1.0f), worldCenter);
    model = glm::scale(model, worldExtents * 2.0f);

    shader->Bind();
    shader->SetMat4("model", model);
    shader->SetMat4("view", view);
    shader->SetMat4("projection", projection);
    shader->SetVec3("color", glm::vec3(1.0f, 0.6f, 0.1f));

    glBindVertexArray(s_CubeVAO);
    glDrawArrays(GL_LINES, 0, s_CubeVertexCount);
}

float GizmoRenderer::ComputeGizmoScale(const glm::vec3& worldPos, const glm::vec3& cameraWorldPos)
{
    float distance = glm::length(worldPos - cameraWorldPos);
    return distance * kGizmoScreenScaleFactor;
}

namespace {

// X axis = identity, Y axis = rotate +90 about Z, Z axis = rotate -90 about Y.
glm::mat4 AxisRotation(int axis)
{
    switch (axis) {
        case 1: return glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        case 2: return glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        default: return glm::mat4(1.0f);
    }
}

glm::vec3 AxisColor(int axis)
{
    switch (axis) {
        case 1: return glm::vec3(0.2f, 0.9f, 0.2f);
        case 2: return glm::vec3(0.2f, 0.4f, 0.95f);
        default: return glm::vec3(0.9f, 0.2f, 0.2f);
    }
}

} // namespace

void GizmoRenderer::DrawMoveGizmo(const glm::vec3& worldPos, const glm::mat4& baseRotation, float scale,
                                  const glm::mat4& view, const glm::mat4& projection)
{
    auto shader = ResourceManager::GetShader("gizmo");
    if (!shader) return;

    shader->Bind();
    shader->SetMat4("view", view);
    shader->SetMat4("projection", projection);

    glm::mat4 base = glm::translate(glm::mat4(1.0f), worldPos) * baseRotation * glm::scale(glm::mat4(1.0f), glm::vec3(scale));

    glBindVertexArray(s_ArrowVAO);
    for (int axis = 0; axis < 3; ++axis) {
        glm::mat4 model = base * AxisRotation(axis);
        shader->SetMat4("model", model);
        shader->SetVec3("color", AxisColor(axis));
        glDrawArrays(GL_LINES, 0, s_ArrowVertexCount);
    }
}

void GizmoRenderer::DrawRotateGizmo(const glm::vec3& worldPos, const glm::mat4& baseRotation, float scale,
                                    const glm::mat4& view, const glm::mat4& projection)
{
    auto shader = ResourceManager::GetShader("gizmo");
    if (!shader) return;

    shader->Bind();
    shader->SetMat4("view", view);
    shader->SetMat4("projection", projection);

    glm::mat4 base = glm::translate(glm::mat4(1.0f), worldPos) * baseRotation * glm::scale(glm::mat4(1.0f), glm::vec3(scale));

    // Ring is built in the XY plane, which rotates *around* Z -- rotate the
    // template so ring[axis] lies in the plane perpendicular to that axis.
    glBindVertexArray(s_RingVAO);
    for (int axis = 0; axis < 3; ++axis) {
        glm::mat4 ringOrient;
        switch (axis) {
            case 0: ringOrient = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)); break;
            case 1: ringOrient = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); break;
            default: ringOrient = glm::mat4(1.0f); break;
        }
        glm::mat4 model = base * ringOrient;
        shader->SetMat4("model", model);
        shader->SetVec3("color", AxisColor(axis));
        glDrawArrays(GL_LINES, 0, s_RingVertexCount);
    }
}

void GizmoRenderer::DrawScaleGizmo(const glm::vec3& worldPos, const glm::mat4& baseRotation, float scale,
                                   const glm::mat4& view, const glm::mat4& projection)
{
    auto shader = ResourceManager::GetShader("gizmo");
    if (!shader) return;

    shader->Bind();
    shader->SetMat4("view", view);
    shader->SetMat4("projection", projection);

    glm::mat4 base = glm::translate(glm::mat4(1.0f), worldPos) * baseRotation * glm::scale(glm::mat4(1.0f), glm::vec3(scale));
    float armLength = kGizmoArmLength * 0.7f;
    float tipCubeSize = 0.12f;

    for (int axis = 0; axis < 3; ++axis) {
        glm::mat4 axisRot = AxisRotation(axis);
        glm::mat4 axisBase = base * axisRot;
        glm::vec3 color = AxisColor(axis);

        glm::mat4 shaftModel = axisBase * glm::scale(glm::mat4(1.0f), glm::vec3(armLength));
        shader->SetMat4("model", shaftModel);
        shader->SetVec3("color", color);
        glBindVertexArray(s_ShaftVAO);
        glDrawArrays(GL_LINES, 0, s_ShaftVertexCount);

        glm::mat4 tipModel = axisBase * glm::translate(glm::mat4(1.0f), glm::vec3(armLength, 0.0f, 0.0f))
                              * glm::scale(glm::mat4(1.0f), glm::vec3(tipCubeSize));
        shader->SetMat4("model", tipModel);
        glBindVertexArray(s_CubeVAO);
        glDrawArrays(GL_LINES, 0, s_CubeVertexCount);
    }
}
