#include "Debug/DebugUI.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/TNode.h"
#include "Scene/SceneSerializer.h"
#include "Scene/CameraComponent.h"
#include "Scene/MeshComponent.h"
#include "Scene/MaterialComponent.h"
#include "Scene/LightComponent.h"
#include "ResourceManager/Material.h"
#include "ResourceManager/Texture.h"
#include "ResourceManager/OpenGLShader.h"
#include "ResourceManager/ResourceManager.h"
#include "Renderer/GizmoRenderer.h"
#include "Renderer/Viewport.h"
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <filesystem>
#include <cstdio>
#include <unordered_map>
#include <limits>
#include <algorithm>
#include <cmath>

namespace {

void GetDefaultCubeMesh(std::vector<MeshVertex>& vertices, std::vector<uint32_t>& indices) {
    // 24 verts (4 per face, own normal/uv), 36 indices — standard "new mesh" cube.
    vertices = {
        // +X
        { {0.5f,-0.5f,-0.5f}, {1,0,0}, {0,0} }, { {0.5f, 0.5f,-0.5f}, {1,0,0}, {1,0} },
        { {0.5f, 0.5f, 0.5f}, {1,0,0}, {1,1} }, { {0.5f,-0.5f, 0.5f}, {1,0,0}, {0,1} },
        // -X
        { {-0.5f,-0.5f, 0.5f}, {-1,0,0}, {0,0} }, { {-0.5f, 0.5f, 0.5f}, {-1,0,0}, {1,0} },
        { {-0.5f, 0.5f,-0.5f}, {-1,0,0}, {1,1} }, { {-0.5f,-0.5f,-0.5f}, {-1,0,0}, {0,1} },
        // +Y
        { {-0.5f,0.5f,-0.5f}, {0,1,0}, {0,0} }, { {-0.5f,0.5f, 0.5f}, {0,1,0}, {1,0} },
        { {0.5f,0.5f, 0.5f}, {0,1,0}, {1,1} }, { {0.5f,0.5f,-0.5f}, {0,1,0}, {0,1} },
        // -Y
        { {-0.5f,-0.5f, 0.5f}, {0,-1,0}, {0,0} }, { {-0.5f,-0.5f,-0.5f}, {0,-1,0}, {1,0} },
        { {0.5f,-0.5f,-0.5f}, {0,-1,0}, {1,1} }, { {0.5f,-0.5f, 0.5f}, {0,-1,0}, {0,1} },
        // +Z
        { {-0.5f,-0.5f,0.5f}, {0,0,1}, {0,0} }, { {0.5f,-0.5f,0.5f}, {0,0,1}, {1,0} },
        { {0.5f, 0.5f,0.5f}, {0,0,1}, {1,1} }, { {-0.5f, 0.5f,0.5f}, {0,0,1}, {0,1} },
        // -Z
        { {0.5f,-0.5f,-0.5f}, {0,0,-1}, {0,0} }, { {-0.5f,-0.5f,-0.5f}, {0,0,-1}, {1,0} },
        { {-0.5f, 0.5f,-0.5f}, {0,0,-1}, {1,1} }, { {0.5f, 0.5f,-0.5f}, {0,0,-1}, {0,1} },
    };

    indices.clear();
    for (uint32_t face = 0; face < 6; ++face) {
        uint32_t base = face * 4;
        indices.insert(indices.end(), { base, base+1, base+2, base, base+2, base+3 });
    }
}

std::string NextName(const std::string& base) {
    static std::unordered_map<std::string, int> counters;
    return base + std::to_string(++counters[base]);
}

TNode* SpawnCubeNode() {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    GetDefaultCubeMesh(vertices, indices);

    TNode* node = new TNode(nullptr, NextName("Cube"));
    node->AddComponent<MeshComponent>(vertices, indices);

    auto material = std::make_shared<Material>(ResourceManager::LoadShader("pbr"));
    node->AddComponent<MaterialComponent>(material);
    return node;
}

TNode* SpawnCameraNode() {
    TNode* node = new TNode(nullptr, NextName("Camera"));
    node->AddComponent<CameraComponent>(node);
    return node;
}

TNode* SpawnLightNode(LightType type) {
    const char* base = type == LightType::Directional ? "DirectionalLight"
                      : type == LightType::Point ? "PointLight" : "SpotLight";
    TNode* node = new TNode(nullptr, NextName(base));
    node->AddComponent<LightComponent>(node, type);
    return node;
}

} // namespace

TNode* DebugUI::m_selectedNode = nullptr;
bool DebugUI::m_showDeleteConfirm = false;
std::string DebugUI::m_sceneToDelete = "";

TNode* DebugUI::m_nodeToDelete = nullptr;
bool DebugUI::m_showNodeDeleteConfirm = false;

TNode* DebugUI::m_renamingNode = nullptr;
char DebugUI::m_renameBuffer[256] = "";
bool DebugUI::m_renameJustStarted = false;

GizmoMode DebugUI::m_gizmoMode = GizmoMode::Move;
GizmoHandle DebugUI::m_activeHandle = GizmoHandle::None;
glm::vec3 DebugUI::m_dragStartPointOnAxis = glm::vec3(0.0f);
float DebugUI::m_dragStartAngle = 0.0f;
glm::vec3 DebugUI::m_dragStartLocalPosition = glm::vec3(0.0f);
glm::vec3 DebugUI::m_dragStartLocalRotation = glm::vec3(0.0f);
glm::vec3 DebugUI::m_dragStartLocalScale = glm::vec3(1.0f);
TNode* DebugUI::m_dragNode = nullptr;

void DebugUI::Init() {
    // ImGui context is already created by OpenGLRenderer
}

void DebugUI::Shutdown() {
    m_selectedNode = nullptr;
}

