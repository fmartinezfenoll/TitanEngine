#include "Core/Application.h"
#include "Core/Time.h"
#include "Renderer/RendererFactory.h"
#include "Renderer/IRenderer.h"
#include <iostream>
#include "ResourceManager/ResourceManager.h"

// ==============================
// Basic functions
// ==============================
Application::Application(const AppConfig& config)
    : m_config(config)
{
}

Application::~Application()
{
    Shutdown();
}

// ==============================
// Main Loop
// ==============================
bool Application::Init()
{
    switch (m_config.API)
    {
        case RendererAPI::OpenGL:
            m_renderer = RendererFactory::Instance().Create("opengl");
            break;

        case RendererAPI::Vulkan:
            m_renderer = RendererFactory::Instance().Create("vulkan");
            break;
    }

    if (!m_renderer)
    {
        std::cout << "[ERROR] Renderer creation failed\n";
        return false;
    }

    if (!m_renderer->Init(m_config.Width, m_config.Height, m_config.AppName))
    {
        std::cout << "[ERROR] Renderer initialization failed\n";
        return false;
    }

    return true;
}

void Application::Run()
{
    double lastTime = Time::GetTime();

    while (!m_renderer->ShouldClose())
    {
        double currentTime = Time::GetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        m_renderer->PollEvents();

        Update(deltaTime);

        m_renderer->BeginFrame();
        m_renderer->Render();
        m_renderer->EndFrame();
    }
}
void Application::Update(float deltaTime)
{
    
}


void Application::Shutdown()
{
    if (m_renderer)
        m_renderer->Shutdown();
}


