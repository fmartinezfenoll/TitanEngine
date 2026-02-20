#include "Renderer/Renderer.h"
#include "Renderer/RendererFactory.h"
#include "Renderer/IRenderer.h" 
#include <iostream>

std::unique_ptr<IRenderer> Renderer::s_Renderer;

bool Renderer::Init(int width, int height, const std::string& name)
{
    s_Renderer = RendererFactory::Instance().Create("opengl");

    if (!s_Renderer)
    {
        std::cout << "[ERROR] Renderer creation failed\n";
        return false;
    }

    return s_Renderer->Init(width, height, name);
}

void Renderer::Render()
{
    s_Renderer->Render();
}

void Renderer::Shutdown()
{
    s_Renderer->Shutdown();
}
