#pragma once
#include "Scene/Component.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

class MaterialComponent;

struct MeshVertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f};
    glm::vec2 uv{0.0f};
};

class MeshComponent : public Component {
public:
    MeshComponent(const std::vector<MeshVertex>& vertices,
                  const std::vector<uint32_t>& indices);
    ~MeshComponent() override;

    MeshComponent(const MeshComponent&) = delete;
    MeshComponent& operator=(const MeshComponent&) = delete;

    void Draw(const glm::mat4& modelMatrix, MaterialComponent* material) const;

    size_t GetVertexCount() const { return m_VertexCount; }
    size_t GetIndexCount() const { return m_IndexCount; }

    const std::vector<MeshVertex>& GetVertices() const { return m_Vertices; }
    const std::vector<uint32_t>& GetIndices() const { return m_Indices; }

private:
    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;
    unsigned int m_EBO = 0;
    size_t m_VertexCount = 0;
    size_t m_IndexCount = 0;
    std::vector<MeshVertex> m_Vertices;
    std::vector<uint32_t> m_Indices;
};
