#pragma once
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <imgui.h>
#include "Scene/TNode.h"

class SceneManager;
class Scene;
class TNode;
class Material;

enum class GizmoMode { Move, Rotate, Scale };
enum class GizmoHandle { None, MoveX, MoveY, MoveZ, RotateX, RotateY, RotateZ, ScaleX, ScaleY, ScaleZ, ScaleUniform };
enum class GizmoSpace { Global, Local };

class DebugUI {
public:
    static void Init();
    static void Shutdown();
    static void ApplyTheme();
    static void DrawFrame(SceneManager* sceneManager);
    static TNode* GetSelectedNode() { return selectedNode; }
    static GizmoMode GetGizmoMode() { return gizmoMode; }
    static GizmoSpace GetGizmoSpace() { return gizmoSpace; }
    // Selects a standalone .material asset (not tied to any TNode) for editing in the Inspector.
    static void SelectMaterialAsset(const std::string& path);
    // Icon font (Material Symbols subset, see MaterialIcons.h) loaded by ApplyTheme().
    // Activate with ImGui::PushFont(...)/PopFont() around icon glyphs.
    static ImFont* GetIconFont() { return iconFont; }

private:
    static std::string DescribeNode(TNode* node);
    static void DrawSceneTree(TNode* node, Scene* activeScene, int depth = 0);
    static void DrawSceneSelector(SceneManager* sceneManager);
    static void DrawInspector(Scene* activeScene);
    static void DrawDeleteConfirmation();
    static void DrawNodeDeleteConfirmation();
    static void DrawSaveConfirmation();
    static void DrawCameraTab(SceneManager* sceneManager);
    static void DrawSkyboxInspector(Scene* activeScene);
    static void SelectScene();
    static void DeleteNode(TNode* node, Scene* activeScene);
    static void DrawAddComponentMenu(TNode* node, Scene* activeScene);
    static void DrawSaveMaterialPopup(const std::shared_ptr<Material>& material);
    // assetPath == nullptr: editing a node's MaterialComponent (manual "Save As..." only).
    // assetPath != nullptr: editing a standalone .material asset directly -- every changed
    // field auto-saves back to *assetPath immediately.
    static void DrawMaterialFields(const std::shared_ptr<Material>& mat, const std::string* assetPath);
    static void DrawCreateMenu(TNode* parent, Scene* activeScene);
    static TNode* PickAtCursor(Scene* activeScene);
    static void ComputePickRay(Scene* activeScene, glm::vec3& outOrigin, glm::vec3& outDirection);
    static GizmoHandle PickGizmoHandle(Scene* activeScene);
    static void BeginGizmoDrag(GizmoHandle handle, Scene* activeScene);
    static void UpdateGizmoDrag(Scene* activeScene);
    static void SelectNode(TNode* node);
    static void FocusOnSelected(Scene* activeScene);
    static void ToggleNodeInMultiSelect(TNode* node);
    static bool IsMultiSelected(TNode* node);
    static void DrawMultiDeleteConfirmation();
    static void UpdateAutoSave(SceneManager* sceneManager);
    static void DrawDockspace();

    static TNode* selectedNode;
    static bool sceneSelected;
    static std::string inspectingMaterialPath; // non-empty = a standalone .material asset is being inspected
    static std::shared_ptr<Material> inspectingMaterial;
    static ImFont* iconFont;
    static bool showDeleteConfirm;
    static std::string sceneToDelete;

    static TNode* nodeToDelete;
    static bool showNodeDeleteConfirm;

    static std::vector<TNode*> multiSelectedNodes;
    static bool showMultiDeleteConfirm;

    static std::string copiedNodeJson; // empty = clipboard empty; set by Copy, consumed (non-destructively) by Paste

    static bool showSaveConfirm;
    static std::string sceneToSave;

    static TNode* renamingNode;
    static char renameBuffer[256];
    static bool renameJustStarted;

    static GizmoMode gizmoMode;
    static GizmoSpace gizmoSpace;
    static GizmoHandle activeHandle;
    static glm::vec3 dragStartPointOnAxis;
    static float dragStartAngle;
    static glm::vec3 dragStartLocalPosition;
    static glm::vec3 dragStartLocalRotation;
    static glm::vec3 dragStartLocalScale;
    static TNode* dragNode;
    static ImVec2 dragStartMousePos;

    static char skyboxFolderBuffer[128];
    static std::string skyboxLoadError;

    static char sceneTreeFilter[128];
    static bool NodeMatchesFilter(TNode* node, const std::string& filter);

    static bool hasCopiedTransform;
    static Transform copiedTransform;
    static bool scaleAxisLocked[3]; // X, Y, Z -- when 2+ are checked, dragging any one of them drags the others by the same delta

    static float autoSaveTimer;
    static std::string lastAutoSaveStatus;

    static bool renamingScene;
    static char sceneRenameBuffer[128];
    static std::string sceneRenameError;

    static char saveMaterialBuffer[128];
    static std::string saveMaterialError;
};
