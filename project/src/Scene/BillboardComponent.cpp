#include "Scene/BillboardComponent.h"
#include "Renderer/BillboardGeometry.h"
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/OpenGLShader.h"
#include "ResourceManager/Texture.h"
#include <glad/glad.h>

void BillboardComponent::Draw(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& worldCenter) const
{
    auto shader = ResourceManager::LoadShader("billboard");
    if (!shader) return;

    shader->Bind();
    shader->SetMat4("view", view);
    shader->SetMat4("projection", projection);
    shader->SetVec3("worldCenter", worldCenter);
    shader->SetVec2("size", size);
    shader->SetVec4("tint", tint);
    shader->SetFloat("alphaCutoff", alphaCutoff);

    shader->SetBool("hasTexture", texture != nullptr);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture ? texture->GetID() : 0);
    shader->SetInt("billboardTexture", 0);

    BillboardGeometry::Bind();
    glDrawArrays(GL_TRIANGLES, 0, BillboardGeometry::kVertexCount);
    BillboardGeometry::Unbind();
}
