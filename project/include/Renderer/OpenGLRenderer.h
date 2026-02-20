#pragma once

#include "Renderer/IRenderer.h"
#include <string>

class OpenGLRenderer : public IRenderer
{
public:
    OpenGLRenderer() = default;
    ~OpenGLRenderer() override = default;

    static void Register(); // Explicit backend registration

    bool Init(int width, int height, const std::string& appName) override;
    void Shutdown() override;
    void Render() override;

    void ImGuiNewFrame() override;
    void* GetWindow() const override;

    bool ShouldClose() const;

private:
    void* window = nullptr; // Stored as void* to avoid exposing GLFW in header
};
