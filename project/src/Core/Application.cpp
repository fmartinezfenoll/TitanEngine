#include "Core/Application.h"
#include "Core/Time.h"
#include "Renderer/RendererFactory.h"
#include "Renderer/IRenderer.h"
#include <iostream>
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Debug/DebugUI.h"
#include "Core/Stats.h"
#include "Core/EngineConfig.h"

// ==============================
// Basic functions
// ==============================
Application::Application(const AppConfig& appConfig)
    : config(appConfig)
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
    switch (config.API)
    {
        case RendererAPI::OpenGL:
            renderer = RendererFactory::Instance().Create("opengl");
            break;

        case RendererAPI::Vulkan:
            renderer = RendererFactory::Instance().Create("vulkan");
            break;
    }

    if (!renderer)
    {
        std::cout << "[ERROR] Renderer creation failed\n";
        return false;
    }

    if (!renderer->Init(config.Width, config.Height, config.AppName))
    {
        std::cout << "[ERROR] Renderer initialization failed\n";
        return false;
    }

    EngineConfig::Load();

    SetupScenes();

    DebugUI::Init();

    return true;
}

void Application::Run()
{
    double lastTime = Time::GetTime();

    while (!renderer->ShouldClose())
    {
        double currentTime = Time::GetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        renderer->PollEvents();
        renderer->Update(deltaTime);

        Update(deltaTime);
        Stats::Tick(deltaTime);

        renderer->BeginFrame();
        renderer->Render();
        OnImGui();
        renderer->EndFrame();
    }
}
void Application::Update(float deltaTime)
{
    if (Scene* active = SceneManager::Instance().GetActiveScene()) {
        active->Update(deltaTime);
    }
}

SceneManager& Application::GetSceneManager()
{
    return SceneManager::Instance();
}

void Application::OnImGui()
{
    DebugUI::DrawFrame(&SceneManager::Instance());
}

void Application::Shutdown()
{
    EngineConfig::Save();
    DebugUI::Shutdown();
    SceneManager::Instance().UnloadAllScenes();
    if (renderer)
        renderer->Shutdown();
}
