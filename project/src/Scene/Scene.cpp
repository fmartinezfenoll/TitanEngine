#include "Scene/Scene.h"
#include "Scene/CameraComponent.h"
#include "Scene/LightComponent.h"
#include <algorithm>

Scene::~Scene() {
    Clear();
}

void Scene::Init() {
    if (!root) {
        root = new TNode(nullptr);
    }
}

TNode* Scene::CreateNode(BoundingVolume* boundingBox) {
    return new TNode(boundingBox);
}

void Scene::AddNodeToRoot(TNode* node) {
    if (!root) {
        Init();
    }
    root->addChild(node);

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

void Scene::Draw(const Frustum& frustum, const glm::mat4& view, const glm::mat4& projection,
                 const std::vector<LightUniformData>& lightUniforms, const ShadowRenderData& shadowData) {
    if (root) {
        root->draw(frustum, view, projection, lightUniforms, shadowData);
    }
}

void Scene::Clear() {
    if (root) {
        delete root;
        root = nullptr;
    }
    cameras.clear();
    mainCamera = nullptr;
    lights.clear();
    skybox.reset();
}

void Scene::RegisterCamera(TNode* cameraNode) {
    if (!cameraNode) return;
    cameras.push_back(cameraNode);
    if (!mainCamera) {
        mainCamera = cameraNode;
    }
}

void Scene::UnregisterCamera(TNode* cameraNode) {
    auto it = std::find(cameras.begin(), cameras.end(), cameraNode);
    if (it != cameras.end()) {
        cameras.erase(it);
    }
    if (mainCamera == cameraNode) {
        mainCamera = cameras.empty() ? nullptr : cameras.front();
    }
}

void Scene::RegisterLight(TNode* lightNode) {
    if (!lightNode) return;
    lights.push_back(lightNode);
}

void Scene::UnregisterLight(TNode* lightNode) {
    auto it = std::find(lights.begin(), lights.end(), lightNode);
    if (it != lights.end()) {
        lights.erase(it);
    }
}
