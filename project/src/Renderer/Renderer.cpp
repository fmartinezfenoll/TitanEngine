#include "Renderer/Renderer.h"
#include "Renderer/RendererFactory.h"
#include "Renderer/IRenderer.h" 
#include <iostream>

std::unique_ptr<IRenderer> Renderer::Instance;

bool Renderer::Init(int width, int height, const std::string& name)
{
    Instance = RendererFactory::Instance().Create("opengl");

    if (!Instance)
    {
        std::cout << "[ERROR] Renderer creation failed\n";
        return false;
    }

    return Instance->Init(width, height, name);
}

void Renderer::Render()
{
    Instance->Render();
}

void Renderer::Shutdown()
{
    Instance->Shutdown();
}