namespace {

bool RayIntersectsAABB(const glm::vec3& origin, const glm::vec3& direction,
                       const glm::vec3& boxMin, const glm::vec3& boxMax, float& outT) {
    float tNear = -std::numeric_limits<float>::max();
    float tFar = std::numeric_limits<float>::max();

    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < 1e-8f) {
            if (origin[axis] < boxMin[axis] || origin[axis] > boxMax[axis]) return false;
            continue;
        }
        float t0 = (boxMin[axis] - origin[axis]) / direction[axis];
        float t1 = (boxMax[axis] - origin[axis]) / direction[axis];
        if (t0 > t1) std::swap(t0, t1);
        tNear = std::max(tNear, t0);
        tFar = std::min(tFar, t1);
        if (tNear > tFar) return false;
    }

    if (tFar < 0.0f) return false;
    outT = tNear;
    return true;
}

// Closest point on the infinite ray to the infinite line (linePoint + t*lineDir).
// Returns the ray parameter and the world-space point on the LINE (not the ray).
glm::vec3 ClosestPointRayToLine(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                                const glm::vec3& linePoint, const glm::vec3& lineDir,
                                float* outRayT = nullptr) {
    glm::vec3 w0 = rayOrigin - linePoint;
    float a = glm::dot(rayDir, rayDir);
    float b = glm::dot(rayDir, lineDir);
    float c = glm::dot(lineDir, lineDir);
    float d = glm::dot(rayDir, w0);
    float e = glm::dot(lineDir, w0);

    float denom = a * c - b * b;
    float rayT = 0.0f;
    float lineT = 0.0f;
    if (std::abs(denom) > 1e-8f) {
        rayT = (b * e - c * d) / denom;
        lineT = (a * e - b * d) / denom;
    }

    if (outRayT) *outRayT = rayT;
    return linePoint + lineDir * lineT;
}

bool RayIntersectsPlane(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                        const glm::vec3& planePoint, const glm::vec3& planeNormal,
                        glm::vec3& outHit) {
    float denom = glm::dot(rayDir, planeNormal);
    if (std::abs(denom) < 1e-4f) return false;
    float t = glm::dot(planePoint - rayOrigin, planeNormal) / denom;
    if (t < 0.0f) return false;
    outHit = rayOrigin + rayDir * t;
    return true;
}

void WalkMeshCandidates(TNode* node, const glm::vec3& origin, const glm::vec3& direction,
                        TNode*& closestNode, float& closestT) {
    if (!node) return;

    if (auto* mesh = node->GetComponent<MeshComponent>()) {
        glm::vec3 localMin, localMax;
        mesh->GetLocalBounds(localMin, localMax);

        glm::mat4 model = node->getModelMatrix();
        glm::vec3 worldMin(std::numeric_limits<float>::max());
        glm::vec3 worldMax(-std::numeric_limits<float>::max());
        for (int i = 0; i < 8; ++i) {
            glm::vec3 corner(
                (i & 1) ? localMax.x : localMin.x,
                (i & 2) ? localMax.y : localMin.y,
                (i & 4) ? localMax.z : localMin.z);
            glm::vec3 worldCorner = glm::vec3(model * glm::vec4(corner, 1.0f));
            worldMin = glm::min(worldMin, worldCorner);
            worldMax = glm::max(worldMax, worldCorner);
        }

        float t;
        if (RayIntersectsAABB(origin, direction, worldMin, worldMax, t) && t < closestT) {
            closestT = t;
            closestNode = node;
        }
    }

    for (TNode* child : node->children) {
        WalkMeshCandidates(child, origin, direction, closestNode, closestT);
    }
}

} // namespace

