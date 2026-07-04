#pragma once
#include <string>

class SceneManager;
class Scene;
class TNode;

class DebugUI {
public:
    static void Init();
    static void Shutdown();
    static void DrawFrame(SceneManager* sceneManager);

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

    static TNode* m_selectedNode;
    static bool m_showDeleteConfirm;
    static std::string m_sceneToDelete;

    static TNode* m_nodeToDelete;
    static bool m_showNodeDeleteConfirm;

    static TNode* m_renamingNode;
    static char m_renameBuffer[256];
    static bool m_renameJustStarted;
};
