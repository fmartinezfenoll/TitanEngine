#include "Renderer/OpenGLRenderer.h"
#include "Renderer/RendererFactory.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

void OpenGLRenderer::Register()
{
    RendererFactory::Instance().RegisterRenderer(
        "opengl",
        []() { return std::make_unique<OpenGLRenderer>(); }
    );
}

bool OpenGLRenderer::Init(int width, int height, const std::string& appName)
{
    if (!glfwInit())
    {
        std::cout << "[ERROR] GLFW init failed" << std::endl;
        return false;
    }

    window = glfwCreateWindow(width, height, appName.c_str(), nullptr, nullptr);

    if (!window)
    {
        std::cout << "[ERROR] Window creation failed" << std::endl;
        return false;
    }

    glfwMakeContextCurrent(static_cast<GLFWwindow*>(window));

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "[ERROR] GLAD init failed" << std::endl;
        return false;
    }

    glEnable(GL_DEPTH_TEST);

    return true;
}

void OpenGLRenderer::Shutdown()
{
    glfwTerminate();
}

void OpenGLRenderer::Render()
{
    GLFWwindow* glfwWindow = static_cast<GLFWwindow*>(window);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glfwSwapBuffers(glfwWindow);
    glfwPollEvents();
}

void OpenGLRenderer::ImGuiNewFrame()
{
    // Future ImGui integration
}

void* OpenGLRenderer::GetWindow() const
{
    return window;
}

namespace
{
    const bool registered = []()
    {
        OpenGLRenderer::Register();
        return true;
    }();
}
