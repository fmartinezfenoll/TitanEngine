#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include <filesystem>
#include "Core/Log.h"

SceneManager::~SceneManager() {
    UnloadAllScenes();
}

Scene* SceneManager::CreateScene(const std::string& name) {
    if (m_scenes.find(name) != m_scenes.end()) {
        return m_scenes[name].get();
    }

    auto scene = std::make_shared<Scene>();
    scene->Init();
    m_scenes[name] = scene;

    if (!m_activeScene) {
        m_activeScene = scene.get();
        m_activeSceneName = name;
    }

    return scene.get();
}

Scene* SceneManager::GetScene(const std::string& name) {
    auto it = m_scenes.find(name);
    if (it != m_scenes.end()) {
        return it->second.get();
    }
    return nullptr;
}

void SceneManager::LoadScene(const std::string& name) {
    Scene* scene = GetScene(name);
    if (scene) {
        m_activeScene = scene;
        m_activeSceneName = name;
    }
}

void SceneManager::UnloadScene(const std::string& name) {
    auto it = m_scenes.find(name);
    if (it != m_scenes.end()) {
        if (m_activeScene == it->second.get()) {
            m_activeScene = nullptr;
            m_activeSceneName = "";

            // Switch to another scene if available
            if (!m_scenes.empty()) {
                auto nextIt = m_scenes.begin();
                if (nextIt->first == name && std::next(nextIt) != m_scenes.end()) {
                    nextIt = std::next(nextIt);
                }
                m_activeScene = nextIt->second.get();
                m_activeSceneName = nextIt->first;
            }
        }
        m_scenes.erase(it);

        // Delete the scene file
        std::string filePath = "scenes/" + name + ".scene";
        if (std::filesystem::exists(filePath)) {
            std::filesystem::remove(filePath);
            Log::Info("Scene file deleted: " + filePath);
        }
    }
}

void SceneManager::UnloadAllScenes() {
    m_scenes.clear();
    m_activeScene = nullptr;
    m_activeSceneName = "";
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
    m_scenes[sceneName] = scene_ptr;

    if (!m_activeScene) {
        m_activeScene = scene;
        m_activeSceneName = sceneName;
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
                m_scenes[sceneName] = scene_ptr;

                if (!m_activeScene) {
                    m_activeScene = scene;
                    m_activeSceneName = sceneName;
                }
            }
        }
    }
}
