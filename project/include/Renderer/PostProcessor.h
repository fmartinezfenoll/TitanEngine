#pragma once

#include <glm/glm.hpp>

// Owns the HDR framebuffer the scene is rendered into, plus the ping-pong and
// effect framebuffers, and runs the post-processing chain (SSAO -> bloom ->
// tone map -> FXAA). Encapsulated like Skybox so OpenGLRenderer stays lean.
//
// Usage per frame (only when EngineSettings::IsPostProcessEnabled()):
//   1. BeginSceneCapture()  -> binds the HDR FBO; caller clears + draws the scene
//   2. (caller draws scene 3D only; NOT gizmos/grid/UI)
//   3. Resolve(view, projection) -> runs the chain, leaves the result in the
//      default framebuffer, ready for the editor to draw gizmos on top.
class PostProcessor
{
public:
    PostProcessor() = default;
    ~PostProcessor();

    PostProcessor(const PostProcessor&) = delete;
    PostProcessor& operator=(const PostProcessor&) = delete;

    // Lazily (re)allocates all framebuffers for the given size. Safe to call
    // every frame; only rebuilds when the size actually changes.
    void EnsureResources(int width, int height);

    // Binds the HDR scene framebuffer and sets the viewport. The caller then
    // clears and renders the 3D scene into it.
    void BeginSceneCapture();

    // Runs the enabled post-process passes and writes the final LDR image to the
    // default framebuffer (screen). `projection` is needed for SSAO's view-space
    // reconstruction.
    void Resolve(const glm::mat4& projection);

    // Frees all GL resources. Called on shutdown / context loss.
    void Cleanup();

    static void InitSharedGeometry();
    static void ShutdownSharedGeometry();

private:
    int width = 0;
    int height = 0;

    // HDR scene target (color RGBA16F + depth). Depth is a texture so SSAO can
    // sample it.
    unsigned int sceneFBO = 0;
    unsigned int sceneColor = 0;
    unsigned int sceneDepth = 0;

    // Ping-pong pair for the separable bloom blur (half res).
    unsigned int bloomFBO[2] = {0, 0};
    unsigned int bloomColor[2] = {0, 0};
    int bloomWidth = 0;
    int bloomHeight = 0;

    // Bright-pass extraction target (half res, feeds the blur).
    unsigned int brightFBO = 0;
    unsigned int brightColor = 0;

    // SSAO raw + blurred (single channel R).
    unsigned int ssaoFBO = 0;
    unsigned int ssaoColor = 0;
    unsigned int ssaoBlurFBO = 0;
    unsigned int ssaoBlurColor = 0;

    // Intermediate LDR target after composite, so FXAA has something to read.
    unsigned int ldrFBO = 0;
    unsigned int ldrColor = 0;

    void CreateFramebuffers();
    void DestroyFramebuffers();

    unsigned int RunBloom();          // returns the blurred bloom texture id
    unsigned int RunSSAO(const glm::mat4& projection); // returns AO texture id

    static void DrawFullscreenQuad();

    static inline unsigned int quadVAO = 0;
    static inline unsigned int quadVBO = 0;
};
