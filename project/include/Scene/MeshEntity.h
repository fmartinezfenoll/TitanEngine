#pragma once
#include "Scene/TEntity.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <memory>

class Material;

struct MeshVertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f};
    glm::vec2 uv{0.0f};
};

class MeshEntity : public TEntity {
public:
    MeshEntity(const std::vector<MeshVertex>& vertices,
               const std::vector<uint32_t>& indices,
               const std::shared_ptr<Material>& material);
    ~MeshEntity() override;

    MeshEntity(const MeshEntity&) = delete;
    MeshEntity& operator=(const MeshEntity&) = delete;

    void draw(const glm::mat4& modelMatrix) override;
    void update(float deltaTime) override {}

private:
    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;
    unsigned int m_EBO = 0;
    size_t m_IndexCount = 0;
    std::shared_ptr<Material> m_Material;
};
