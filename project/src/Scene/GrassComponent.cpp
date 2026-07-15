#include "Scene/GrassComponent.h"
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/OpenGLShader.h"
#include "ResourceManager/Texture.h"
#include <glad/glad.h>
#include <glm/gtc/constants.hpp>
#include <random>
#include <vector>

namespace {
// Same unit quad as BillboardGeometry, but the grass VAO needs its own copy so
// it can attach a per-instance attribute alongside it. Interleaved (posX, posY,
// u, v); blade base sits at y=0 in the shader (Corner.y + 0.5).
const float kQuadVertices[] = {
    -0.5f, -0.5f,  0.0f, 0.0f,
     0.5f, -0.5f,  1.0f, 0.0f,
     0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f, -0.5f,  0.0f, 0.0f,
     0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f,  0.0f, 1.0f,
};
constexpr int kQuadVertexCount = 6;
}

GrassComponent::GrassComponent() = default;

GrassComponent::~GrassComponent() {
    if (vao != 0) glDeleteVertexArrays(1, &vao);
    if (quadVBO != 0) glDeleteBuffers(1, &quadVBO);
    if (instanceVBO != 0) glDeleteBuffers(1, &instanceVBO);
}

void GrassComponent::EnsureGLObjects() {
    if (vao != 0) return;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &quadVBO);
    glGenBuffers(1, &instanceVBO);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Per-instance: vec4 (xyz offset, w yaw). Divisor 1 = advances once per
    // instance instead of per vertex.
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    glBindVertexArray(0);
}

void GrassComponent::Rebuild() {
    EnsureGLObjects();

    // Two crossed quads per blade (yaw offset by 90 deg) for a fuller look from
    // any angle. Deterministic scatter from `seed`.
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> distX(-areaSize.x * 0.5f, areaSize.x * 0.5f);
    std::uniform_real_distribution<float> distZ(-areaSize.y * 0.5f, areaSize.y * 0.5f);
    std::uniform_real_distribution<float> distYaw(0.0f, glm::pi<float>());

    int blades = density < 0 ? 0 : density;
    std::vector<glm::vec4> instances;
    instances.reserve(static_cast<size_t>(blades) * 2);

    for (int i = 0; i < blades; ++i) {
        float x = distX(rng);
        float z = distZ(rng);
        float yaw = distYaw(rng);
        instances.emplace_back(x, 0.0f, z, yaw);
        instances.emplace_back(x, 0.0f, z, yaw + glm::half_pi<float>());
    }

    instanceCount = static_cast<int>(instances.size());

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, instances.size() * sizeof(glm::vec4),
                 instances.empty() ? nullptr : instances.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    dirty = false;
}

void GrassComponent::Draw(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, float time) const {
    if (dirty) const_cast<GrassComponent*>(this)->Rebuild();
    if (vao == 0 || instanceCount == 0) return;

    auto shader = ResourceManager::LoadShader("grass");
    if (!shader) return;

    shader->Bind();
    shader->SetMat4("model", model);
    shader->SetMat4("view", view);
    shader->SetMat4("projection", projection);
    shader->SetVec2("bladeSize", bladeSize);
    shader->SetFloat("time", time);
    shader->SetFloat("windStrength", windStrength);
    shader->SetFloat("windSpeed", windSpeed);
    shader->SetVec3("tint", tint);
    shader->SetFloat("alphaCutoff", alphaCutoff);

    shader->SetBool("hasTexture", texture != nullptr);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture ? texture->GetID() : 0);
    shader->SetInt("grassTexture", 0);

    // Grass blades are two-sided; don't cull backfaces (culling is off engine-
    // wide anyway, but be explicit in case that changes).
    glBindVertexArray(vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, kQuadVertexCount, instanceCount);
    glBindVertexArray(0);
}
