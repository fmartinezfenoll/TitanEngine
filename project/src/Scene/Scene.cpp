#include "Scene/Scene.h"
#include "Scene/CameraComponent.h"
#include "Scene/LightComponent.h"
#include <algorithm>

Scene::~Scene() {
    Clear();
}

void Scene::Init() {
    if (!m_root) {
        m_root = new TNode(nullptr);
    }
}

TNode* Scene::CreateNode(BoundingVolume* boundingBox) {
    return new TNode(boundingBox);
}

void Scene::AddNodeToRoot(TNode* node) {
    if (!m_root) {
        Init();
    }
    m_root->addChild(node);

    if (node && node->GetComponent<CameraComponent>()) {
        RegisterCamera(node);
    }

    if (node && node->GetComponent<LightComponent>()) {
        RegisterLight(node);
    }
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
    m_cameras.clear();
    m_mainCamera = nullptr;
    m_lights.clear();
}

void Scene::RegisterCamera(TNode* cameraNode) {
    if (!cameraNode) return;
    m_cameras.push_back(cameraNode);
    if (!m_mainCamera) {
        m_mainCamera = cameraNode;
    }
}

void Scene::UnregisterCamera(TNode* cameraNode) {
    auto it = std::find(m_cameras.begin(), m_cameras.end(), cameraNode);
    if (it != m_cameras.end()) {
        m_cameras.erase(it);
    }
    if (m_mainCamera == cameraNode) {
        m_mainCamera = m_cameras.empty() ? nullptr : m_cameras.front();
    }
}

void Scene::RegisterLight(TNode* lightNode) {
    if (!lightNode) return;
    m_lights.push_back(lightNode);
}

void Scene::UnregisterLight(TNode* lightNode) {
    auto it = std::find(m_lights.begin(), m_lights.end(), lightNode);
    if (it != m_lights.end()) {
        m_lights.erase(it);
    }
}
