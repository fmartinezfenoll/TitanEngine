#include "Renderer/Skybox.h"
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/OpenGLShader.h"
#include "ResourceManager/CubemapTexture.h"
#include "Core/Log.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <array>

namespace {
// Six view matrices for a cube centered at the origin, one per cubemap
// face -- same face order as glTexImage2D's GL_TEXTURE_CUBE_MAP_POSITIVE_X..
// sequence. Paired with a shared 90-degree-FOV projection.
std::array<glm::mat4, 6> CubeFaceViews() {
    static const glm::vec3 dirs[6] = {
        glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0),
        glm::vec3(0, 1, 0), glm::vec3(0, -1, 0),
        glm::vec3(0, 0, 1), glm::vec3(0, 0, -1)
    };
    static const glm::vec3 ups[6] = {
        glm::vec3(0, -1, 0), glm::vec3(0, -1, 0),
        glm::vec3(0, 0, 1),  glm::vec3(0, 0, -1),
        glm::vec3(0, -1, 0), glm::vec3(0, -1, 0)
    };

    std::array<glm::mat4, 6> result;
    for (int i = 0; i < 6; ++i) {
        result[i] = glm::lookAt(glm::vec3(0.0f), dirs[i], ups[i]);
    }
    return result;
}

glm::mat4 CubeFaceProjection(float nearPlane, float farPlane) {
    return glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, farPlane);
}

std::array<glm::mat4, 6> CubeFaceViewProjections(float nearPlane, float farPlane) {
    glm::mat4 proj = CubeFaceProjection(nearPlane, farPlane);
    std::array<glm::mat4, 6> views = CubeFaceViews();
    std::array<glm::mat4, 6> result;
    for (int i = 0; i < 6; ++i) result[i] = proj * views[i];
    return result;
}
}

Skybox::Skybox(std::shared_ptr<CubemapTexture> cubemapIn, const std::string& nameIn)
    : cubemap(std::move(cubemapIn)), name(nameIn)
{
}

Skybox::~Skybox()
{
    if (irradianceMapID != 0) glDeleteTextures(1, &irradianceMapID);
    if (prefilterMapID != 0) glDeleteTextures(1, &prefilterMapID);
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

void Skybox::CreateIrradianceStorage()
{
    glGenTextures(1, &irradianceMapID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMapID);

    for (int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, kIrradianceResolution,
                     kIrradianceResolution, 0, GL_RGB, GL_FLOAT, nullptr);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void Skybox::CreatePrefilterStorage()
{
    glGenTextures(1, &prefilterMapID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMapID);

    for (int mip = 0; mip < kPrefilterMipLevels; ++mip) {
        int mipRes = kPrefilterResolution >> mip;
        for (int i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, mip, GL_RGB16F, mipRes, mipRes, 0,
                         GL_RGB, GL_FLOAT, nullptr);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void Skybox::BakeIrradiance()
{
    auto shader = ResourceManager::LoadShader("ibl_irradiance", true);
    if (!shader) return;

    unsigned int fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, irradianceMapID, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        Log::Error("Skybox irradiance framebuffer incomplete");

    glViewport(0, 0, kIrradianceResolution, kIrradianceResolution);

    shader->Bind();
    auto matrices = CubeFaceViewProjections(0.1f, 10.0f);
    for (int i = 0; i < 6; ++i)
        shader->SetMat4("shadowMatrices[" + std::to_string(i) + "]", matrices[i]);
    cubemap->Bind(0);
    shader->SetInt("environmentMap", 0);

    glBindVertexArray(CubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
}

void Skybox::BakePrefilter()
{
    auto shader = ResourceManager::LoadShader("ibl_prefilter");
    if (!shader) return;

    unsigned int fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    shader->Bind();
    cubemap->Bind(0);
    shader->SetInt("environmentMap", 0);

    auto views = CubeFaceViews();
    glm::mat4 projection = CubeFaceProjection(0.1f, 10.0f);
    shader->SetMat4("projection", projection);

    for (int mip = 0; mip < kPrefilterMipLevels; ++mip) {
        int mipRes = kPrefilterResolution >> mip;
        glViewport(0, 0, mipRes, mipRes);

        float roughness = static_cast<float>(mip) / static_cast<float>(kPrefilterMipLevels - 1);
        shader->SetFloat("roughness", roughness);

        for (int face = 0; face < 6; ++face) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, prefilterMapID, mip);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            glReadBuffer(GL_NONE);

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                Log::Error("Skybox prefilter framebuffer incomplete (mip " + std::to_string(mip) + ")");

            shader->SetMat4("view", views[face]);

            glBindVertexArray(CubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
}

void Skybox::EnsureIBLGenerated()
{
    if (iblGenerated) return;
    if (!cubemap || !cubemap->IsValid() || CubeVAO == 0) return;

    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    CreateIrradianceStorage();
    CreatePrefilterStorage();
    BakeIrradiance();
    BakePrefilter();

    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

    iblGenerated = true;
}
