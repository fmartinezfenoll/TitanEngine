#include "Renderer/PostProcessor.h"
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/OpenGLShader.h"
#include "Renderer/Viewport.h"
#include "Core/EngineSettings.h"
#include "Core/Log.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>

namespace {
// SSAO and bloom run at half resolution for performance; the eye barely notices
// on these low-frequency effects.
constexpr int kBloomDivisor = 2;

// A single fullscreen quad (two triangles): pos.xy in [-1,1], uv in [0,1].
constexpr float kQuadVertices[] = {
    // pos        uv
    -1.0f, -1.0f, 0.0f, 0.0f,
     1.0f, -1.0f, 1.0f, 0.0f,
     1.0f,  1.0f, 1.0f, 1.0f,
    -1.0f, -1.0f, 0.0f, 0.0f,
     1.0f,  1.0f, 1.0f, 1.0f,
    -1.0f,  1.0f, 0.0f, 1.0f,
};
} // namespace

PostProcessor::~PostProcessor()
{
    Cleanup();
}

void PostProcessor::InitSharedGeometry()
{
    if (quadVAO != 0) return;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void PostProcessor::ShutdownSharedGeometry()
{
    if (quadVBO) glDeleteBuffers(1, &quadVBO);
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
    quadVBO = quadVAO = 0;
}

void PostProcessor::DrawFullscreenQuad()
{
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void PostProcessor::EnsureResources(int w, int h)
{
    if (w == width && h == height && sceneFBO != 0) return;
    if (w <= 0 || h <= 0) return;

    width = w;
    height = h;
    bloomWidth = std::max(1, w / kBloomDivisor);
    bloomHeight = std::max(1, h / kBloomDivisor);

    DestroyFramebuffers();
    CreateFramebuffers();
}

void PostProcessor::CreateFramebuffers()
{
    // --- HDR scene target: RGBA16F color + depth texture ---
    glGenFramebuffers(1, &sceneFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);

    glGenTextures(1, &sceneColor);
    glBindTexture(GL_TEXTURE_2D, sceneColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColor, 0);

    glGenTextures(1, &sceneDepth);
    glBindTexture(GL_TEXTURE_2D, sceneDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneDepth, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        Log::Error("PostProcessor scene framebuffer incomplete");

    // --- Helper to make a simple single-attachment color FBO ---
    auto makeColorFBO = [](unsigned int& fbo, unsigned int& tex, int fw, int fh,
                           GLint internalFormat, GLenum format, GLenum type) {
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, fw, fh, 0, format, type, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    };

    // Bright pass + bloom ping-pong (half res, HDR).
    makeColorFBO(brightFBO, brightColor, bloomWidth, bloomHeight, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    for (int i = 0; i < 2; ++i)
        makeColorFBO(bloomFBO[i], bloomColor[i], bloomWidth, bloomHeight, GL_RGBA16F, GL_RGBA, GL_FLOAT);

    // SSAO raw + blur (half res, single channel).
    makeColorFBO(ssaoFBO, ssaoColor, bloomWidth, bloomHeight, GL_R8, GL_RED, GL_UNSIGNED_BYTE);
    makeColorFBO(ssaoBlurFBO, ssaoBlurColor, bloomWidth, bloomHeight, GL_R8, GL_RED, GL_UNSIGNED_BYTE);

    // LDR intermediate (full res) so FXAA can sample the composited image.
    makeColorFBO(ldrFBO, ldrColor, width, height, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcessor::DestroyFramebuffers()
{
    unsigned int fbos[] = {sceneFBO, brightFBO, bloomFBO[0], bloomFBO[1], ssaoFBO, ssaoBlurFBO, ldrFBO};
    unsigned int texs[] = {sceneColor, sceneDepth, brightColor, bloomColor[0], bloomColor[1],
                           ssaoColor, ssaoBlurColor, ldrColor};
    for (unsigned int f : fbos) if (f) glDeleteFramebuffers(1, &f);
    for (unsigned int t : texs) if (t) glDeleteTextures(1, &t);

    sceneFBO = brightFBO = bloomFBO[0] = bloomFBO[1] = ssaoFBO = ssaoBlurFBO = ldrFBO = 0;
    sceneColor = sceneDepth = brightColor = bloomColor[0] = bloomColor[1] = 0;
    ssaoColor = ssaoBlurColor = ldrColor = 0;
}

void PostProcessor::Cleanup()
{
    DestroyFramebuffers();
    width = height = 0;
}

void PostProcessor::BeginSceneCapture()
{
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
    glViewport(0, 0, width, height);
}

unsigned int PostProcessor::RunSSAO(const glm::mat4& projection)
{
    glViewport(0, 0, bloomWidth, bloomHeight);
    glDisable(GL_DEPTH_TEST);

    auto shader = ResourceManager::LoadShader("postproc_ssao");
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    shader->Bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneDepth);
    shader->SetInt("depthTexture", 0);
    shader->SetMat4("projection", projection);
    shader->SetMat4("invProjection", glm::inverse(projection));
    shader->SetFloat("radius", EngineSettings::GetSSAORadius());
    shader->SetFloat("intensity", EngineSettings::GetSSAOIntensity());
    // Noise scale tiles a 4x4 rotation pattern across the screen.
    shader->SetVec2("noiseScale", glm::vec2(bloomWidth / 4.0f, bloomHeight / 4.0f));
    DrawFullscreenQuad();

    // Blur the raw AO to remove the per-pixel jitter noise.
    auto blur = ResourceManager::LoadShader("postproc_ssao_blur");
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    blur->Bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ssaoColor);
    blur->SetInt("ssaoInput", 0);
    DrawFullscreenQuad();

    return ssaoBlurColor;
}

unsigned int PostProcessor::RunBloom()
{
    glViewport(0, 0, bloomWidth, bloomHeight);
    glDisable(GL_DEPTH_TEST);

    // Bright pass: extract the bright fragments of the HDR scene.
    auto bright = ResourceManager::LoadShader("postproc_bright");
    glBindFramebuffer(GL_FRAMEBUFFER, brightFBO);
    bright->Bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneColor);
    bright->SetInt("sceneTexture", 0);
    bright->SetFloat("threshold", EngineSettings::GetBloomThreshold());
    DrawFullscreenQuad();

    // Separable Gaussian blur, ping-ponging between the two bloom FBOs.
    auto blur = ResourceManager::LoadShader("postproc_blur");
    blur->Bind();
    bool horizontal = true;
    bool firstIteration = true;
    const int passes = 6; // 3 H + 3 V
    for (int i = 0; i < passes; ++i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO[horizontal ? 0 : 1]);
        blur->SetBool("horizontal", horizontal);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, firstIteration ? brightColor : bloomColor[horizontal ? 1 : 0]);
        blur->SetInt("image", 0);
        DrawFullscreenQuad();
        horizontal = !horizontal;
        firstIteration = false;
    }

    // Last write went to bloomFBO[!horizontal]; horizontal was flipped after it.
    return bloomColor[horizontal ? 1 : 0];
}

void PostProcessor::Resolve(const glm::mat4& projection)
{
    bool ssaoOn = EngineSettings::IsSSAOEnabled();
    bool bloomOn = EngineSettings::IsBloomEnabled();
    bool fxaaOn = EngineSettings::IsFXAAEnabled();

    unsigned int ssaoTex = 0;
    if (ssaoOn) ssaoTex = RunSSAO(projection);

    unsigned int bloomTex = 0;
    if (bloomOn) bloomTex = RunBloom();

    // --- Composite (HDR -> LDR): scene + bloom + AO, tone map, gamma ---
    // If FXAA is on, render into the LDR intermediate; otherwise straight to
    // the screen.
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);

    auto composite = ResourceManager::LoadShader("postproc_composite");
    glBindFramebuffer(GL_FRAMEBUFFER, fxaaOn ? ldrFBO : 0);
    composite->Bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneColor);
    composite->SetInt("sceneTexture", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomOn ? bloomTex : 0);
    composite->SetInt("bloomTexture", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, ssaoOn ? ssaoTex : 0);
    composite->SetInt("ssaoTexture", 2);

    composite->SetBool("bloomEnabled", bloomOn);
    composite->SetFloat("bloomIntensity", EngineSettings::GetBloomIntensity());
    composite->SetBool("ssaoEnabled", ssaoOn);
    composite->SetBool("tonemapEnabled", EngineSettings::IsTonemapEnabled());
    composite->SetFloat("exposure", EngineSettings::GetExposure());
    DrawFullscreenQuad();

    // --- FXAA: read the LDR intermediate, write to the screen ---
    if (fxaaOn)
    {
        auto fxaa = ResourceManager::LoadShader("postproc_fxaa");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        fxaa->Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ldrColor);
        fxaa->SetInt("image", 0);
        DrawFullscreenQuad();
    }

    // Restore depth testing + default framebuffer for the editor overlays that
    // are drawn on top (gizmos/grid/selection).
    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);

    // The overlays need the scene depth to occlude correctly against geometry;
    // blit the captured depth into the default framebuffer's depth buffer.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
