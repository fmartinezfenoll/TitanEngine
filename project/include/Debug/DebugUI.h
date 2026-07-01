#pragma once
#include <string>

class SceneManager;
class TNode;

class DebugUI {
public:
    static void Init();
    static void Shutdown();
    static void DrawFrame(SceneManager* sceneManager);

private:
    static void DrawSceneTree(TNode* node, int depth = 0);
    static void DrawResourcesTree();
    static void DrawNodeProperties(TNode* node);
    static void DrawSceneSelector(SceneManager* sceneManager);
    static void DrawInspector();
    static void DrawDeleteConfirmation();
    static void DrawCameraTab(SceneManager* sceneManager);

    static TNode* m_selectedNode;
    static bool m_showDeleteConfirm;
    static std::string m_sceneToDelete;
};
