#include "Renderer/RendererFactory.h"
#include "Renderer/IRenderer.h"

#include <iostream>
#include <stdexcept>

RendererFactory& RendererFactory::Instance()
{
    static RendererFactory instance;
    return instance;
}

void RendererFactory::RegisterRenderer(const std::string& name, Creator creator)
{
    creators[name] = creator;
}

std::unique_ptr<IRenderer> RendererFactory::Create(const std::string& name)
{
    auto it = creators.find(name);

    if (it == creators.end())
    {
        std::cout << "[ERROR] Renderer not registered: " << name << std::endl;
        throw std::runtime_error("Renderer not registered: " + name);
    }

    return it->second();
}
