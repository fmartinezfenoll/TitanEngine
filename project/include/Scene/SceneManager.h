#pragma once
#include <unordered_map>
#include <string>
#include <memory>

class Scene;

class SceneManager {
public:
    static SceneManager& Instance() {
        static SceneManager instance;
        return instance;
    }

    Scene* CreateScene(const std::string& name);
    Scene* GetScene(const std::string& name);
    Scene* GetActiveScene() { return activeScene; }

    void LoadScene(const std::string& name);
    void UnloadScene(const std::string& name);
    void UnloadAllScenes();
    bool RenameScene(const std::string& oldName, const std::string& newName);

    const std::unordered_map<std::string, std::shared_ptr<Scene>>& GetAllScenes() const {
        return scenes;
    }

    const std::string& GetActiveSceneName() const { return activeSceneName; }

    bool SaveSceneToFile(const std::string& sceneName, const std::string& filePath);
    Scene* LoadSceneFromFile(const std::string& filePath);
    void LoadAllScenesFromDirectory(const std::string& directory);

private:
    SceneManager() = default;
    ~SceneManager();

    std::unordered_map<std::string, std::shared_ptr<Scene>> scenes;
    Scene* activeScene = nullptr;
    std::string activeSceneName;
};
