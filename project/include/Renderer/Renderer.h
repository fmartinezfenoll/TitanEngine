#pragma once
#include <memory>

class IRenderer;

class Renderer
{
public:
    static bool Init(int width, int height, const std::string& name);
    static void Render();
    static void Shutdown();

private:
    static std::unique_ptr<IRenderer> s_Renderer;
};
