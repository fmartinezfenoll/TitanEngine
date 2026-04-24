#pragma once
#include "Scene/TNode.h"
#include "Scene/TEntity.h"

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

private:
    TNode* m_root = nullptr;
};
