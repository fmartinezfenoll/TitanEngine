#pragma once
#include <string>

enum class RendererAPI
{
    OpenGL,
    Vulkan
};

struct AppConfig
{
    int Width = 1920;
    int Height = 1080;
    std::string AppName = "TitanEngine";

    RendererAPI API = RendererAPI::OpenGL;
};
