#include "Scene/Scene.h"
#include "Scene/CameraComponent.h"
#include "Scene/LightComponent.h"
#include "Scene/MeshComponent.h"
#include "Scene/MaterialComponent.h"
#include "Scene/SkinComponent.h"
#include "Scene/AnimationComponent.h"
#include "Scene/PatrolComponent.h"
#include "Scene/CameraPathComponent.h"
#include "Scene/BillboardComponent.h"
#include "Scene/GrassComponent.h"
#include "Scene/ParticleSystemComponent.h"
#include <glad/glad.h>
#include <algorithm>
#include <functional>

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
                 || node->GetComponent<CameraPathComponent>() || node->GetComponent<ParticleSystemComponent>())) {
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
            // If this node's animation is state-machine-driven and some
            // ancestor is a patrol node (the common "CharacterRoot moves, a
            // descendant mesh plays a walk cycle" split -- see PatrolComponent's
            // design doc), feed the state machine an "isMoving" parameter from
            // the patrol's own paused/moving status before evaluating
            // transitions. Walks the whole ancestor chain, not just the direct
            // parent, so rigs with extra container nodes between the patrol node
            // and the animated node still couple correctly.
            if (auto* stateMachine = anim->GetStateMachine()) {
                for (TNode* ancestor = node->parent; ancestor; ancestor = ancestor->parent) {
                    if (auto* patrol = ancestor->GetComponent<PatrolComponent>()) {
                        stateMachine->SetBool("isMoving", !patrol->IsPaused());
                        break;
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
        if (auto* particles = node->GetComponent<ParticleSystemComponent>()) {
            particles->Update(deltaTime, node->getGlobalPosition());
        }
    }
}

void Scene::Draw(const Frustum& frustum, const glm::mat4& view, const glm::mat4& projection,
                 const glm::vec3& cameraWorldPos, const std::vector<LightUniformData>& lightUniforms,
                 const ShadowRenderData& shadowData, const IBLRenderData& iblData,
                 float time) {
    if (!root) return;

    // --- Opaque pass: meshes (recursive, fills the transparent list) ---
    std::vector<TransparentDrawItem> transparentItems;
    root->draw(frustum, view, projection, cameraWorldPos, lightUniforms, shadowData, iblData, &transparentItems);

    // Walk the tree once for the VFX components (billboards/grass/particles),
    // collecting each with its world matrix. Grass draws now (opaque, alpha-
    // cutout); billboards/particles are deferred into the transparent pass.
    std::vector<std::pair<BillboardComponent*, glm::vec3>> billboards;
    std::vector<std::pair<ParticleSystemComponent*, glm::vec3>> particleSystems;

    std::function<void(TNode*, const glm::mat4&)> walk = [&](TNode* node, const glm::mat4& parentMatrix) {
        if (!node) return;
        glm::mat4 modelMatrix = parentMatrix * node->transform.getModelMatrix();
        if (node->visible) {
            glm::vec3 worldPos(modelMatrix[3]);
            if (auto* grass = node->GetComponent<GrassComponent>()) {
                grass->Draw(modelMatrix, view, projection, time);
            }
            if (auto* billboard = node->GetComponent<BillboardComponent>()) {
                billboards.emplace_back(billboard, worldPos);
            }
            if (auto* particles = node->GetComponent<ParticleSystemComponent>()) {
                particleSystems.emplace_back(particles, worldPos);
            }
        }
        for (TNode* child : node->children) walk(child, modelMatrix);
    };
    walk(root, glm::mat4(1.0f));

    bool hasTransparent = !transparentItems.empty() || !billboards.empty() || !particleSystems.empty();
    if (!hasTransparent) return;

    // --- Transparent pass ---
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

    // Billboards: alpha-blended, camera-facing.
    for (const auto& [billboard, worldPos] : billboards) {
        billboard->Draw(view, projection, worldPos);
    }

    // Particle systems: each picks its own blend func (additive for fire/sparks,
    // alpha for smoke). Restore alpha blending afterward for consistency.
    for (const auto& [particles, worldPos] : particleSystems) {
        if (particles->blendMode == ParticleSystemComponent::BlendMode::Additive) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        } else {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        particles->Draw(view, projection);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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