void DebugUI::ComputePickRay(Scene* activeScene, glm::vec3& outOrigin, glm::vec3& outDirection) {
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    float width = static_cast<float>(Viewport::GetWidth());
    float height = static_cast<float>(Viewport::GetHeight());

    float ndcX = width > 0.0f ? (2.0f * mousePos.x) / width - 1.0f : 0.0f;
    float ndcY = height > 0.0f ? 1.0f - (2.0f * mousePos.y) / height : 0.0f;

    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
    glm::mat4 projection = glm::perspective(glm::radians(60.0f), Viewport::GetAspectRatio(), 0.1f, 100.0f);
    if (TNode* cameraNode = activeScene->GetMainCamera()) {
        if (auto* camera = cameraNode->GetComponent<CameraComponent>()) {
            view = camera->GetViewMatrix();
            projection = camera->GetProjectionMatrix(Viewport::GetAspectRatio());
        }
    }

    glm::mat4 invVP = glm::inverse(projection * view);
    glm::vec4 ndcNear(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 ndcFar(ndcX, ndcY, 1.0f, 1.0f);

    glm::vec4 worldNear = invVP * ndcNear;
    worldNear /= worldNear.w;
    glm::vec4 worldFar = invVP * ndcFar;
    worldFar /= worldFar.w;

    outOrigin = glm::vec3(worldNear);
    outDirection = glm::normalize(glm::vec3(worldFar - worldNear));
}

TNode* DebugUI::PickAtCursor(Scene* activeScene) {
    if (!activeScene) return nullptr;
    float width = static_cast<float>(Viewport::GetWidth());
    float height = static_cast<float>(Viewport::GetHeight());
    if (width <= 0.0f || height <= 0.0f) return nullptr;

    glm::vec3 origin, direction;
    ComputePickRay(activeScene, origin, direction);

    TNode* closestNode = nullptr;
    float closestT = std::numeric_limits<float>::max();

    auto testCandidate = [&](TNode* node, const glm::vec3& center, float radius) {
        glm::vec3 oc = center - origin;
        float tca = glm::dot(oc, direction);
        if (tca < 0.0f) return;
        float d2 = glm::dot(oc, oc) - tca * tca;
        if (d2 > radius * radius) return;
        if (tca < closestT) {
            closestT = tca;
            closestNode = node;
        }
    };

    for (TNode* lightNode : activeScene->GetLights()) {
        if (auto* light = lightNode->GetComponent<LightComponent>()) {
            testCandidate(lightNode, light->GetPosition(), GizmoRenderer::kLightGizmoRadius);
        }
    }

    for (TNode* cameraNode : activeScene->GetCameras()) {
        testCandidate(cameraNode, cameraNode->getGlobalPosition(), GizmoRenderer::kCameraGizmoRadius);
    }

    WalkMeshCandidates(activeScene->GetRoot(), origin, direction, closestNode, closestT);

    return closestNode;
}

void DebugUI::SelectNode(TNode* node) {
    m_selectedNode = node;
    m_gizmoMode = GizmoMode::Move;
}

namespace {

glm::vec3 WorldAxisDirection(TNode* node, int axis) {
    glm::vec3 local(axis == 0 ? 1.0f : 0.0f, axis == 1 ? 1.0f : 0.0f, axis == 2 ? 1.0f : 0.0f);
    glm::vec4 world = node->getModelMatrix() * glm::vec4(local, 0.0f);
    return glm::normalize(glm::vec3(world));
}

} // namespace

GizmoHandle DebugUI::PickGizmoHandle(Scene* activeScene) {
    TNode* node = m_selectedNode;
    if (!node || !activeScene) return GizmoHandle::None;

    glm::vec3 origin, direction;
    ComputePickRay(activeScene, origin, direction);

    glm::vec3 nodePos = node->getGlobalPosition();
    glm::vec3 cameraPos = origin;
    float scale = GizmoRenderer::ComputeGizmoScale(nodePos, cameraPos);
    float armLength = GizmoRenderer::kGizmoArmLength * scale;
    float ringRadius = GizmoRenderer::kGizmoRingRadius * scale;
    float tolerance = GizmoRenderer::kGizmoPickTolerance * scale;

    GizmoMode mode = m_gizmoMode;
    GizmoHandle bestHandle = GizmoHandle::None;
    float bestT = std::numeric_limits<float>::max();

    for (int axis = 0; axis < 3; ++axis) {
        glm::vec3 axisDir = WorldAxisDirection(node, axis);

        if (mode == GizmoMode::Move || mode == GizmoMode::Scale) {
            float rayT;
            glm::vec3 closestOnLine = ClosestPointRayToLine(origin, direction, nodePos, axisDir, &rayT);
            if (rayT < 0.0f) continue;

            float along = glm::dot(closestOnLine - nodePos, axisDir);
            along = std::max(0.0f, std::min(along, armLength));
            glm::vec3 clampedPoint = nodePos + axisDir * along;

            glm::vec3 rayPoint = origin + direction * rayT;
            float dist = glm::length(rayPoint - clampedPoint);
            if (dist <= tolerance && rayT < bestT) {
                bestT = rayT;
                bestHandle = mode == GizmoMode::Move
                    ? static_cast<GizmoHandle>(static_cast<int>(GizmoHandle::MoveX) + axis)
                    : static_cast<GizmoHandle>(static_cast<int>(GizmoHandle::ScaleX) + axis);
            }
        } else if (mode == GizmoMode::Rotate) {
            glm::vec3 hit;
            if (!RayIntersectsPlane(origin, direction, nodePos, axisDir, hit)) continue;

            float distFromCenter = glm::length(hit - nodePos);
            if (std::abs(distFromCenter - ringRadius) <= tolerance) {
                float rayT = glm::dot(hit - origin, direction);
                if (rayT < bestT) {
                    bestT = rayT;
                    bestHandle = static_cast<GizmoHandle>(static_cast<int>(GizmoHandle::RotateX) + axis);
                }
            }
        }
    }

    return bestHandle;
}

void DebugUI::BeginGizmoDrag(GizmoHandle handle, Scene* activeScene) {
    TNode* node = m_selectedNode;
    if (!node || !activeScene || handle == GizmoHandle::None) return;

    m_activeHandle = handle;
    m_dragNode = node;
    m_dragStartLocalPosition = node->transform.position;
    m_dragStartLocalRotation = node->transform.rotation;
    m_dragStartLocalScale = node->transform.scale;

    glm::vec3 origin, direction;
    ComputePickRay(activeScene, origin, direction);
    glm::vec3 nodePos = node->getGlobalPosition();

    int axis;
    if (handle == GizmoHandle::MoveX || handle == GizmoHandle::MoveY || handle == GizmoHandle::MoveZ) {
        axis = static_cast<int>(handle) - static_cast<int>(GizmoHandle::MoveX);
    } else if (handle == GizmoHandle::ScaleX || handle == GizmoHandle::ScaleY || handle == GizmoHandle::ScaleZ) {
        axis = static_cast<int>(handle) - static_cast<int>(GizmoHandle::ScaleX);
    } else {
        axis = static_cast<int>(handle) - static_cast<int>(GizmoHandle::RotateX);
    }

    glm::vec3 axisDir = WorldAxisDirection(node, axis);

    if (handle == GizmoHandle::MoveX || handle == GizmoHandle::MoveY || handle == GizmoHandle::MoveZ ||
        handle == GizmoHandle::ScaleX || handle == GizmoHandle::ScaleY || handle == GizmoHandle::ScaleZ) {
        m_dragStartPointOnAxis = ClosestPointRayToLine(origin, direction, nodePos, axisDir);
    } else {
        glm::vec3 hit;
        if (RayIntersectsPlane(origin, direction, nodePos, axisDir, hit)) {
            glm::vec3 toHit = hit - nodePos;
            glm::vec3 basisA, basisB;
            glm::vec3 arbitrary = std::abs(axisDir.x) < 0.9f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
            basisA = glm::normalize(glm::cross(axisDir, arbitrary));
            basisB = glm::cross(axisDir, basisA);
            m_dragStartAngle = std::atan2(glm::dot(toHit, basisB), glm::dot(toHit, basisA));
        }
    }
}

void DebugUI::UpdateGizmoDrag(Scene* activeScene) {
    TNode* node = m_dragNode;
    if (!node || !activeScene || m_activeHandle == GizmoHandle::None) return;

    glm::vec3 origin, direction;
    ComputePickRay(activeScene, origin, direction);
    glm::vec3 nodePos = node->getGlobalPosition();

    TNode* parent = node->parent;
    glm::mat4 parentModel = parent ? parent->getModelMatrix() : glm::mat4(1.0f);
    glm::mat4 parentInverse = glm::inverse(parentModel);

    if (m_activeHandle == GizmoHandle::MoveX || m_activeHandle == GizmoHandle::MoveY || m_activeHandle == GizmoHandle::MoveZ) {
        int axis = static_cast<int>(m_activeHandle) - static_cast<int>(GizmoHandle::MoveX);
        glm::vec3 axisDir = WorldAxisDirection(node, axis);

        glm::vec3 currentPoint = ClosestPointRayToLine(origin, direction, nodePos, axisDir);
        glm::vec3 worldDelta = currentPoint - m_dragStartPointOnAxis;

        glm::vec3 localDelta = glm::vec3(parentInverse * glm::vec4(worldDelta, 0.0f));
        node->transform.position = m_dragStartLocalPosition + localDelta;
    } else if (m_activeHandle == GizmoHandle::ScaleX || m_activeHandle == GizmoHandle::ScaleY || m_activeHandle == GizmoHandle::ScaleZ) {
        int axis = static_cast<int>(m_activeHandle) - static_cast<int>(GizmoHandle::ScaleX);
        glm::vec3 axisDir = WorldAxisDirection(node, axis);

        glm::vec3 currentPoint = ClosestPointRayToLine(origin, direction, nodePos, axisDir);
        glm::vec3 worldDelta = currentPoint - m_dragStartPointOnAxis;
        float signedDistance = glm::dot(worldDelta, axisDir);

        constexpr float kScaleSensitivity = 1.0f;
        float multiplier = 1.0f + signedDistance * kScaleSensitivity;

        glm::vec3 newScale = m_dragStartLocalScale;
        float* scaleAxis = &newScale.x + axis;
        float* startAxis = &m_dragStartLocalScale.x + axis;
        *scaleAxis = std::max(0.01f, (*startAxis) * multiplier);
        node->transform.scale = newScale;
    } else {
        int axis = static_cast<int>(m_activeHandle) - static_cast<int>(GizmoHandle::RotateX);
        glm::vec3 axisDir = WorldAxisDirection(node, axis);

        glm::vec3 hit;
        if (RayIntersectsPlane(origin, direction, nodePos, axisDir, hit)) {
            glm::vec3 toHit = hit - nodePos;
            glm::vec3 arbitrary = std::abs(axisDir.x) < 0.9f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
            glm::vec3 basisA = glm::normalize(glm::cross(axisDir, arbitrary));
            glm::vec3 basisB = glm::cross(axisDir, basisA);
            float currentAngle = std::atan2(glm::dot(toHit, basisB), glm::dot(toHit, basisA));

            float angleDelta = currentAngle - m_dragStartAngle;

            glm::vec3 newRotation = m_dragStartLocalRotation;
            float* rotAxis = &newRotation.x + axis;
            *rotAxis += glm::degrees(angleDelta);
            node->transform.rotation = newRotation;
        }
    }
}

void DebugUI::DrawFrame(SceneManager* sceneManager) {
    if (!sceneManager) return;

    Scene* activeScene = sceneManager->GetActiveScene();
    if (!activeScene) return;

    if (!ImGui::GetIO().WantCaptureMouse) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_activeHandle == GizmoHandle::None) {
            GizmoHandle handle = PickGizmoHandle(activeScene);
            if (handle != GizmoHandle::None) {
                BeginGizmoDrag(handle, activeScene);
            } else {
                TNode* picked = PickAtCursor(activeScene);
                SelectNode(picked);
                if (picked) {
                    ImGui::SetWindowFocus("Inspector");
                }
            }
        } else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && m_activeHandle != GizmoHandle::None) {
            UpdateGizmoDrag(activeScene);
        }
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && m_activeHandle != GizmoHandle::None) {
        m_activeHandle = GizmoHandle::None;
        m_dragNode = nullptr;
    }

    if (!ImGui::GetIO().WantTextInput && m_selectedNode) {
        if (ImGui::IsKeyPressed(ImGuiKey_W)) m_gizmoMode = GizmoMode::Move;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) m_gizmoMode = GizmoMode::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_gizmoMode = GizmoMode::Scale;
    }

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500, 700), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Scene Debug", nullptr, ImGuiWindowFlags_NoMove)) {
        // Scene Selector at top
        DrawSceneSelector(sceneManager);
        ImGui::Separator();

        if (ImGui::BeginTabBar("DebugTabs")) {
            // Tab 1: Scene Tree
            if (ImGui::BeginTabItem("Scene Tree")) {
                ImGui::Text("Scene Hierarchy:");
                ImGui::Separator();

                ImGui::BeginChild("SceneTreeChild", ImVec2(0, 400), true);
                TNode* root = activeScene->GetRoot();
                if (root) {
                    for (TNode* child : root->children) {
                        DrawSceneTree(child, activeScene);
                    }
                }

                if (ImGui::BeginPopupContextWindow("SceneRootContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                    if (root) {
                        DrawCreateMenu(root, activeScene);

                        ImGui::Separator();
                        if (ImGui::MenuItem("Add Empty Object")) {
                            TNode* empty = new TNode(nullptr, "Empty");
                            root->addChild(empty);
                            SelectNode(empty);
                        }
                    }

                    ImGui::EndPopup();
                }

                ImGui::EndChild();

                ImGui::EndTabItem();
            }

            // Tab 2: Resources
            if (ImGui::BeginTabItem("Resources")) {
                ImGui::Text("Loaded Resources:");
                ImGui::Separator();

                ImGui::BeginChild("ResourcesChild", ImVec2(0, 400), true);
                DrawResourcesTree();
                ImGui::EndChild();

                ImGui::EndTabItem();
            }

            // Tab 3: Camera
            if (ImGui::BeginTabItem("Camera")) {
                ImGui::BeginChild("CameraChild", ImVec2(0, 400), true);
                DrawCameraTab(sceneManager);
                ImGui::EndChild();

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    DrawInspector(activeScene);
    DrawDeleteConfirmation();
    DrawNodeDeleteConfirmation();
}

std::string DebugUI::DescribeNode(TNode* node) {
    if (!node) return "";

    if (node->components.empty()) {
        return "Group";
    }

    std::string desc;
    for (const auto& c : node->components) {
        if (!desc.empty()) desc += ", ";

        if (dynamic_cast<MeshComponent*>(c.get())) {
            desc += "Mesh";
        } else if (dynamic_cast<MaterialComponent*>(c.get())) {
            desc += "Material";
        } else if (dynamic_cast<CameraComponent*>(c.get())) {
            desc += "Camera";
        } else if (auto* light = dynamic_cast<LightComponent*>(c.get())) {
            switch (light->type) {
                case LightType::Directional: desc += "Light(Dir)"; break;
                case LightType::Point:       desc += "Light(Point)"; break;
                case LightType::Spot:        desc += "Light(Spot)"; break;
            }
        } else {
            desc += "Component";
        }
    }
    return desc;
}

void DebugUI::DrawSceneTree(TNode* node, Scene* activeScene, int depth) {
    if (!node) return;

    std::string idSuffix = "##" + std::to_string(reinterpret_cast<uintptr_t>(node));

    if (m_renamingNode == node) {
        if (m_renameJustStarted) {
            ImGui::SetKeyboardFocusHere();
            m_renameJustStarted = false;
        }
        ImGui::SetNextItemWidth(200);
        bool commit = ImGui::InputText(("##rename" + idSuffix).c_str(), m_renameBuffer,
            sizeof(m_renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        if (commit) {
            node->name = m_renameBuffer;
            m_renamingNode = nullptr;
        } else if (ImGui::IsItemDeactivated()) {
            m_renamingNode = nullptr;
        }
        return;
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    if (node->children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    if (node == m_selectedNode) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    std::string label = node->name.empty() ? "Unnamed" : node->name;
    label += "  [" + DescribeNode(node) + "]";
    label += idSuffix;

    bool opened = ImGui::TreeNodeEx(label.c_str(), flags);

    if (ImGui::IsItemClicked()) {
        SelectNode(node);
    }

    if (ImGui::BeginPopupContextItem(("NodeContextMenu" + idSuffix).c_str())) {
        SelectNode(node);

        if (ImGui::MenuItem("Rename")) {
            m_renamingNode = node;
            m_renameJustStarted = true;
            std::string current = node->name.empty() ? "Unnamed" : node->name;
            std::snprintf(m_renameBuffer, sizeof(m_renameBuffer), "%s", current.c_str());
        }

        if (ImGui::MenuItem("Delete")) {
            m_nodeToDelete = node;
            m_showNodeDeleteConfirm = true;
        }

        ImGui::Separator();

        DrawCreateMenu(node, activeScene);

        if (ImGui::MenuItem("Add Empty Object")) {
            TNode* empty = new TNode(nullptr, "Empty");
            node->addChild(empty);
            SelectNode(empty);
        }

        ImGui::Separator();
        DrawAddComponentMenu(node, activeScene);

        ImGui::EndPopup();
    }

    if (m_selectedNode == node && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
        && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        m_nodeToDelete = node;
        m_showNodeDeleteConfirm = true;
    }

    if (opened) {
        for (TNode* child : node->children) {
            DrawSceneTree(child, activeScene, depth + 1);
        }
        ImGui::TreePop();
    }
}

void DebugUI::DrawResourcesTree() {
    ImGui::Text("Shaders:");
    ImGui::Separator();

    const auto& shaders = ResourceManager::GetAllShaders();
    ImGui::BulletText("Shaders Loaded: %zu", shaders.size());
    ImGui::Indent();

    for (const auto& [name, shader] : shaders) {
        if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_Leaf)) {
            ImGui::BulletText("Vertex: resources/shaders/%s.vert", name.c_str());
            ImGui::BulletText("Fragment: resources/shaders/%s.frag", name.c_str());
            ImGui::TreePop();
        }
    }

    ImGui::Unindent();

    ImGui::Spacing();
    ImGui::Text("Memory Usage:");
    ImGui::Separator();
    ImGui::BulletText("Shaders Loaded: %zu", shaders.size());

    int totalShaderFiles = 0;
    std::filesystem::path shadersDir("resources/shaders");
    if (std::filesystem::exists(shadersDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(shadersDir)) {
            if (entry.path().extension() == ".vert" || entry.path().extension() == ".frag") {
                totalShaderFiles++;
            }
        }
    }
    ImGui::BulletText("Total Shader Files: %d", totalShaderFiles);
}

void DebugUI::DrawNodeProperties(TNode* node) {
    if (!node) return;

    ImGui::Text("Node Type: %s", node->components.empty() ? "Group" : "Entity");

    ImGui::Spacing();
    ImGui::Text("Transform:");
    ImGui::Separator();

    ImGui::DragFloat3("Position##transform", &node->transform.position.x, 0.1f);
    ImGui::DragFloat3("Rotation##transform", &node->transform.rotation.x, 1.0f);
    ImGui::DragFloat3("Scale##transform", &node->transform.scale.x, 0.1f);

    ImGui::Spacing();
    ImGui::Text("Hierarchy:");
    ImGui::Separator();

    ImGui::Text("Parent: %s", node->parent ? "Yes" : "None");
    ImGui::Text("Children: %zu", node->children.size());

    ImGui::Spacing();
    ImGui::Text("Bounding Box:");
    ImGui::Separator();

    ImGui::Text("Has BoundingBox: %s", node->boundingBox ? "Yes" : "No");

    if (node->boundingBox) {
        ImGui::BulletText("Type: Sphere or AABB");
    }

    ImGui::Spacing();
    ImGui::Text("Global Position: (%.2f, %.2f, %.2f)",
        node->getGlobalPosition().x,
        node->getGlobalPosition().y,
        node->getGlobalPosition().z);
}

void DebugUI::DrawInspector(Scene* activeScene) {
    if (!m_selectedNode) return;

    ImGui::SetNextWindowPos(ImVec2(520, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 700), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Inspector", nullptr)) {
        ImGui::Text("Node: %s", m_selectedNode->name.empty() ? "Unnamed" : m_selectedNode->name.c_str());
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Gizmo Mode:");
        if (ImGui::RadioButton("Move (W)", m_gizmoMode == GizmoMode::Move)) m_gizmoMode = GizmoMode::Move;
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate (E)", m_gizmoMode == GizmoMode::Rotate)) m_gizmoMode = GizmoMode::Rotate;
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale (R)", m_gizmoMode == GizmoMode::Scale)) m_gizmoMode = GizmoMode::Scale;

        ImGui::Text("Transform:");
        ImGui::Separator();
        ImGui::DragFloat3("Position##inspector", &m_selectedNode->transform.position.x, 0.1f);
        ImGui::DragFloat3("Rotation##inspector", &m_selectedNode->transform.rotation.x, 1.0f);
        ImGui::DragFloat3("Scale##inspector", &m_selectedNode->transform.scale.x, 0.1f);

        ImGui::Spacing();
        ImGui::Text("Components: %zu", m_selectedNode->components.size());
        ImGui::Separator();

        if (m_selectedNode->components.empty()) {
            ImGui::BulletText("No components (Group Node)");
        }

        if (auto* mesh = m_selectedNode->GetComponent<MeshComponent>()) {
            ImGui::Text("Mesh:");
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove##mesh")) {
                m_selectedNode->RemoveComponent<MeshComponent>();
            } else {
                ImGui::Indent();
                ImGui::BulletText("Vertices: %zu", mesh->GetVertexCount());
                ImGui::BulletText("Indices: %zu", mesh->GetIndexCount());
                ImGui::BulletText("Triangles: %zu", mesh->GetIndexCount() / 3);
                ImGui::Unindent();
            }
        }

        if (auto* materialComp = m_selectedNode->GetComponent<MaterialComponent>()) {
            ImGui::Spacing();
            ImGui::Text("Material:");
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove##material")) {
                m_selectedNode->RemoveComponent<MaterialComponent>();
            } else {
                ImGui::Indent();
                if (const auto& mat = materialComp->material) {
                    auto shader = mat->GetShader();
                    ImGui::BulletText("Shader: %s", shader ? shader->GetName().c_str() : "None");
                    ImGui::BulletText("Base Color: (%.2f, %.2f, %.2f, %.2f)",
                        mat->baseColor.r, mat->baseColor.g, mat->baseColor.b, mat->baseColor.a);
                    ImGui::BulletText("Albedo Texture: %s", mat->albedo ? "Yes" : "No");
                    if (mat->albedo) {
                        ImGui::Indent();
                        ImGui::BulletText("%dx%d", mat->albedo->GetWidth(), mat->albedo->GetHeight());
                        ImGui::Unindent();
                    }
                    ImGui::BulletText("Normal Texture: %s", mat->normal ? "Yes" : "No");
                    ImGui::BulletText("MetallicRoughness Texture: %s", mat->metallicRoughness ? "Yes" : "No");
                } else {
                    ImGui::BulletText("No material resource");
                }
                ImGui::Unindent();
            }
        }

        if (auto* camera = m_selectedNode->GetComponent<CameraComponent>()) {
            ImGui::Spacing();
            ImGui::Text("Camera:");
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove##camera")) {
                if (activeScene) {
                    activeScene->UnregisterCamera(m_selectedNode);
                }
                m_selectedNode->RemoveComponent<CameraComponent>();
            } else {
                ImGui::Indent();
                ImGui::BulletText("FOV: %.1f", camera->fov);
                ImGui::BulletText("Yaw/Pitch: %.1f / %.1f", camera->yaw, camera->pitch);
                ImGui::BulletText("Near/Far: %.2f / %.2f", camera->nearPlane, camera->farPlane);
                ImGui::Unindent();
            }
        }

        if (auto* light = m_selectedNode->GetComponent<LightComponent>()) {
            const char* typeName = light->type == LightType::Directional ? "Directional"
                                  : light->type == LightType::Point ? "Point" : "Spot";

            ImGui::Spacing();
            ImGui::Text("Light (%s):", typeName);
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove##light")) {
                if (activeScene) {
                    activeScene->UnregisterLight(m_selectedNode);
                }
                m_selectedNode->RemoveComponent<LightComponent>();
            } else {
                ImGui::Indent();
                ImGui::ColorEdit3("Color##light", &light->color.x);
                ImGui::DragFloat("Intensity##light", &light->intensity, 0.05f, 0.0f, 100.0f);
                if (light->type != LightType::Directional) {
                    ImGui::DragFloat("Range##light", &light->range, 0.1f, 0.01f, 1000.0f);
                }
                if (light->type == LightType::Spot) {
                    ImGui::DragFloat("Inner Cone##light", &light->innerConeDegrees, 0.5f, 0.0f, light->outerConeDegrees - 0.1f);
                    ImGui::DragFloat("Outer Cone##light", &light->outerConeDegrees, 0.5f, light->innerConeDegrees + 0.1f, 89.0f);
                }
                ImGui::Unindent();
            }
        }

        ImGui::Spacing();
        ImGui::Text("Hierarchy:");
        ImGui::Separator();
        ImGui::BulletText("Parent: %s", m_selectedNode->parent ? (m_selectedNode->parent->name.empty() ? "Root" : m_selectedNode->parent->name.c_str()) : "None");
        ImGui::BulletText("Children: %zu", m_selectedNode->children.size());
        if (!m_selectedNode->children.empty()) {
            ImGui::Indent();
            for (TNode* child : m_selectedNode->children) {
                ImGui::BulletText("%s", child->name.empty() ? "Unnamed" : child->name.c_str());
            }
            ImGui::Unindent();
        }

        ImGui::Spacing();
        ImGui::Text("Global Position: (%.2f, %.2f, %.2f)",
            m_selectedNode->getGlobalPosition().x,
            m_selectedNode->getGlobalPosition().y,
            m_selectedNode->getGlobalPosition().z);
    }
    ImGui::End();
}

