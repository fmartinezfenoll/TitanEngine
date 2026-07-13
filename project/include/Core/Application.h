#pragma once
#include <memory>
#include "Core/Config.h"

class IRenderer;
class SceneManager;

class Application
{
public:
    Application(const AppConfig& config = AppConfig());
    ~Application();

    bool Init();
    void Run();
    virtual void Update(float deltaTime);
    void OnImGui();
    void Shutdown();

    SceneManager& GetSceneManager();

protected:
    virtual void SetupScenes();

private:
    AppConfig config;
    std::unique_ptr<IRenderer> renderer;
};
