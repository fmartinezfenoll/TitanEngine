#pragma once
#include <memory>
#include "Core/Config.h"

class IRenderer;

class Application
{
public:
    Application(const AppConfig& config = AppConfig());
    ~Application();

    bool Init();
    void Run();
    void Update(float deltaTime);
    void Shutdown();

private:
    AppConfig m_config;
    std::unique_ptr<IRenderer> m_renderer;
};
