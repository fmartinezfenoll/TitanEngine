#include "Core/Application.h"
#include "Renderer/RendererFactory.h"
#include "Renderer/IRenderer.h"

#include <iostream>


Application::Application() = default;
Application::~Application() = default;

void Application::Run()
{
    m_renderer = RendererFactory::Instance().Create("opengl");

    if (!m_renderer->Init(1920, 1080, "Engine"))
    {
        std::cout << "[ERROR] Renderer init failed" << std::endl;
        return;
    }

    while (true)
    {
        m_renderer->Render();
    }

    m_renderer->Shutdown();
}
