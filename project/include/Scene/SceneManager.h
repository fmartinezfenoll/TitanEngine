#pragma once
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include <functional>

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

    // Makes `name` the active scene, keeping only it in memory: the previously
    // active scene is saved to disk and unloaded first, and `name` is loaded
    // from scenes/{name}.scene. No-op if `name` is already active.
    void LoadScene(const std::string& name);

    // Creates a fresh empty scene, makes it the sole in-memory/active scene
    // (saving+unloading the previous one), and persists it so it shows up in
    // the disk-backed scene list. Used by the editor's "New Scene" action.
    Scene* NewScene(const std::string& name);

    // Writes the currently active scene back to its scenes/{name}.scene file.
    // Called on shutdown so quitting persists the active scene, matching the
    // save-on-switch behavior of LoadScene.
    void SaveActiveScene();

    void UnloadScene(const std::string& name);
    void UnloadAllScenes();
    bool RenameScene(const std::string& oldName, const std::string& newName);

    const std::unordered_map<std::string, std::shared_ptr<Scene>>& GetAllScenes() const {
        return scenes;
    }

    // Names (stems) of every scene file on disk under scenes/, excluding
    // autosave backups, sorted alphabetically. This, not the in-memory map, is
    // the source of truth for the scene selector, since only the active scene
    // is kept loaded.
    std::vector<std::string> GetAvailableSceneNames() const;

    // True if scenes/{name}.scene exists on disk.
    bool SceneFileExists(const std::string& name) const;

    const std::string& GetActiveSceneName() const { return activeSceneName; }

    bool SaveSceneToFile(const std::string& sceneName, const std::string& filePath);

    // Registered by the editor. Fired whenever the active scene actually changes
    // (load/new/delete), so the editor can drop any state pointing into the
    // now-unloaded scene (selection, gizmo drag, undo history) before that freed
    // memory is touched again.
    void SetOnActiveSceneChanged(std::function<void()> callback) {
        onActiveSceneChanged = std::move(callback);
    }

private:
    SceneManager() = default;
    ~SceneManager();

    void NotifyActiveSceneChanged() {
        if (onActiveSceneChanged) onActiveSceneChanged();
    }

    std::unordered_map<std::string, std::shared_ptr<Scene>> scenes;
    Scene* activeScene = nullptr;
    std::string activeSceneName;
    std::function<void()> onActiveSceneChanged;
};