void DebugUI::DrawDeleteConfirmation() {
    if (!m_showDeleteConfirm) return;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, -1), ImGuiCond_FirstUseEver);

    bool open = true;
    if (ImGui::Begin("Delete Scene?", &open, ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Are you sure you want to delete:\n\n\"%s\"\n\nThis action cannot be undone.", m_sceneToDelete.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float buttonWidth = 140.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float totalWidth = (buttonWidth * 2) + spacing;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - totalWidth) * 0.5f);

        if (ImGui::Button("Delete Forever", ImVec2(buttonWidth, 0))) {
            SceneManager::Instance().UnloadScene(m_sceneToDelete);
            m_sceneToDelete = "";
            m_showDeleteConfirm = false;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            m_showDeleteConfirm = false;
            m_sceneToDelete = "";
        }

        ImGui::End();
    }

    if (!open) {
        m_showDeleteConfirm = false;
    }
}

void DebugUI::DeleteNode(TNode* node, Scene* activeScene) {
    if (!node) return;

    if (activeScene) {
        // Unregister any cameras/lights anywhere in this node's subtree before it's freed.
        std::vector<TNode*> stack = { node };
        while (!stack.empty()) {
            TNode* current = stack.back();
            stack.pop_back();
            if (current->GetComponent<CameraComponent>()) {
                activeScene->UnregisterCamera(current);
            }
            if (current->GetComponent<LightComponent>()) {
                activeScene->UnregisterLight(current);
            }
            for (TNode* child : current->children) {
                stack.push_back(child);
            }
        }
    }

    if (m_selectedNode == node) {
        m_selectedNode = nullptr;
    }
    if (m_renamingNode == node) {
        m_renamingNode = nullptr;
    }

    node->removeFromParent();
    delete node;
}

