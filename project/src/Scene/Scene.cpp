#include "Scene/Scene.h"
#include "Scene/CameraComponent.h"
#include "Scene/LightComponent.h"
#include "Scene/MeshComponent.h"
#include "Scene/MaterialComponent.h"
#include <glad/glad.h>
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
                 const glm::vec3& cameraWorldPos, const std::vector<LightUniformData>& lightUniforms,
                 const ShadowRenderData& shadowData, const IBLRenderData& iblData) {
    if (!root) return;

    std::vector<TransparentDrawItem> transparentItems;
    root->draw(frustum, view, projection, cameraWorldPos, lightUniforms, shadowData, iblData, &transparentItems);

    if (transparentItems.empty()) return;

    std::sort(transparentItems.begin(), transparentItems.end(),
        [&cameraWorldPos](const TransparentDrawItem& a, const TransparentDrawItem& b) {
            glm::vec3 posA(a.modelMatrix[3]);
            glm::vec3 posB(b.modelMatrix[3]);
            float distA = glm::dot(cameraWorldPos - posA, cameraWorldPos - posA);
            float distB = glm::dot(cameraWorldPos - posB, cameraWorldPos - posB);
            return distA > distB; // farthest first (back-to-front)
        });

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); // depth test stays enabled -- still occluded correctly by opaque geometry

    for (const auto& item : transparentItems) {
        if (auto* mesh = item.node->GetComponent<MeshComponent>()) {
            mesh->Draw(item.modelMatrix, item.node->GetComponent<MaterialComponent>(),
                       view, projection, cameraWorldPos, lightUniforms, shadowData, iblData);
        }
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
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
