#include "Scene/Scene.h"
#include "Scene/CameraComponent.h"
#include "Scene/LightComponent.h"
#include "Scene/MeshComponent.h"
#include "Scene/MaterialComponent.h"
#include "Scene/SkinComponent.h"
#include "Scene/AnimationComponent.h"
#include "Scene/PatrolComponent.h"
#include "Scene/CameraPathComponent.h"
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

    if (node && (node->GetComponent<AnimationComponent>() || node->GetComponent<PatrolComponent>()
                 || node->GetComponent<CameraPathComponent>())) {
        RegisterAnimator(node);
    }
}

void Scene::RemoveNode(TNode* node) {
    if (node && node->parent) {
        node->removeFromParent();
    }
}

void Scene::Update(float deltaTime) {
    for (TNode* node : animatedNodes) {
        if (auto* anim = node->GetComponent<AnimationComponent>()) {
            // If this node's animation is state-machine-driven and its parent
            // is a patrol node (the common "CharacterRoot moves, child mesh
            // plays a walk cycle" split -- see PatrolComponent's design doc),
            // feed the state machine an "isMoving" parameter from the patrol's
            // own paused/moving status before evaluating transitions.
            if (auto* stateMachine = anim->GetStateMachine()) {
                if (node->parent) {
                    if (auto* patrol = node->parent->GetComponent<PatrolComponent>()) {
                        stateMachine->SetBool("isMoving", !patrol->IsPaused());
                    }
                }
            }
            anim->Update(deltaTime);
        }
        if (auto* patrol = node->GetComponent<PatrolComponent>()) {
            patrol->Update(deltaTime);
        }
        if (auto* cameraPath = node->GetComponent<CameraPathComponent>()) {
            cameraPath->Update(deltaTime);
        }
    }
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
                       view, projection, cameraWorldPos, lightUniforms, shadowData, iblData,
                       item.node->GetComponent<SkinComponent>());
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
    animatedNodes.clear();
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

void Scene::RegisterAnimator(TNode* node) {
    if (!node) return;
    // Idempotent: a node can carry both an AnimationComponent and a
    // PatrolComponent (or gain a second one in-place via "Add Component"),
    // each of which registers independently -- avoid double-ticking Update().
    if (std::find(animatedNodes.begin(), animatedNodes.end(), node) != animatedNodes.end()) return;
    animatedNodes.push_back(node);
}

void Scene::UnregisterAnimator(TNode* node) {
    auto it = std::find(animatedNodes.begin(), animatedNodes.end(), node);
    if (it != animatedNodes.end()) {
        animatedNodes.erase(it);
    }
}
