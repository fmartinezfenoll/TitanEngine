#pragma once
#include "Scene/Component.h"
#include "Scene/LightUniformData.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

class MaterialComponent;
class OpenGLShader;
class SkinComponent;

struct MeshVertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f};
    glm::vec2 uv{0.0f};
    glm::vec3 tangent{0.0f}; // Derived automatically by MeshComponent from position/normal/uv; callers never set this.
    glm::ivec4 jointIndices{0};   // glTF JOINTS_0; unused vertices default to (0,0,0,0) with weight 0.
    glm::vec4 jointWeights{0.0f}; // glTF WEIGHTS_0; (0,0,0,0) means "not skinned" (see MeshComponent::HasSkinning).
};

class MeshComponent : public Component {
public:
    MeshComponent(const std::vector<MeshVertex>& vertices,
                  const std::vector<uint32_t>& indices);
    ~MeshComponent() override;

    MeshComponent(const MeshComponent&) = delete;
    MeshComponent& operator=(const MeshComponent&) = delete;

    void Draw(const glm::mat4& modelMatrix, MaterialComponent* material,
              const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraWorldPos,
              const std::vector<LightUniformData>& lights,
              const ShadowRenderData& shadowData, const IBLRenderData& iblData,
              SkinComponent* skin = nullptr, const FogSettings* fog = nullptr) const;

    void DrawDepthOnly(const glm::mat4& modelMatrix, OpenGLShader* depthShader) const;

    size_t GetVertexCount() const { return VertexCount; }
    size_t GetIndexCount() const { return IndexCount; }
    bool HasSkinning() const { return SkinnedMesh; }

    const std::vector<MeshVertex>& GetVertices() const { return Vertices; }
    const std::vector<uint32_t>& GetIndices() const { return Indices; }

    void GetLocalBounds(glm::vec3& outMin, glm::vec3& outMax) const {
        outMin = LocalMin;
        outMax = LocalMax;
    }

    // Shifts all vertices so the local bounding-box center becomes (0,0,0),
    // re-uploads the VBO, and returns the shift applied (world-space, at scale 1)
    // so the caller can compensate the owning node's transform.position.
    glm::vec3 RecenterPivot();

private:
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int EBO = 0;
    size_t VertexCount = 0;
    size_t IndexCount = 0;
    std::vector<MeshVertex> Vertices;
    std::vector<uint32_t> Indices;
    glm::vec3 LocalMin{0.0f};
    glm::vec3 LocalMax{0.0f};
    bool SkinnedMesh = false;
};
