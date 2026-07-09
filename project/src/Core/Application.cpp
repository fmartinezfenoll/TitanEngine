#include "Core/Application.h"
#include "Core/Time.h"
#include "Renderer/RendererFactory.h"
#include "Renderer/IRenderer.h"
#include <iostream>
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/Material.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/MeshComponent.h"
#include "Scene/MaterialComponent.h"
#include "Scene/SceneSerializer.h"
#include "Scene/CameraComponent.h"
#include "Scene/LightComponent.h"
#include "Scene/GLTFLoader.h"
#include "Debug/DebugUI.h"
#include <glm/glm.hpp>
#include <filesystem>

namespace {

TNode* BuildTriangleNode() {
    std::vector<MeshVertex> vertices = {
        { {-0.5f, -0.5f, 0.0f}, {}, {} },
        { { 0.5f, -0.5f, 0.0f}, {}, {} },
        { { 0.0f,  0.5f, 0.0f}, {}, {} },
    };
    std::vector<uint32_t> indices = { 0, 1, 2 };

    auto material = std::make_shared<Material>(ResourceManager::LoadShader("basic"));

    TNode* node = new TNode(nullptr, "Triangle");
    node->AddComponent<MeshComponent>(vertices, indices);
    node->AddComponent<MaterialComponent>(material);
    return node;
}

TNode* BuildSquareNode() {
    std::vector<MeshVertex> vertices = {
        { {-0.5f,  0.5f, 0.0f}, {}, {} },
        { {-0.5f, -0.5f, 0.0f}, {}, {} },
        { { 0.5f, -0.5f, 0.0f}, {}, {} },
        { { 0.5f,  0.5f, 0.0f}, {}, {} },
    };
    std::vector<uint32_t> indices = { 0, 1, 2, 0, 2, 3 };

    auto material = std::make_shared<Material>(ResourceManager::LoadShader("basic"));

    TNode* node = new TNode(nullptr, "Square");
    node->AddComponent<MeshComponent>(vertices, indices);
    node->AddComponent<MaterialComponent>(material);
    return node;
}

} // namespace

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
        m_renderer->Update(deltaTime);

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
        TNode* triangleNode = BuildTriangleNode();
        triangleNode->transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
        triangleScene->AddNodeToRoot(triangleNode);
        SceneSerializer::SaveScene(triangleScene, "scenes/Triangle Scene.scene");

        Scene* squareScene = sm.CreateScene("Square Scene");
        TNode* squareNode = BuildSquareNode();
        squareNode->transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
        squareScene->AddNodeToRoot(squareNode);
        SceneSerializer::SaveScene(squareScene, "scenes/Square Scene.scene");

        sm.LoadScene("Triangle Scene");
    }

    if (!sm.GetScene("GLTF Scene")) {
        Scene* gltfScene = sm.CreateScene("GLTF Scene");
        for (TNode* node : GLTFLoader::LoadModel("resources/models/Box.glb")) {
            gltfScene->AddNodeToRoot(node);
        }
    }

    if (!sm.GetScene("Duck Scene")) {
        Scene* duckScene = sm.CreateScene("Duck Scene");
        for (TNode* node : GLTFLoader::LoadModel("resources/models/Duck.glb")) {
            duckScene->AddNodeToRoot(node);
        }
    }

    sm.LoadScene("Duck Scene");

    for (const auto& [name, scene] : sm.GetAllScenes()) {
        if (!scene->GetMainCamera()) {
            TNode* cameraNode = new TNode(nullptr, "MainCamera");
            CameraComponent* camera = cameraNode->AddComponent<CameraComponent>(cameraNode);

            if (name == "Duck Scene") {
                cameraNode->transform.position = glm::vec3(0.0f, 50.0f, 150.0f);
                camera->farPlane = 1000.0f;
            } else {
                cameraNode->transform.position = glm::vec3(0.0f, 0.0f, 3.0f);
            }

            scene->AddNodeToRoot(cameraNode);
        }

        if (scene->GetLights().empty()) {
            TNode* lightNode = new TNode(nullptr, "DirectionalLight1");
            lightNode->AddComponent<LightComponent>(lightNode, LightType::Directional);
            lightNode->transform.rotation = glm::vec3(-50.0f, -30.0f, 0.0f);
            scene->AddNodeToRoot(lightNode);
        }
    }
}

void Application::Shutdown()
{
    DebugUI::Shutdown();
    SceneManager::Instance().UnloadAllScenes();
    if (m_renderer)
        m_renderer->Shutdown();
}


