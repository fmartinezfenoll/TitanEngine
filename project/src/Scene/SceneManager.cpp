#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include <filesystem>
#include <algorithm>
#include "Core/Log.h"

namespace {
constexpr const char* kScenesDir = "scenes";

std::string ScenePath(const std::string& name) {
    return std::string(kScenesDir) + "/" + name + ".scene";
}
}

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
    if (name == activeSceneName && activeScene) return;

    // Persist the outgoing scene and drop it (and anything else) from memory so
    // that only the requested scene stays loaded. Saving first means switching
    // scenes never silently loses edits.
    if (activeScene) {
        SceneSerializer::SaveScene(activeScene, ScenePath(activeSceneName));
    }
    scenes.clear();
    activeScene = nullptr;
    activeSceneName.clear();

    std::string path = ScenePath(name);
    Scene* scene = SceneSerializer::LoadScene(path);
    if (!scene) {
        Log::Error("LoadScene: could not load scene file: " + path);
        return;
    }

    scene->Init();
    scenes[name] = std::shared_ptr<Scene>(scene);
    activeScene = scene;
    activeSceneName = name;

    NotifyActiveSceneChanged();
}

Scene* SceneManager::NewScene(const std::string& name) {
    if (activeScene) {
        SceneSerializer::SaveScene(activeScene, ScenePath(activeSceneName));
    }
    scenes.clear();

    auto scene = std::make_shared<Scene>();
    scene->Init();
    scenes[name] = scene;
    activeScene = scene.get();
    activeSceneName = name;

    // Persist immediately so the new scene appears in the disk-backed selector.
    std::filesystem::create_directories(kScenesDir);
    SceneSerializer::SaveScene(activeScene, ScenePath(name));

    NotifyActiveSceneChanged();
    return scene.get();
}

void SceneManager::SaveActiveScene() {
    if (activeScene) {
        std::filesystem::create_directories(kScenesDir);
        SceneSerializer::SaveScene(activeScene, ScenePath(activeSceneName));
    }
}

void SceneManager::UnloadScene(const std::string& name) {
    // Delete the scene file from disk.
    std::error_code ec;
    std::filesystem::remove(ScenePath(name), ec);
    if (!ec) {
        Log::Info("Scene file deleted: " + ScenePath(name));
    }

    bool wasActive = (name == activeSceneName);
    scenes.erase(name);

    if (wasActive) {
        activeScene = nullptr;
        activeSceneName.clear();

        // Switch to another scene on disk, if any remain. LoadScene fires the
        // change notification itself; if none remain, fire it here so the editor
        // still drops its now-dangling selection.
        std::vector<std::string> remaining = GetAvailableSceneNames();
        if (!remaining.empty()) {
            LoadScene(remaining.front());
        } else {
            NotifyActiveSceneChanged();
        }
    }
}

bool SceneManager::RenameScene(const std::string& oldName, const std::string& newName) {
    if (oldName == newName || newName.empty()) return false;
    if (SceneFileExists(newName)) return false;

    auto it = scenes.find(oldName);
    if (it != scenes.end()) {
        std::shared_ptr<Scene> scene = it->second;
        scenes.erase(it);
        scenes[newName] = scene;
    }

    if (activeSceneName == oldName) {
        activeSceneName = newName;
    }

    std::string oldPath = ScenePath(oldName);
    std::string newPath = ScenePath(newName);
    if (std::filesystem::exists(oldPath)) {
        std::error_code ec;
        std::filesystem::rename(oldPath, newPath, ec);
        if (ec) {
            Log::Error("Failed to rename scene file: " + oldPath + " -> " + newPath);
        }
    }

    return true;
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

std::vector<std::string> SceneManager::GetAvailableSceneNames() const {
    std::vector<std::string> names;
    if (!std::filesystem::exists(kScenesDir)) return names;

    for (const auto& entry : std::filesystem::directory_iterator(kScenesDir)) {
        if (entry.path().extension() != ".scene") continue;
        std::string stem = entry.path().stem().string();

        // Skip autosave backups -- they're not user-facing scenes.
        const std::string suffix = "_autosave";
        if (stem.size() >= suffix.size() &&
            stem.compare(stem.size() - suffix.size(), suffix.size(), suffix) == 0) {
            continue;
        }
        names.push_back(stem);
    }

    std::sort(names.begin(), names.end());
    return names;
}

bool SceneManager::SceneFileExists(const std::string& name) const {
    return std::filesystem::exists(ScenePath(name));
}
