#pragma once

#include "Renderer/IRenderer.h"
#include <string>

class Scene;

class OpenGLRenderer : public IRenderer
{
public:
    OpenGLRenderer() = default;
    ~OpenGLRenderer() override = default;

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
    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;

    bool m_firstMouse = true;
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;

    void UpdateCameraInput(float deltaTime);
    void DrawGizmos(Scene* activeScene);
    void DrawSelectionHighlight(Scene* activeScene);
    void DrawTransformGizmo(Scene* activeScene);
};
