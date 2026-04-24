#include "Scene/Scene.h"

Scene::~Scene() {
    Clear();
}

void Scene::Init() {
    if (!m_root) {
        m_root = new TNode(nullptr, nullptr);
    }
}

TNode* Scene::CreateNode(TEntity* entity, BoundingVolume* boundingBox) {
    return new TNode(entity, boundingBox);
}

void Scene::AddNodeToRoot(TNode* node) {
    if (!m_root) {
        Init();
    }
    m_root->addChild(node);
}

void Scene::RemoveNode(TNode* node) {
    if (node && node->parent) {
        node->removeFromParent();
    }
}

void Scene::Update(float deltaTime) {
    // Placeholder for future physics/logic updates
}

void Scene::Draw(const Frustum& frustum) {
    if (m_root) {
        m_root->draw(frustum);
    }
}

void Scene::Clear() {
    if (m_root) {
        delete m_root;
        m_root = nullptr;
    }
}
