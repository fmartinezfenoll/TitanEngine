#include "Renderer/OpenGLRenderer.h"
#include "Renderer/RendererFactory.h"
#include "Renderer/Viewport.h"
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/OpenGLShader.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/TNode.h"
#include "Scene/CameraComponent.h"
#include "Scene/LightComponent.h"
#include "Scene/MeshComponent.h"
#include "Renderer/GizmoRenderer.h"
#include "Debug/DebugUI.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <limits>

// ImGui backend function declarations (headers not available, declared from backends/*.cpp)
extern bool ImGui_ImplGlfw_InitForOpenGL(GLFWwindow* window, bool install_callbacks);
extern void ImGui_ImplGlfw_Shutdown();
extern void ImGui_ImplGlfw_NewFrame();
extern bool ImGui_ImplOpenGL3_Init(const char* glsl_version = nullptr);
extern void ImGui_ImplOpenGL3_Shutdown();
extern void ImGui_ImplOpenGL3_NewFrame();
extern void ImGui_ImplOpenGL3_RenderDrawData(ImDrawData* draw_data);

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

    // Default OpenGL state
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(window), true);
    ImGui_ImplOpenGL3_Init("#version 450");

    GizmoRenderer::Init();

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;

    return true;
}


void OpenGLRenderer::Shutdown()
{
    GizmoRenderer::Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
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

    glm::vec3 moveDir(0.0f);
    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) moveDir.z += 1.0f;
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) moveDir.z -= 1.0f;
    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) moveDir.x += 1.0f;
    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) moveDir.x -= 1.0f;
    if (glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS) moveDir.y += 1.0f;
    if (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) moveDir.y -= 1.0f;
    camera->ProcessKeyboard(moveDir, deltaTime);

    if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
    {
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        double mouseX, mouseY;
        glfwGetCursorPos(win, &mouseX, &mouseY);

        if (m_firstMouse)
        {
            m_lastMouseX = mouseX;
            m_lastMouseY = mouseY;
            m_firstMouse = false;
        }

        float xOffset = static_cast<float>(mouseX - m_lastMouseX);
        float yOffset = static_cast<float>(m_lastMouseY - mouseY);
        m_lastMouseX = mouseX;
        m_lastMouseY = mouseY;

        camera->ProcessMouseLook(xOffset, yOffset);
    }
    else
    {
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        m_firstMouse = true;
    }
}

void OpenGLRenderer::BeginFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
void OpenGLRenderer::Render()
{
    Scene* activeScene = SceneManager::Instance().GetActiveScene();
    if (activeScene) {
        Frustum frustum;
        TNode* root = activeScene->GetRoot();
        if (root) {
            activeScene->Draw(frustum);
        }
        DrawGizmos(activeScene);
        DrawSelectionHighlight(activeScene);
        DrawTransformGizmo(activeScene);
    }
}

void OpenGLRenderer::DrawGizmos(Scene* activeScene)
{
    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
    glm::mat4 projection = glm::perspective(glm::radians(60.0f), Viewport::GetAspectRatio(), 0.1f, 100.0f);

    if (TNode* cameraNode = activeScene->GetMainCamera()) {
        if (auto* camera = cameraNode->GetComponent<CameraComponent>()) {
            view = camera->GetViewMatrix();
            projection = camera->GetProjectionMatrix(Viewport::GetAspectRatio());
        }
    }

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

void OpenGLRenderer::DrawSelectionHighlight(Scene* activeScene)
{
    TNode* selected = DebugUI::GetSelectedNode();
    if (!selected) return;

    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
    glm::mat4 projection = glm::perspective(glm::radians(60.0f), Viewport::GetAspectRatio(), 0.1f, 100.0f);
    if (TNode* cameraNode = activeScene->GetMainCamera()) {
        if (auto* camera = cameraNode->GetComponent<CameraComponent>()) {
            view = camera->GetViewMatrix();
            projection = camera->GetProjectionMatrix(Viewport::GetAspectRatio());
        }
    }

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

void OpenGLRenderer::DrawTransformGizmo(Scene* activeScene)
{
    TNode* selected = DebugUI::GetSelectedNode();
    if (!selected) return;

    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
    glm::mat4 projection = glm::perspective(glm::radians(60.0f), Viewport::GetAspectRatio(), 0.1f, 100.0f);
    glm::vec3 cameraWorldPos(0.0f, 0.0f, 3.0f);
    if (TNode* cameraNode = activeScene->GetMainCamera()) {
        if (auto* camera = cameraNode->GetComponent<CameraComponent>()) {
            view = camera->GetViewMatrix();
            projection = camera->GetProjectionMatrix(Viewport::GetAspectRatio());
            cameraWorldPos = cameraNode->transform.position;
        }
    }

    glm::vec3 worldPos = selected->getGlobalPosition();
    glm::mat4 baseRotation = selected->getModelMatrix();
    baseRotation[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    glm::vec3 scaleCol0 = glm::vec3(baseRotation[0]);
    glm::vec3 scaleCol1 = glm::vec3(baseRotation[1]);
    glm::vec3 scaleCol2 = glm::vec3(baseRotation[2]);
    if (glm::length(scaleCol0) > 1e-6f) baseRotation[0] /= glm::length(scaleCol0);
    if (glm::length(scaleCol1) > 1e-6f) baseRotation[1] /= glm::length(scaleCol1);
    if (glm::length(scaleCol2) > 1e-6f) baseRotation[2] /= glm::length(scaleCol2);

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
