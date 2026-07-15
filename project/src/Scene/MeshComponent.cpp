#include "Scene/MeshComponent.h"
#include "Scene/MaterialComponent.h"
#include "Scene/SkinComponent.h"
#include "ResourceManager/Material.h"
#include "ResourceManager/OpenGLShader.h"
#include "Core/Stats.h"
#include <glad/glad.h>
#include <string>
#include <algorithm>

namespace {
constexpr int kMaxLights = 32;

// Lengyel's method: accumulate per-triangle tangents weighted by UV area,
// then orthonormalize per-vertex against the interpolated normal (Gram-Schmidt).
void ComputeTangents(std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices) {
    std::vector<glm::vec3> accum(vertices.size(), glm::vec3(0.0f));

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i], i1 = indices[i + 1], i2 = indices[i + 2];
        const MeshVertex& v0 = vertices[i0];
        const MeshVertex& v1 = vertices[i1];
        const MeshVertex& v2 = vertices[i2];

        glm::vec3 edge1 = v1.position - v0.position;
        glm::vec3 edge2 = v2.position - v0.position;
        glm::vec2 deltaUV1 = v1.uv - v0.uv;
        glm::vec2 deltaUV2 = v2.uv - v0.uv;

        float denom = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        if (std::abs(denom) < 1e-8f) continue;

        float r = 1.0f / denom;
        glm::vec3 tangent = (edge1 * deltaUV2.y - edge2 * deltaUV1.y) * r;

        accum[i0] += tangent;
        accum[i1] += tangent;
        accum[i2] += tangent;
    }

    for (size_t i = 0; i < vertices.size(); ++i) {
        const glm::vec3& n = vertices[i].normal;
        glm::vec3 t = accum[i];

        // Gram-Schmidt orthogonalize against the normal.
        t = t - n * glm::dot(n, t);

        float len = glm::length(t);
        vertices[i].tangent = (len > 1e-8f) ? (t / len) : glm::vec3(1.0f, 0.0f, 0.0f);
    }
}
}

MeshComponent::MeshComponent(const std::vector<MeshVertex>& vertices,
                             const std::vector<uint32_t>& indices)
    : VertexCount(vertices.size()), IndexCount(indices.size()),
      Vertices(vertices), Indices(indices)
{
    ComputeTangents(Vertices, indices);

    if (!vertices.empty()) {
        LocalMin = LocalMax = vertices[0].position;
        for (const MeshVertex& v : vertices) {
            LocalMin = glm::min(LocalMin, v.position);
            LocalMax = glm::max(LocalMax, v.position);
        }
    }

    for (const MeshVertex& v : Vertices) {
        if (v.jointWeights != glm::vec4(0.0f)) {
            SkinnedMesh = true;
            break;
        }
    }

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, Vertices.size() * sizeof(MeshVertex), Vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, normal));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, uv));
    glEnableVertexAttribArray(2);

    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, tangent));
    glEnableVertexAttribArray(3);

    glVertexAttribIPointer(4, 4, GL_INT, sizeof(MeshVertex), (void*)offsetof(MeshVertex, jointIndices));
    glEnableVertexAttribArray(4);

    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, jointWeights));
    glEnableVertexAttribArray(5);

    glBindVertexArray(0);
}

glm::vec3 MeshComponent::RecenterPivot() {
    glm::vec3 center = (LocalMin + LocalMax) * 0.5f;
    if (center == glm::vec3(0.0f)) return glm::vec3(0.0f);

    for (MeshVertex& v : Vertices) {
        v.position -= center;
    }
    LocalMin -= center;
    LocalMax -= center;

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, Vertices.size() * sizeof(MeshVertex), Vertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return center;
}

MeshComponent::~MeshComponent() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

