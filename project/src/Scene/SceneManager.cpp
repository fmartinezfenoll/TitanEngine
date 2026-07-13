#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include <filesystem>
#include "Core/Log.h"

SceneManager::~SceneManager() {
    UnloadAllScenes();
}

Scene* SceneManager::CreateScene(const std::string& name) {
    if (scenes.find(name) != scenes.end()) {
        return scenes[name].get();
    }

    auto scene = std::make_shared<Scene>();
    scene->Init();
    scenes[name] = scene;

    if (!activeScene) {
        activeScene = scene.get();
        activeSceneName = name;
    }

    return scene.get();
}

Scene* SceneManager::GetScene(const std::string& name) {
    auto it = scenes.find(name);
    if (it != scenes.end()) {
        return it->second.get();
    }
    return nullptr;
}

void SceneManager::LoadScene(const std::string& name) {
    Scene* scene = GetScene(name);
    if (scene) {
        activeScene = scene;
        activeSceneName = name;
    }
}

void SceneManager::UnloadScene(const std::string& name) {
    auto it = scenes.find(name);
    if (it != scenes.end()) {
        if (activeScene == it->second.get()) {
            activeScene = nullptr;
            activeSceneName = "";

            // Switch to another scene if available
            if (!scenes.empty()) {
                auto nextIt = scenes.begin();
                if (nextIt->first == name && std::next(nextIt) != scenes.end()) {
                    nextIt = std::next(nextIt);
                }
                activeScene = nextIt->second.get();
                activeSceneName = nextIt->first;
            }
        }
        scenes.erase(it);

        // Delete the scene file
        std::string filePath = "scenes/" + name + ".scene";
        if (std::filesystem::exists(filePath)) {
            std::filesystem::remove(filePath);
            Log::Info("Scene file deleted: " + filePath);
        }
    }
}

void SceneManager::UnloadAllScenes() {
    scenes.clear();
    activeScene = nullptr;
    activeSceneName = "";
}

bool SceneManager::SaveSceneToFile(const std::string& sceneName, const std::string& filePath) {
    Scene* scene = GetScene(sceneName);
    if (!scene) {
        return false;
    }
    return SceneSerializer::SaveScene(scene, filePath);
}

Scene* SceneManager::LoadSceneFromFile(const std::string& filePath) {
    Scene* scene = SceneSerializer::LoadScene(filePath);
    if (!scene) {
        return nullptr;
    }

    static int load_counter = 0;
    std::string sceneName = "loaded_scene_" + std::to_string(load_counter++);

    auto scene_ptr = std::make_shared<Scene>();
    scene_ptr.reset(scene);
    scenes[sceneName] = scene_ptr;

    if (!activeScene) {
        activeScene = scene;
        activeSceneName = sceneName;
    }

    return scene;
}

void SceneManager::LoadAllScenesFromDirectory(const std::string& directory) {
    if (!std::filesystem::exists(directory)) {
        Log::Info("Scenes directory does not exist: " + directory);
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() == ".scene") {
            std::string sceneFile = entry.path().string();
            std::string sceneName = entry.path().stem().string();

            Scene* scene = SceneSerializer::LoadScene(sceneFile);
            if (scene) {
                scene->Init();
                auto scene_ptr = std::make_shared<Scene>();
                scene_ptr.reset(scene);
                scenes[sceneName] = scene_ptr;

                if (!activeScene) {
                    activeScene = scene;
                    activeSceneName = sceneName;
                }
            }
        }
    }
}
