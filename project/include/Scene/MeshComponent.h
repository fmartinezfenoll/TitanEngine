#pragma once
#include "Scene/Component.h"
#include "Scene/LightUniformData.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

class MaterialComponent;
class OpenGLShader;

struct MeshVertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f};
    glm::vec2 uv{0.0f};
    glm::vec3 tangent{0.0f}; // Derived automatically by MeshComponent from position/normal/uv; callers never set this.
};

class MeshComponent : public Component {
public:
    MeshComponent(const std::vector<MeshVertex>& vertices,
                  const std::vector<uint32_t>& indices);
    ~MeshComponent() override;

    MeshComponent(const MeshComponent&) = delete;
    MeshComponent& operator=(const MeshComponent&) = delete;

    void Draw(const glm::mat4& modelMatrix, MaterialComponent* material,
              const glm::mat4& view, const glm::mat4& projection,
              const std::vector<LightUniformData>& lights,
              const ShadowRenderData& shadowData) const;

    void DrawDepthOnly(const glm::mat4& modelMatrix, OpenGLShader* depthShader) const;

    size_t GetVertexCount() const { return VertexCount; }
    size_t GetIndexCount() const { return IndexCount; }

    const std::vector<MeshVertex>& GetVertices() const { return Vertices; }
    const std::vector<uint32_t>& GetIndices() const { return Indices; }

    void GetLocalBounds(glm::vec3& outMin, glm::vec3& outMax) const {
        outMin = LocalMin;
        outMax = LocalMax;
    }

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
};
