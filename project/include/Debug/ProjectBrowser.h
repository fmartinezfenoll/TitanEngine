#pragma once
#include <filesystem>
#include <string>

class SceneManager;
class Scene;

class ProjectBrowser {
public:
    static void Draw(SceneManager* sceneManager, Scene* activeScene);

    // Drag-drop payload type shared with the Material inspector's texture slot targets.
    static constexpr const char* kTexturePayloadType = "ASSET_TEXTURE_PATH";

private:
    static void DrawRoots();
    static void DrawBreadcrumb();
    static void DrawToolbar(SceneManager* sceneManager);
    static void DrawGrid(SceneManager* sceneManager, Scene* activeScene);
    static void DrawItem(const std::filesystem::directory_entry& entry, SceneManager* sceneManager, Scene* activeScene);
    static void HandleActivate(const std::filesystem::path& path, SceneManager* sceneManager, Scene* activeScene);
    static void DrawContextMenu(const std::filesystem::path& path, SceneManager* sceneManager);
    static void DrawRenamePopup(SceneManager* sceneManager);
    static void DrawDeleteConfirm(SceneManager* sceneManager);

    static std::filesystem::path currentDir;
    static std::filesystem::path selectedPath;

    static bool renamingItem;
    static std::filesystem::path renameTarget;
    static char renameBuffer[256];
    static std::string renameError;

    static bool showDeleteConfirm;
    static std::filesystem::path deleteTarget;
};
