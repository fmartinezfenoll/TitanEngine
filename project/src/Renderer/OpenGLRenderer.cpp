#include "Renderer/OpenGLRenderer.h"
#include "Renderer/RendererFactory.h"
#include "Renderer/Viewport.h"
#include "Renderer/ShadowFramebuffer.h"
#include "Renderer/ShadowMap.h"
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/OpenGLShader.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/TNode.h"
#include "Scene/CameraComponent.h"
#include "Scene/LightComponent.h"
#include "Scene/MeshComponent.h"
#include "Renderer/GizmoRenderer.h"
#include "Renderer/Skybox.h"
#include "Debug/DebugUI.h"
#include "Debug/ProjectBrowser.h"
#include "Core/Stats.h"
#include "Core/EngineSettings.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>

namespace {
    constexpr unsigned int kShadowTextureUnitBase = 3; // 0-2 reserved by Material (albedo/normal/metallicRoughness)
}

void OpenGLRenderer::Register()
{
    RendererFactory::Instance().RegisterRenderer(
        "opengl",
        []() { return std::make_unique<OpenGLRenderer>(); }
    );
}

bool OpenGLRenderer::Init(int width, int height, const std::string& appName)
{
    // Initialize GLFW
    if (!glfwInit())
    {
        std::cout << "[ERROR] GLFW init failed" << std::endl;
        return false;
    }

    // Set OpenGL version (Core Profile 4.5)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create window
    window = glfwCreateWindow(width, height, appName.c_str(), nullptr, nullptr);
    if (!window)
    {
        std::cout << "[ERROR] Window creation failed" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(static_cast<GLFWwindow*>(window));

    // Enable VSync
    glfwSwapInterval(1);

    // Load OpenGL functions via GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "[ERROR] GLAD init failed" << std::endl;
        glfwTerminate();
        return false;
    }

    // Configure viewport
    glViewport(0, 0, width, height);
    Viewport::Set(width, height);

    // Resize callback
    glfwSetFramebufferSizeCallback(
        static_cast<GLFWwindow*>(window),
        [](GLFWwindow*, int w, int h)
        {
            glViewport(0, 0, w, h);
            Viewport::Set(w, h);
        }
    );

    // External file-drop callback: dropping files from the OS file explorer onto
    // the window queues them for import into the Project browser's current folder.
    glfwSetDropCallback(
        static_cast<GLFWwindow*>(window),
        [](GLFWwindow*, int count, const char** paths)
        {
            ProjectBrowser::EnqueueDroppedPaths(count, paths);
        }
    );

    // Default OpenGL state
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    DebugUI::ApplyTheme();
    ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(window), true);
    ImGui_ImplOpenGL3_Init("#version 450");

    GizmoRenderer::Init();
    Skybox::InitSharedGeometry();

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;

    return true;
}


void OpenGLRenderer::Shutdown()
{
    if (brdfLUTID != 0) glDeleteTextures(1, &brdfLUTID);
    Skybox::ShutdownSharedGeometry();
    GizmoRenderer::Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glfwTerminate();
}

void OpenGLRenderer::Update(float deltaTime)
{
    UpdateCameraInput(deltaTime);
}

void OpenGLRenderer::UpdateCameraInput(float deltaTime)
{
    Scene* activeScene = SceneManager::Instance().GetActiveScene();
    if (!activeScene || !activeScene->GetMainCamera()) return;

    TNode* cameraNode = activeScene->GetMainCamera();
    auto* camera = cameraNode->GetComponent<CameraComponent>();
    if (!camera) return;

    GLFWwindow* win = static_cast<GLFWwindow*>(window);

    // Skip WASD/Space/Ctrl movement while ImGui has keyboard focus (typing in the
    // search filter, a rename field, etc.) -- otherwise those letters also drive
    // the camera underneath the UI.
    if (!ImGui::GetIO().WantCaptureKeyboard) {
        glm::vec3 moveDir(0.0f);
        if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) moveDir.z += 1.0f;
        if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) moveDir.z -= 1.0f;
        if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) moveDir.x += 1.0f;
        if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) moveDir.x -= 1.0f;
        if (glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS) moveDir.y += 1.0f;
        if (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) moveDir.y -= 1.0f;
        camera->ProcessKeyboard(moveDir, deltaTime);
    }

    if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
    {
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        double mouseX, mouseY;
        glfwGetCursorPos(win, &mouseX, &mouseY);

        if (firstMouse)
        {
            lastMouseX = mouseX;
            lastMouseY = mouseY;
            firstMouse = false;
        }

        float xOffset = static_cast<float>(mouseX - lastMouseX);
        float yOffset = static_cast<float>(lastMouseY - mouseY);
        lastMouseX = mouseX;
        lastMouseY = mouseY;

        camera->ProcessMouseLook(xOffset, yOffset);
    }
    else
    {
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        firstMouse = true;
    }
}

