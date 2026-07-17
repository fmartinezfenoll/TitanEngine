#pragma once

#include "Renderer/IRenderer.h"
#include "Scene/LightUniformData.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

class Scene;
class TNode;
class ShadowFramebuffer;
class PostProcessor;

struct ShadowMapData {
    TNode* lightNode;
    int lightType;
    unsigned int textureId;
    bool isCubemap;
    glm::mat4 lightSpaceMatrix;
    glm::vec3 lightPos;
    float farPlane;
};

class OpenGLRenderer : public IRenderer
{
public:
    OpenGLRenderer() = default;
    // Defined in the .cpp because the unique_ptr<PostProcessor> member needs the
    // complete type at the destructor's definition point.
    ~OpenGLRenderer() override;

    static void Register(); // Explicit backend registration

    bool Init(int width, int height, const std::string& appName) override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    void BeginFrame();
    void Render() override;

    void EndFrame();

    void* GetWindow() const override;

    void PollEvents() override;

    bool ShouldClose() const;

private:
    void* window = nullptr; // Stored as void* to avoid exposing GLFW in header
    unsigned int VAO = 0;
    unsigned int VBO = 0;

    bool firstMouse = true;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;

    bool appliedVSync = true;

    unsigned int brdfLUTID = 0;

    std::unique_ptr<PostProcessor> postProcessor;

    std::unordered_map<TNode*, std::unique_ptr<ShadowFramebuffer>> shadowFramebuffers;

    void UpdateCameraInput(float deltaTime);
    void DrawGrid(Scene* activeScene, const glm::mat4& view, const glm::mat4& projection);
    void DrawGizmos(Scene* activeScene, const glm::mat4& view, const glm::mat4& projection);
    void DrawSelectionHighlight(Scene* activeScene, const glm::mat4& view, const glm::mat4& projection);
    void DrawTransformGizmo(Scene* activeScene, const glm::mat4& view, const glm::mat4& projection);

    std::vector<ShadowMapData> RenderShadowPass(Scene* activeScene, const glm::vec3& cameraWorldPos);
    void ReconcileShadowFramebuffers(Scene* activeScene);
    void EnsureBRDFLUTGenerated();

    // Exports the three split-sum IBL textures of the active scene's skybox
    // (irradiance + prefilter cubemap front faces, and the BRDF LUT) to PNG
    // files under resources/ibl_debug/, for documentation figures. Bound to a
    // key in the editor. No-op if there's no skybox/IBL generated.
    void ExportIBLTextures();
};
