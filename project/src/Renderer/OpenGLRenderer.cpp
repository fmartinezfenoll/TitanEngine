#include "Renderer/OpenGLRenderer.h"
#include "Renderer/RendererFactory.h"
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/OpenGLShader.h"

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

    // Resize callback
    glfwSetFramebufferSizeCallback(
        static_cast<GLFWwindow*>(window),
        [](GLFWwindow*, int w, int h)
        {
            glViewport(0, 0, w, h);
        }
    );

    // Default OpenGL state
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    glGenVertexArrays(1, &m_VAO);

    // Debug info
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;

    return true;
}


void OpenGLRenderer::Shutdown()
{
    glfwTerminate();
}

void OpenGLRenderer::BeginFrame()
{
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}
void OpenGLRenderer::Render()
{
    auto shader = ResourceManager::LoadShader("basic");
    if (!shader) return;

    shader->Bind();

    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
void OpenGLRenderer::EndFrame()
{
    glfwSwapBuffers(static_cast<GLFWwindow*>(window));
}
void OpenGLRenderer::ImGuiNewFrame()
{
    // Future ImGui integration
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