void OpenGLRenderer::BeginFrame()
{
    bool vsyncEnabled = EngineSettings::IsVSyncEnabled();
    if (vsyncEnabled != appliedVSync) {
        glfwSwapInterval(vsyncEnabled ? 1 : 0);
        appliedVSync = vsyncEnabled;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    glm::vec3 clearColor(0.1f, 0.1f, 0.15f);
    if (Scene* activeScene = SceneManager::Instance().GetActiveScene()) {
        clearColor = activeScene->GetClearColor();
    }
    glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
void OpenGLRenderer::ReconcileShadowFramebuffers(Scene* activeScene)
{
    const std::vector<TNode*>& currentLights = activeScene->GetLights();

    for (auto it = shadowFramebuffers.begin(); it != shadowFramebuffers.end();) {
        bool stillPresent = std::find(currentLights.begin(), currentLights.end(), it->first) != currentLights.end();
        if (!stillPresent) {
            it = shadowFramebuffers.erase(it);
        } else {
            ++it;
        }
    }

    for (TNode* lightNode : currentLights) {
        auto* light = lightNode->GetComponent<LightComponent>();
        if (!light || !light->castsShadow) continue;

        bool isCubemap = (light->type == LightType::Point);
        int resolution = isCubemap ? EngineSettings::GetShadowResolutionCube()
                                    : EngineSettings::GetShadowResolution2D();

        auto it = shadowFramebuffers.find(lightNode);
        if (it != shadowFramebuffers.end() && it->second->GetResolution() == resolution) continue;

        shadowFramebuffers[lightNode] = std::make_unique<ShadowFramebuffer>(resolution, isCubemap);
    }
}

void OpenGLRenderer::EnsureBRDFLUTGenerated()
{
    if (brdfLUTID != 0) return;

    constexpr int kBRDFLUTResolution = 512;

    glGenTextures(1, &brdfLUTID);
    glBindTexture(GL_TEXTURE_2D, brdfLUTID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, kBRDFLUTResolution, kBRDFLUTResolution, 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    unsigned int fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTID, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "[ERROR] BRDF LUT framebuffer incomplete" << std::endl;

    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    glViewport(0, 0, kBRDFLUTResolution, kBRDFLUTResolution);

    auto shader = ResourceManager::LoadShader("ibl_brdf_lut");
    if (shader) {
        shader->Bind();

        unsigned int emptyVAO = 0;
        glGenVertexArrays(1, &emptyVAO);
        glBindVertexArray(emptyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
        glDeleteVertexArrays(1, &emptyVAO);
    }

    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
}

std::vector<ShadowMapData> OpenGLRenderer::RenderShadowPass(Scene* activeScene, const glm::vec3& cameraWorldPos)
{
    std::vector<ShadowMapData> shadowMapData;

    ReconcileShadowFramebuffers(activeScene);

    TNode* root = activeScene->GetRoot();
    if (!root) return shadowMapData;

    int spotCount = 0, pointCount = 0;
    bool directionalDone = false;

    glCullFace(GL_FRONT);

    for (TNode* lightNode : activeScene->GetLights()) {
        auto* light = lightNode->GetComponent<LightComponent>();
        if (!light || !light->castsShadow) continue;
        if (light->type == LightType::Directional && directionalDone) continue;
        if (light->type == LightType::Spot && spotCount >= 2) continue;
        if (light->type == LightType::Point && pointCount >= 2) continue;

        auto fbIt = shadowFramebuffers.find(lightNode);
        if (fbIt == shadowFramebuffers.end()) continue;
        ShadowFramebuffer* fb = fbIt->second.get();

        fb->BindForWriting();
        glClear(GL_DEPTH_BUFFER_BIT);

        if (light->type == LightType::Point) {
            auto shader = ResourceManager::LoadShader("shadow_depth_cubemap", true);
            shader->Bind();
            auto matrices = light->GetCubemapViewProjections();
            for (int i = 0; i < 6; ++i)
                shader->SetMat4("shadowMatrices[" + std::to_string(i) + "]", matrices[i]);
            shader->SetVec3("lightPos", light->GetPosition());
            shader->SetFloat("farPlane", light->range);
            root->drawDepthOnly(shader.get());

            ShadowMapData data;
            data.lightNode = lightNode;
            data.lightType = static_cast<int>(light->type);
            data.textureId = fb->GetShadowMap().GetID();
            data.isCubemap = true;
            data.lightSpaceMatrix = glm::mat4(1.0f);
            data.lightPos = light->GetPosition();
            data.farPlane = light->range;
            shadowMapData.push_back(data);
        } else {
            auto shader = ResourceManager::LoadShader("shadow_depth");
            shader->Bind();
            glm::vec3 focusPoint = (light->type == LightType::Directional) ? cameraWorldPos : glm::vec3(0.0f);
            glm::mat4 lightSpaceMatrix = light->GetLightSpaceMatrix(focusPoint);
            shader->SetMat4("lightSpaceMatrix", lightSpaceMatrix);
            root->drawDepthOnly(shader.get());

            ShadowMapData data;
            data.lightNode = lightNode;
            data.lightType = static_cast<int>(light->type);
            data.textureId = fb->GetShadowMap().GetID();
            data.isCubemap = false;
            data.lightSpaceMatrix = lightSpaceMatrix;
            data.lightPos = light->GetPosition();
            data.farPlane = light->range;
            shadowMapData.push_back(data);
        }

        if (light->type == LightType::Spot) ++spotCount;
        if (light->type == LightType::Point) ++pointCount;
        if (light->type == LightType::Directional) directionalDone = true;
    }

    glCullFace(GL_BACK);
    ShadowFramebuffer::UnbindToScreen();
    glViewport(0, 0, Viewport::GetWidth(), Viewport::GetHeight());

    return shadowMapData;
}

void OpenGLRenderer::Render()
{
    Stats::BeginFrame();

    Scene* activeScene = SceneManager::Instance().GetActiveScene();
    if (activeScene) {
        glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
        glm::mat4 projection = glm::perspective(glm::radians(60.0f), Viewport::GetAspectRatio(), 0.1f, 100.0f);

        glm::vec3 cameraWorldPos(0.0f);
        if (TNode* cameraNode = activeScene->GetMainCamera()) {
            if (auto* camera = cameraNode->GetComponent<CameraComponent>()) {
                view = camera->GetViewMatrix();
                projection = camera->GetProjectionMatrix(Viewport::GetAspectRatio());
                cameraWorldPos = cameraNode->getGlobalPosition();
            }
        }

        std::vector<ShadowMapData> shadowMapData;
        if (EngineSettings::AreShadowsEnabled()) {
            shadowMapData = RenderShadowPass(activeScene, cameraWorldPos);
        }

        std::vector<LightUniformData> lights;
        for (TNode* lightNode : activeScene->GetLights()) {
            if (auto* light = lightNode->GetComponent<LightComponent>()) {
                LightUniformData data;
                data.type = static_cast<int>(light->type);
                data.position = light->GetPosition();
                data.direction = light->GetDirection();
                data.color = light->color;
                data.intensity = light->intensity;
                data.range = light->range;
                data.innerCutoff = glm::cos(glm::radians(light->innerConeDegrees));
                data.outerCutoff = glm::cos(glm::radians(light->outerConeDegrees));
                data.shadowIndex = -1;
                lights.push_back(data);
            }
        }

        // Resolve shadowIndex per-type and build ShadowRenderData for the main pass.
        ShadowRenderData shadowRenderData;
        {
            int spotSlot = 0, pointSlot = 0;
            unsigned int nextUnit = kShadowTextureUnitBase;

            size_t lightIdx = 0;
            for (TNode* lightNode : activeScene->GetLights()) {
                auto* light = lightNode->GetComponent<LightComponent>();
                if (!light) continue;
                if (lightIdx >= lights.size()) break;

                if (light->castsShadow) {
                    for (const ShadowMapData& sm : shadowMapData) {
                        if (sm.lightNode != lightNode)
                            continue;

                        if (light->type == LightType::Directional) {
                            lights[lightIdx].shadowIndex = 0;
                            shadowRenderData.hasDirectional = true;
                            shadowRenderData.directionalSlot = nextUnit;
                            shadowRenderData.directionalLightSpaceMatrix = sm.lightSpaceMatrix;
                            glActiveTexture(GL_TEXTURE0 + nextUnit);
                            glBindTexture(GL_TEXTURE_2D, sm.textureId);
                            ++nextUnit;
                        } else if (light->type == LightType::Spot && spotSlot < 2) {
                            lights[lightIdx].shadowIndex = spotSlot;
                            shadowRenderData.spotSlots[spotSlot] = nextUnit;
                            shadowRenderData.spotLightSpaceMatrices[spotSlot] = sm.lightSpaceMatrix;
                            glActiveTexture(GL_TEXTURE0 + nextUnit);
                            glBindTexture(GL_TEXTURE_2D, sm.textureId);
                            ++nextUnit;
                            ++spotSlot;
                            shadowRenderData.spotCount = spotSlot;
                        } else if (light->type == LightType::Point && pointSlot < 2) {
                            lights[lightIdx].shadowIndex = pointSlot;
                            shadowRenderData.pointSlots[pointSlot] = nextUnit;
                            shadowRenderData.pointLightPos[pointSlot] = sm.lightPos;
                            shadowRenderData.pointFarPlane[pointSlot] = sm.farPlane;
                            glActiveTexture(GL_TEXTURE0 + nextUnit);
                            glBindTexture(GL_TEXTURE_CUBE_MAP, sm.textureId);
                            ++nextUnit;
                            ++pointSlot;
                            shadowRenderData.pointCount = pointSlot;
                        }
                        break;
                    }
                }
                ++lightIdx;
            }
        }

        Frustum frustum;
        frustum.updateFromCamera(projection * view);

        Skybox* skybox = activeScene->GetSkybox();

        IBLRenderData iblRenderData;
        if (skybox && EngineSettings::IsIBLEnabled()) {
            EnsureBRDFLUTGenerated();
            skybox->EnsureIBLGenerated();

            if (skybox->IsIBLGenerated()) {
                iblRenderData.hasIBL = true;

                glActiveTexture(GL_TEXTURE0 + iblRenderData.irradianceSlot);
                glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->GetIrradianceMap());

                glActiveTexture(GL_TEXTURE0 + iblRenderData.prefilterSlot);
                glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->GetPrefilterMap());

                glActiveTexture(GL_TEXTURE0 + iblRenderData.brdfLUTSlot);
                glBindTexture(GL_TEXTURE_2D, brdfLUTID);
            }
        }

        if (skybox) {
            skybox->Draw(view, projection);
        }

        DrawGrid(activeScene, view, projection);

        bool wireframe = EngineSettings::IsWireframeEnabled();
        if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        TNode* root = activeScene->GetRoot();
        if (root) {
            activeScene->Draw(frustum, view, projection, cameraWorldPos, lights, shadowRenderData, iblRenderData);
        }

        if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        DrawGizmos(activeScene, view, projection);
        DrawSelectionHighlight(activeScene, view, projection);
        DrawTransformGizmo(activeScene, view, projection);
    }
}

void OpenGLRenderer::DrawGrid(Scene* activeScene, const glm::mat4& view, const glm::mat4& projection)
{
    if (!activeScene->IsGridVisible()) return;
    GizmoRenderer::DrawGrid(view, projection);
}

void OpenGLRenderer::DrawGizmos(Scene* activeScene, const glm::mat4& view, const glm::mat4& projection)
{
    for (TNode* lightNode : activeScene->GetLights()) {
        if (auto* light = lightNode->GetComponent<LightComponent>()) {
            GizmoRenderer::DrawLightGizmo(light->GetPosition(), light->color, view, projection);
        }
    }

    TNode* mainCameraNode = activeScene->GetMainCamera();
    for (TNode* cameraNode : activeScene->GetCameras()) {
        if (cameraNode == mainCameraNode) continue;
        GizmoRenderer::DrawCameraGizmo(cameraNode->getModelMatrix(), view, projection);
    }
}

void OpenGLRenderer::DrawSelectionHighlight(Scene* activeScene, const glm::mat4& view, const glm::mat4& projection)
{
    TNode* selected = DebugUI::GetSelectedNode();
    if (!selected) return;

    if (auto* mesh = selected->GetComponent<MeshComponent>()) {
        glm::vec3 localMin, localMax;
        mesh->GetLocalBounds(localMin, localMax);

        glm::mat4 model = selected->getModelMatrix();
        glm::vec3 worldMin(std::numeric_limits<float>::max());
        glm::vec3 worldMax(-std::numeric_limits<float>::max());
        for (int i = 0; i < 8; ++i) {
            glm::vec3 corner(
                (i & 1) ? localMax.x : localMin.x,
                (i & 2) ? localMax.y : localMin.y,
                (i & 4) ? localMax.z : localMin.z);
            glm::vec3 worldCorner = glm::vec3(model * glm::vec4(corner, 1.0f));
            worldMin = glm::min(worldMin, worldCorner);
            worldMax = glm::max(worldMax, worldCorner);
        }

        glm::vec3 center = (worldMin + worldMax) * 0.5f;
        glm::vec3 extents = (worldMax - worldMin) * 0.5f;
        GizmoRenderer::DrawSelectionBox(center, extents, view, projection);
        return;
    }

    if (auto* light = selected->GetComponent<LightComponent>()) {
        GizmoRenderer::DrawSelectionBox(light->GetPosition(),
            glm::vec3(GizmoRenderer::kLightGizmoRadius * 1.3f), view, projection);
        return;
    }

    if (selected->GetComponent<CameraComponent>()) {
        GizmoRenderer::DrawSelectionBox(selected->getGlobalPosition(),
            glm::vec3(GizmoRenderer::kCameraGizmoRadius * 1.3f), view, projection);
    }
}

void OpenGLRenderer::DrawTransformGizmo(Scene* activeScene, const glm::mat4& view, const glm::mat4& projection)
{
    TNode* selected = DebugUI::GetSelectedNode();
    if (!selected) return;

    glm::vec3 cameraWorldPos(0.0f, 0.0f, 3.0f);
    if (TNode* cameraNode = activeScene->GetMainCamera()) {
        cameraWorldPos = cameraNode->transform.position;
    }

    glm::vec3 worldPos = selected->getGlobalPosition();
    glm::mat4 baseRotation = glm::mat4(1.0f);
    if (DebugUI::GetGizmoSpace() == GizmoSpace::Local) {
        baseRotation = selected->getModelMatrix();
        baseRotation[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        glm::vec3 scaleCol0 = glm::vec3(baseRotation[0]);
        glm::vec3 scaleCol1 = glm::vec3(baseRotation[1]);
        glm::vec3 scaleCol2 = glm::vec3(baseRotation[2]);
        if (glm::length(scaleCol0) > 1e-6f) baseRotation[0] /= glm::length(scaleCol0);
        if (glm::length(scaleCol1) > 1e-6f) baseRotation[1] /= glm::length(scaleCol1);
        if (glm::length(scaleCol2) > 1e-6f) baseRotation[2] /= glm::length(scaleCol2);
    }

    float scale = GizmoRenderer::ComputeGizmoScale(worldPos, cameraWorldPos);

    glDisable(GL_DEPTH_TEST);
    switch (DebugUI::GetGizmoMode()) {
        case GizmoMode::Move:
            GizmoRenderer::DrawMoveGizmo(worldPos, baseRotation, scale, view, projection);
            break;
        case GizmoMode::Rotate:
            GizmoRenderer::DrawRotateGizmo(worldPos, baseRotation, scale, view, projection);
            break;
        case GizmoMode::Scale:
            GizmoRenderer::DrawScaleGizmo(worldPos, baseRotation, scale, view, projection);
            break;
    }
    glEnable(GL_DEPTH_TEST);
}

void OpenGLRenderer::EndFrame()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(static_cast<GLFWwindow*>(window));
}

void* OpenGLRenderer::GetWindow() const
{
    return window;
}
void OpenGLRenderer::PollEvents()
{
    glfwPollEvents();
}
bool OpenGLRenderer::ShouldClose() const
{
    return glfwWindowShouldClose(static_cast<GLFWwindow*>(window));
}

namespace
{
    const bool registered = []()
    {
        OpenGLRenderer::Register();
        return true;
    }();
}