void DebugUI::DrawAddComponentMenu(TNode* node, Scene* activeScene) {
    if (!node) return;

    if (ImGui::BeginMenu("Add Component")) {
        if (!node->GetComponent<MeshComponent>() && ImGui::MenuItem("Mesh")) {
            std::vector<MeshVertex> vertices;
            std::vector<uint32_t> indices;
            GetDefaultCubeMesh(vertices, indices);
            node->AddComponent<MeshComponent>(vertices, indices);

            if (!node->GetComponent<MaterialComponent>()) {
                auto material = std::make_shared<Material>(ResourceManager::LoadShader("pbr"));
                node->AddComponent<MaterialComponent>(material);
            }
        }

        if (!node->GetComponent<MaterialComponent>() && ImGui::MenuItem("Material")) {
            auto material = std::make_shared<Material>(ResourceManager::LoadShader("pbr"));
            node->AddComponent<MaterialComponent>(material);
        }

        if (!node->GetComponent<CameraComponent>() && ImGui::MenuItem("Camera")) {
            node->AddComponent<CameraComponent>(node);
            if (activeScene) {
                activeScene->RegisterCamera(node);
            }
        }

        if (!node->GetComponent<LightComponent>() && ImGui::BeginMenu("Light")) {
            if (ImGui::MenuItem("Directional")) {
                node->AddComponent<LightComponent>(node, LightType::Directional);
                if (activeScene) activeScene->RegisterLight(node);
            }
            if (ImGui::MenuItem("Point")) {
                node->AddComponent<LightComponent>(node, LightType::Point);
                if (activeScene) activeScene->RegisterLight(node);
            }
            if (ImGui::MenuItem("Spot")) {
                node->AddComponent<LightComponent>(node, LightType::Spot);
                if (activeScene) activeScene->RegisterLight(node);
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }
}

void DebugUI::DrawCreateMenu(TNode* parent, Scene* activeScene) {
    if (!parent) return;

    if (ImGui::MenuItem("Cube")) {
        TNode* node = SpawnCubeNode();
        parent->addChild(node);
        SelectNode(node);
    }

    if (ImGui::MenuItem("Camera")) {
        TNode* node = SpawnCameraNode();
        parent->addChild(node);
        if (activeScene) activeScene->RegisterCamera(node);
        SelectNode(node);
    }

    if (ImGui::BeginMenu("Light")) {
        if (ImGui::MenuItem("Directional")) {
            TNode* node = SpawnLightNode(LightType::Directional);
            parent->addChild(node);
            if (activeScene) activeScene->RegisterLight(node);
            SelectNode(node);
        }
        if (ImGui::MenuItem("Point")) {
            TNode* node = SpawnLightNode(LightType::Point);
            parent->addChild(node);
            if (activeScene) activeScene->RegisterLight(node);
            SelectNode(node);
        }
        if (ImGui::MenuItem("Spot")) {
            TNode* node = SpawnLightNode(LightType::Spot);
            parent->addChild(node);
            if (activeScene) activeScene->RegisterLight(node);
            SelectNode(node);
        }
        ImGui::EndMenu();
    }
}

void DebugUI::DrawNodeDeleteConfirmation() {
    if (!m_showNodeDeleteConfirm || !m_nodeToDelete) return;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, -1), ImGuiCond_FirstUseEver);

    std::string nodeName = m_nodeToDelete->name.empty() ? "Unnamed" : m_nodeToDelete->name;

    bool open = true;
    if (ImGui::Begin("Delete Node?", &open, ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Are you sure you want to delete:\n\n\"%s\"\n\nThis will also delete all of its children. This action cannot be undone.", nodeName.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float buttonWidth = 140.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float totalWidth = (buttonWidth * 2) + spacing;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - totalWidth) * 0.5f);

        if (ImGui::Button("Delete Forever##node", ImVec2(buttonWidth, 0))) {
            DeleteNode(m_nodeToDelete, SceneManager::Instance().GetActiveScene());
            m_nodeToDelete = nullptr;
            m_showNodeDeleteConfirm = false;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel##node", ImVec2(buttonWidth, 0))) {
            m_nodeToDelete = nullptr;
            m_showNodeDeleteConfirm = false;
        }

        ImGui::End();
    }

    if (!open) {
        m_nodeToDelete = nullptr;
        m_showNodeDeleteConfirm = false;
    }
}

