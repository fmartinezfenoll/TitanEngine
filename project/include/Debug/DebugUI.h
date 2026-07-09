#pragma once
#include <string>
#include <glm/glm.hpp>

class SceneManager;
class Scene;
class TNode;

enum class GizmoMode { Move, Rotate, Scale };
enum class GizmoHandle { None, MoveX, MoveY, MoveZ, RotateX, RotateY, RotateZ, ScaleX, ScaleY, ScaleZ };

class DebugUI {
public:
    static void Init();
    static void Shutdown();
    static void DrawFrame(SceneManager* sceneManager);
    static TNode* GetSelectedNode() { return m_selectedNode; }
    static GizmoMode GetGizmoMode() { return m_gizmoMode; }

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
    static void DeleteNode(TNode* node, Scene* activeScene);
    static void DrawAddComponentMenu(TNode* node, Scene* activeScene);
    static void DrawCreateMenu(TNode* parent, Scene* activeScene);
    static TNode* PickAtCursor(Scene* activeScene);
    static void ComputePickRay(Scene* activeScene, glm::vec3& outOrigin, glm::vec3& outDirection);
    static GizmoHandle PickGizmoHandle(Scene* activeScene);
    static void BeginGizmoDrag(GizmoHandle handle, Scene* activeScene);
    static void UpdateGizmoDrag(Scene* activeScene);
    static void SelectNode(TNode* node);

    static TNode* m_selectedNode;
    static bool m_showDeleteConfirm;
    static std::string m_sceneToDelete;

    static TNode* m_nodeToDelete;
    static bool m_showNodeDeleteConfirm;

    static TNode* m_renamingNode;
    static char m_renameBuffer[256];
    static bool m_renameJustStarted;

    static GizmoMode m_gizmoMode;
    static GizmoHandle m_activeHandle;
    static glm::vec3 m_dragStartPointOnAxis;
    static float m_dragStartAngle;
    static glm::vec3 m_dragStartLocalPosition;
    static glm::vec3 m_dragStartLocalRotation;
    static glm::vec3 m_dragStartLocalScale;
    static TNode* m_dragNode;
};
