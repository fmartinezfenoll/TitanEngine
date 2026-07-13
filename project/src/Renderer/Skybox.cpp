#include "Renderer/Skybox.h"
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/OpenGLShader.h"
#include "ResourceManager/CubemapTexture.h"

#include <glad/glad.h>

Skybox::Skybox(std::shared_ptr<CubemapTexture> cubemapIn, const std::string& nameIn)
    : cubemap(std::move(cubemapIn)), name(nameIn)
{
}

void Skybox::InitSharedGeometry()
{
    if (CubeVAO != 0) return;

    float vertices[] = {
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &CubeVAO);
    glGenBuffers(1, &CubeVBO);

    glBindVertexArray(CubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, CubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Skybox::ShutdownSharedGeometry()
{
    if (CubeVAO != 0) glDeleteVertexArrays(1, &CubeVAO);
    if (CubeVBO != 0) glDeleteBuffers(1, &CubeVBO);
    CubeVAO = 0;
    CubeVBO = 0;
}

void Skybox::Draw(const glm::mat4& view, const glm::mat4& projection) const
{
    if (!cubemap || !cubemap->IsValid() || CubeVAO == 0) return;

    auto shader = ResourceManager::LoadShader("skybox");
    if (!shader) return;

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    shader->Bind();
    shader->SetMat4("view", view);
    shader->SetMat4("projection", projection);
    cubemap->Bind(0);
    shader->SetInt("skyboxMap", 0);

    glBindVertexArray(CubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}
