#include "Core/Application.h"
#include "Core/Time.h"
#include "Renderer/RendererFactory.h"
#include "Renderer/IRenderer.h"
#include <iostream>
#include "ResourceManager/ResourceManager.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/SimpleEntities.h"
#include "Scene/SceneSerializer.h"
#include "Debug/DebugUI.h"
#include <glm/glm.hpp>
#include <filesystem>

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

    SetupScenes();

    DebugUI::Init();

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
        OnImGui();
        m_renderer->EndFrame();
    }
}
void Application::Update(float deltaTime)
{
}

SceneManager& Application::GetSceneManager()
{
    return SceneManager::Instance();
}

void Application::OnImGui()
{
    DebugUI::DrawFrame(&SceneManager::Instance());
}

void Application::SetupScenes()
{
    SceneManager& sm = SceneManager::Instance();

    std::filesystem::path scenesDir("scenes");
    if (std::filesystem::exists(scenesDir) && !std::filesystem::is_empty(scenesDir)) {
        sm.LoadAllScenesFromDirectory("scenes");
    } else {
        std::filesystem::create_directories("scenes");

        Scene* triangleScene = sm.CreateScene("Triangle Scene");
        TNode* triangleNode = new TNode(new TriangleEntity(), nullptr, "Triangle");
        triangleNode->transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
        triangleScene->AddNodeToRoot(triangleNode);
        SceneSerializer::SaveScene(triangleScene, "scenes/Triangle Scene.scene");

        Scene* squareScene = sm.CreateScene("Square Scene");
        TNode* squareNode = new TNode(new SquareEntity(), nullptr, "Square");
        squareNode->transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
        squareScene->AddNodeToRoot(squareNode);
        SceneSerializer::SaveScene(squareScene, "scenes/Square Scene.scene");

        sm.LoadScene("Triangle Scene");
    }
}

void Application::Shutdown()
{
    DebugUI::Shutdown();
    SceneManager::Instance().UnloadAllScenes();
    if (m_renderer)
        m_renderer->Shutdown();
}


