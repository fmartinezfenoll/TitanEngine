#pragma once
#include <string>
#include <glm/glm.hpp>
#include <imgui.h>

class SceneManager;
class Scene;
class TNode;

enum class GizmoMode { Move, Rotate, Scale };
enum class GizmoHandle { None, MoveX, MoveY, MoveZ, RotateX, RotateY, RotateZ, ScaleX, ScaleY, ScaleZ, ScaleUniform };
enum class GizmoSpace { Global, Local };

class DebugUI {
public:
    static void Init();
    static void Shutdown();
    static void DrawFrame(SceneManager* sceneManager);
    static TNode* GetSelectedNode() { return selectedNode; }
    static GizmoMode GetGizmoMode() { return gizmoMode; }
    static GizmoSpace GetGizmoSpace() { return gizmoSpace; }

private:
    static std::string DescribeNode(TNode* node);
    static void DrawSceneTree(TNode* node, Scene* activeScene, int depth = 0);
    static void DrawResourcesTree();
    static void DrawNodeProperties(TNode* node);
    static void DrawSceneSelector(SceneManager* sceneManager);
    static void DrawInspector(Scene* activeScene);
    static void DrawDeleteConfirmation();
    static void DrawNodeDeleteConfirmation();
    static void DrawCameraTab(SceneManager* sceneManager);
    static void DrawSkyboxInspector(Scene* activeScene);
    static void SelectScene();
    static void DeleteNode(TNode* node, Scene* activeScene);
    static void DrawAddComponentMenu(TNode* node, Scene* activeScene);
    static void DrawCreateMenu(TNode* parent, Scene* activeScene);
    static TNode* PickAtCursor(Scene* activeScene);
    static void ComputePickRay(Scene* activeScene, glm::vec3& outOrigin, glm::vec3& outDirection);
    static GizmoHandle PickGizmoHandle(Scene* activeScene);
    static void BeginGizmoDrag(GizmoHandle handle, Scene* activeScene);
    static void UpdateGizmoDrag(Scene* activeScene);
    static void SelectNode(TNode* node);

    static TNode* selectedNode;
    static bool sceneSelected;
    static bool showDeleteConfirm;
    static std::string sceneToDelete;

    static TNode* nodeToDelete;
    static bool showNodeDeleteConfirm;

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
};