void MeshComponent::Draw(const glm::mat4& modelMatrix, MaterialComponent* material,
                         const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraWorldPos,
                         const std::vector<LightUniformData>& lights,
                         const ShadowRenderData& shadowData, const IBLRenderData& iblData,
                         SkinComponent* skin) const {
    if (!material || !material->material) return;

    auto shader = material->material->GetShader();
    if (!shader) return;

    material->Bind();
    shader->SetMat4("projection", projection);
    shader->SetMat4("view", view);
    shader->SetMat4("model", modelMatrix);
    shader->SetVec3("cameraWorldPos", cameraWorldPos);

    if (SkinnedMesh) {
        // Always upload joint matrices for a skinned mesh, even when there's no
        // skin or the skin resolved to nothing -- otherwise jointMatrices keeps
        // whatever a previously-drawn skinned mesh sharing this program left in
        // it, deforming this mesh with someone else's pose. A single identity is
        // enough: the vertex shader falls back to identity for zero-weight verts
        // and indexes jointMatrices[0] for the rest.
        std::vector<glm::mat4> jointMatrices;
        if (skin) jointMatrices = skin->ComputeJointMatrices(glm::inverse(modelMatrix));
        if (jointMatrices.empty()) jointMatrices.push_back(glm::mat4(1.0f));
        shader->SetMat4Array("jointMatrices", jointMatrices);
    }

    int lightCount = std::min(static_cast<int>(lights.size()), kMaxLights);
    for (int i = 0; i < lightCount; ++i)
    {
        const LightUniformData& light = lights[i];
        std::string prefix = "lights[" + std::to_string(i) + "].";
        shader->SetInt(prefix + "type", light.type);
        shader->SetVec3(prefix + "position", light.position);
        shader->SetVec3(prefix + "direction", light.direction);
        shader->SetVec3(prefix + "color", light.color);
        shader->SetFloat(prefix + "intensity", light.intensity);
        shader->SetFloat(prefix + "range", light.range);
        shader->SetFloat(prefix + "innerCutoff", light.innerCutoff);
        shader->SetFloat(prefix + "outerCutoff", light.outerCutoff);
        shader->SetInt(prefix + "shadowIndex", light.shadowIndex);
    }
    shader->SetInt("lightCount", lightCount);

    // Every sampler uniform below is always given an explicit, distinct
    // texture unit -- even when the corresponding feature (shadows/IBL) is
    // off for this draw. Leaving a sampler uniform at its link-time default
    // (0) when its "enabled" flag is false means it silently shares unit 0
    // with albedoMap; a sampler2D and a samplerCube uniform both resolving
    // to the same unit is a type mismatch that makes the whole draw call
    // fail with GL_INVALID_OPERATION (no GL error message, no visible
    // geometry) as soon as anything else populates that unit for real.
    //
    // Inactive samplers point at dedicated high units (>=11) that the renderer
    // never binds anything to -- NOT at the shadow range (3-7), which the
    // renderer fills DYNAMICALLY in light order (a point light's cubemap can
    // land on unit 3), so reusing those as dummies could re-create the exact
    // type collision. An unbound unit is valid for any sampler type.
    constexpr int kDummyDirShadowUnit = 11;   // sampler2D
    constexpr int kDummySpotShadowUnit = 12;  // sampler2D (12, 13)
    constexpr int kDummyPointShadowUnit = 14; // samplerCube (14, 15)

    shader->SetBool("hasDirectionalShadow", shadowData.hasDirectional);
    shader->SetInt("directionalShadowMap", shadowData.hasDirectional ? static_cast<int>(shadowData.directionalSlot) : kDummyDirShadowUnit);
    if (shadowData.hasDirectional) {
        shader->SetMat4("directionalLightSpaceMatrix", shadowData.directionalLightSpaceMatrix);
    }

    shader->SetInt("spotShadowCount", shadowData.spotCount);
    for (int i = 0; i < 2; ++i) {
        std::string idx = "[" + std::to_string(i) + "]";
        int unit = (i < shadowData.spotCount) ? static_cast<int>(shadowData.spotSlots[i]) : kDummySpotShadowUnit + i;
        shader->SetInt("spotShadowMaps" + idx, unit);
        if (i < shadowData.spotCount) {
            shader->SetMat4("spotLightSpaceMatrices" + idx, shadowData.spotLightSpaceMatrices[i]);
        }
    }

    shader->SetInt("pointShadowCount", shadowData.pointCount);
    for (int i = 0; i < 2; ++i) {
        std::string idx = "[" + std::to_string(i) + "]";
        int unit = (i < shadowData.pointCount) ? static_cast<int>(shadowData.pointSlots[i]) : kDummyPointShadowUnit + i;
        shader->SetInt("pointShadowMaps" + idx, unit);
        if (i < shadowData.pointCount) {
            shader->SetVec3("pointShadowLightPos" + idx, shadowData.pointLightPos[i]);
            shader->SetFloat("pointShadowFarPlane" + idx, shadowData.pointFarPlane[i]);
        }
    }

    shader->SetBool("hasIBL", iblData.hasIBL);
    shader->SetInt("irradianceMap", static_cast<int>(iblData.irradianceSlot));
    shader->SetInt("prefilterMap", static_cast<int>(iblData.prefilterSlot));
    shader->SetInt("brdfLUT", static_cast<int>(iblData.brdfLUTSlot));

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(IndexCount), GL_UNSIGNED_INT, 0);
    Stats::RecordDrawCall();
}

void MeshComponent::DrawDepthOnly(const glm::mat4& modelMatrix, OpenGLShader* depthShader) const {
    if (!depthShader) return;

    depthShader->SetMat4("model", modelMatrix);

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(IndexCount), GL_UNSIGNED_INT, 0);
    Stats::RecordDrawCall();
}