void DebugUI::DrawCameraTab(SceneManager* sceneManager) {
    if (!sceneManager) return;

    Scene* activeScene = sceneManager->GetActiveScene();
    if (!activeScene) {
        ImGui::Text("No active scene.");
        return;
    }

    const auto& cameras = activeScene->GetCameras();
    if (cameras.empty()) {
        ImGui::Text("No cameras in this scene.");
        return;
    }

    TNode* mainCamera = activeScene->GetMainCamera();

    ImGui::Text("Cameras: %zu", cameras.size());
    ImGui::Separator();

    if (ImGui::BeginCombo("Main Camera", mainCamera && !mainCamera->name.empty() ? mainCamera->name.c_str() : "Unnamed")) {
        for (TNode* camNode : cameras) {
            bool isSelected = (camNode == mainCamera);
            std::string label = camNode->name.empty() ? "Unnamed" : camNode->name;
            label += "##" + std::to_string(reinterpret_cast<uintptr_t>(camNode));
            if (ImGui::Selectable(label.c_str(), isSelected)) {
                activeScene->SetMainCamera(camNode);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (!mainCamera) return;

    auto* camera = mainCamera->GetComponent<CameraComponent>();
    if (!camera) {
        ImGui::Text("Main camera node has no CameraComponent.");
        return;
    }

    ImGui::Spacing();
    ImGui::Text("Transform:");
    ImGui::Separator();
    ImGui::DragFloat3("Position##camera", &mainCamera->transform.position.x, 0.1f);
    ImGui::DragFloat("Yaw##camera", &camera->yaw, 0.5f);
    ImGui::DragFloat("Pitch##camera", &camera->pitch, 0.5f, -89.0f, 89.0f);

    ImGui::Spacing();
    ImGui::Text("Lens:");
    ImGui::Separator();
    ImGui::DragFloat("FOV##camera", &camera->fov, 0.5f, 1.0f, 170.0f);
    ImGui::DragFloat("Near Plane##camera", &camera->nearPlane, 0.01f, 0.001f, camera->farPlane - 0.01f);
    ImGui::DragFloat("Far Plane##camera", &camera->farPlane, 1.0f, camera->nearPlane + 0.01f, 10000.0f);

    ImGui::Spacing();
    ImGui::Text("Input:");
    ImGui::Separator();
    ImGui::DragFloat("Move Speed##camera", &camera->moveSpeed, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("Mouse Sensitivity##camera", &camera->mouseSensitivity, 0.01f, 0.01f, 5.0f);

    ImGui::Spacing();
    glm::vec3 forward = camera->GetForward();
    ImGui::Text("Forward: (%.2f, %.2f, %.2f)", forward.x, forward.y, forward.z);
}

void DebugUI::DrawSceneSelector(SceneManager* sceneManager) {
    if (!sceneManager) return;

    ImGui::Text("Scenes:");
    const auto& scenes = sceneManager->GetAllScenes();

    if (ImGui::BeginCombo("##SceneList", sceneManager->GetActiveSceneName().c_str())) {
        for (const auto& [name, scene] : scenes) {
            bool isSelected = (sceneManager->GetActiveSceneName() == name);
            if (ImGui::Selectable(name.c_str(), isSelected)) {
                sceneManager->LoadScene(name);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("New Scene##btn")) {
        static int sceneCounter = 1;
        std::string newSceneName = "Scene_" + std::to_string(sceneCounter++);
        sceneManager->CreateScene(newSceneName);
        sceneManager->LoadScene(newSceneName);
    }

    ImGui::SameLine();
    if (ImGui::Button("Save##btn")) {
        if (sceneManager->GetActiveScene()) {
            static int saveCounter = 0;
            std::string fileName = "scenes/" + sceneManager->GetActiveSceneName() + "_" + std::to_string(saveCounter++) + ".scene";
            std::filesystem::create_directories("scenes");
            SceneSerializer::SaveScene(sceneManager->GetActiveScene(), fileName);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Delete##btn")) {
        if (sceneManager->GetActiveScene()) {
            m_sceneToDelete = sceneManager->GetActiveSceneName();
            m_showDeleteConfirm = true;
            ImGui::OpenPopup("Delete Scene Confirmation");
        }
    }
}
