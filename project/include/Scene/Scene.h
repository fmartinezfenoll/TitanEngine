#pragma once
#include "Scene/TNode.h"
#include "Scene/TEntity.h"
#include <vector>

class Scene {
public:
    Scene() : m_root(nullptr) {}
    ~Scene();

    void Init();

    TNode* GetRoot() { return m_root; }
    TNode* CreateNode(TEntity* entity = nullptr, BoundingVolume* boundingBox = nullptr);

    void AddNodeToRoot(TNode* node);
    void RemoveNode(TNode* node);

    void Update(float deltaTime);
    void Draw(const Frustum& frustum);

    void Clear();

    void RegisterCamera(TNode* cameraNode);
    void UnregisterCamera(TNode* cameraNode);
    const std::vector<TNode*>& GetCameras() const { return m_cameras; }

    void SetMainCamera(TNode* cameraNode) { m_mainCamera = cameraNode; }
    TNode* GetMainCamera() const { return m_mainCamera; }

private:
    TNode* m_root = nullptr;
    std::vector<TNode*> m_cameras;
    TNode* m_mainCamera = nullptr;
};
