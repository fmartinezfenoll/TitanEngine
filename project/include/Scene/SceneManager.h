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
    Scene* GetActiveScene() { return m_activeScene; }

    void LoadScene(const std::string& name);
    void UnloadScene(const std::string& name);
    void UnloadAllScenes();

    const std::unordered_map<std::string, std::shared_ptr<Scene>>& GetAllScenes() const {
        return m_scenes;
    }

    const std::string& GetActiveSceneName() const { return m_activeSceneName; }

    bool SaveSceneToFile(const std::string& sceneName, const std::string& filePath);
    Scene* LoadSceneFromFile(const std::string& filePath);
    void LoadAllScenesFromDirectory(const std::string& directory);

private:
    SceneManager() = default;
    ~SceneManager();

    std::unordered_map<std::string, std::shared_ptr<Scene>> m_scenes;
    Scene* m_activeScene = nullptr;
    std::string m_activeSceneName;
};
