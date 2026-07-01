#include "Scene/MeshEntity.h"
#include "ResourceManager/Material.h"
#include "ResourceManager/OpenGLShader.h"
#include "Scene/CameraEntity.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/TNode.h"
#include "Renderer/Viewport.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

MeshEntity::MeshEntity(const std::vector<MeshVertex>& vertices,
                       const std::vector<uint32_t>& indices,
                       const std::shared_ptr<Material>& material)
    : m_IndexCount(indices.size()), m_Material(material)
{
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(MeshVertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, normal));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, uv));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

MeshEntity::~MeshEntity() {
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
    glDeleteBuffers(1, &m_EBO);
}

void MeshEntity::draw(const glm::mat4& modelMatrix) {
    if (!m_Material) return;

    auto shader = m_Material->GetShader();
    if (!shader) return;

    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
    glm::mat4 projection = glm::perspective(glm::radians(60.0f), Viewport::GetAspectRatio(), 0.1f, 100.0f);

    Scene* activeScene = SceneManager::Instance().GetActiveScene();
    if (activeScene && activeScene->GetMainCamera())
    {
        TNode* cameraNode = activeScene->GetMainCamera();
        if (auto* camera = dynamic_cast<CameraEntity*>(cameraNode->entity))
        {
            view = camera->GetViewMatrix();
            projection = camera->GetProjectionMatrix(Viewport::GetAspectRatio());
        }
    }

    m_Material->Bind();
    shader->SetMat4("projection", projection);
    shader->SetMat4("view", view);
    shader->SetMat4("model", modelMatrix);

    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_IndexCount), GL_UNSIGNED_INT, 0);
}
