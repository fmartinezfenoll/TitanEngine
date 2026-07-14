#pragma once
#include <filesystem>
#include <string>
#include <vector>

class SceneManager;
class Scene;

class ProjectBrowser {
public:
    static void Draw(SceneManager* sceneManager, Scene* activeScene);

    // Drag-drop payload type shared with the Material inspector's texture slot targets.
    static constexpr const char* kTexturePayloadType = "ASSET_TEXTURE_PATH";

    // Drag-drop payload type for dropping a saved .material asset onto an object's Material slot.
    static constexpr const char* kMaterialPayloadType = "ASSET_MATERIAL_PATH";

    // Drag-drop payload type for dropping a saved .prefab asset onto the Scene Tree to instantiate it.
    static constexpr const char* kPrefabPayloadType = "ASSET_PREFAB_PATH";

    // Generic drag-drop payload type carrying any asset's path, used to move it onto a folder.
    static constexpr const char* kAssetMovePayloadType = "ASSET_MOVE_PATH";

    // Called from the GLFW drop callback (a free function) when files are dropped onto the
    // window from an external app (e.g. Windows Explorer). Only queues paths -- the actual
    // copy into currentDir happens next frame in Draw(), inside the ImGui frame.
    static void EnqueueDroppedPaths(int count, const char** paths);

private:
    static void DrawRoots();
    static void DrawBreadcrumb();
    static void DrawToolbar(SceneManager* sceneManager);
    static void DrawGrid(SceneManager* sceneManager, Scene* activeScene);
    static void DrawItem(const std::filesystem::directory_entry& entry, SceneManager* sceneManager, Scene* activeScene);
    static void DrawDetailsPanel();
    static void HandleActivate(const std::filesystem::path& path, SceneManager* sceneManager, Scene* activeScene);
    static void DrawContextMenu(const std::filesystem::path& path, SceneManager* sceneManager);
    static void DrawBackgroundContextMenu(SceneManager* sceneManager);
    static void DrawRenamePopup(SceneManager* sceneManager);
    static void DrawDeleteConfirm(SceneManager* sceneManager);
    static void DrawNewFolderPopup();
    static void ProcessPendingDrops();
    static void DrawImportConflictPopup();
    static void DrawMultiDeleteConfirm(SceneManager* sceneManager);

    // Multi-select (Ctrl/Shift+click), mirroring DebugUI's multiSelectedNodes pattern:
    // seeds from selectedPath on first toggle, collapses back to selectedPath at size 1.
    static void ToggleAssetInMultiSelect(const std::filesystem::path& path);
    static bool IsAssetMultiSelected(const std::filesystem::path& path);

    // Copies a dropped file/folder into currentDir. keepBoth adds a _N suffix on
    // name collision; otherwise overwrites the existing destination.
    static void ImportOne(const std::filesystem::path& src, bool keepBoth);

    // Shared "create X in currentDir" helpers, reused by both the toolbar buttons
    // and the empty-space right-click menu.
    static void CreateNewScene(SceneManager* sceneManager);
    static void CreateNewMaterial();
    static void OpenNewFolderPopup();

    // Opens the OS file explorer at the given path (or currentDir for a folder).
    static void RevealInExplorer(const std::filesystem::path& path);

    // True if folderPath contains (recursively) the .scene file backing the currently active scene.
    static bool FolderContainsActiveScene(const std::filesystem::path& folderPath, SceneManager* sceneManager);

    // Deletes a single asset (kind-specific: Folder guards + remove_all, Scene goes
    // through SceneManager, everything else is a plain remove). Returns an error
    // message on failure (e.g. folder contains the active scene), or empty on success.
    static std::string DeleteOneAsset(const std::filesystem::path& target, SceneManager* sceneManager);

    static std::filesystem::path currentDir;
    static std::filesystem::path selectedPath;
    static std::vector<std::filesystem::path> multiSelectedPaths;
    static bool showMultiDeleteConfirm;

    static bool renamingItem;
    static std::filesystem::path renameTarget;
    static char renameBuffer[256];
    static std::string renameError;

    static bool showDeleteConfirm;
    static std::filesystem::path deleteTarget;
    static std::string deleteError;

    static bool creatingFolder;
    static char newFolderBuffer[128];
    static std::string newFolderError;

    static char searchFilter[128];

    // Sort mode for the grid: 0 = Name, 1 = Type, 2 = Date modified. Folders always sort first.
    static int sortMode;
    static bool sortAscending;

    // Paths dropped onto the window from an external app, queued by the GLFW drop callback
    // and drained by ProcessPendingDrops() at the top of Draw().
    static std::vector<std::string> pendingDrops;

    // While non-empty, an import hit a name collision and the conflict modal is asking
    // the user to Overwrite / Keep both / Cancel for this source path.
    static std::filesystem::path importConflictSrc;
    static bool importApplyToAll;   // remembered choice for the rest of this drop batch
    static int importRememberedChoice; // -1 = none, 0 = overwrite, 1 = keep both, 2 = cancel
};
