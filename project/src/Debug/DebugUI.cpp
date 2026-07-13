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
#include "Renderer/Skybox.h"
#include "ResourceManager/CubemapTexture.h"
#include "Debug/ProjectBrowser.h"
#include "Core/Stats.h"
#include "Core/EngineSettings.h"
#include "Core/EngineConfig.h"
#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <filesystem>
#include <cstdio>
#include <unordered_map>
#include <limits>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

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

// Spawn point in front of the active camera, with a small random jitter so
// repeated creates don't stack exactly on top of each other.
glm::vec3 ComputeSpawnWorldPosition(Scene* activeScene) {
    constexpr float kSpawnDistance = 5.0f;
    constexpr float kJitterRange = 0.5f;

    glm::vec3 basePos(0.0f, 0.0f, 0.0f);
    glm::vec3 forward(0.0f, 0.0f, -1.0f);

    if (activeScene) {
        if (TNode* cameraNode = activeScene->GetMainCamera()) {
            if (auto* camera = cameraNode->GetComponent<CameraComponent>()) {
                basePos = cameraNode->transform.position;
                forward = camera->GetForward();
            }
        }
    }

    glm::vec3 jitter(
        ((std::rand() % 1000) / 1000.0f - 0.5f) * 2.0f * kJitterRange,
        ((std::rand() % 1000) / 1000.0f - 0.5f) * 2.0f * kJitterRange,
        ((std::rand() % 1000) / 1000.0f - 0.5f) * 2.0f * kJitterRange);

    return basePos + forward * kSpawnDistance + jitter;
}

TNode* SpawnCubeNode() {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    GetDefaultCubeMesh(vertices, indices);

    TNode* node = new TNode(nullptr, NextName("Cube"));
    auto* mesh = node->AddComponent<MeshComponent>(vertices, indices);

    glm::vec3 localMin, localMax;
    mesh->GetLocalBounds(localMin, localMax);
    node->boundingBox = new AABB(localMin, localMax);

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

TNode* DebugUI::selectedNode = nullptr;
bool DebugUI::sceneSelected = false;
bool DebugUI::showDeleteConfirm = false;
std::string DebugUI::sceneToDelete = "";

TNode* DebugUI::nodeToDelete = nullptr;
bool DebugUI::showNodeDeleteConfirm = false;

std::vector<TNode*> DebugUI::multiSelectedNodes;
bool DebugUI::showMultiDeleteConfirm = false;

bool DebugUI::showSaveConfirm = false;
std::string DebugUI::sceneToSave = "";

TNode* DebugUI::renamingNode = nullptr;
char DebugUI::renameBuffer[256] = "";
bool DebugUI::renameJustStarted = false;

GizmoMode DebugUI::gizmoMode = GizmoMode::Move;
GizmoSpace DebugUI::gizmoSpace = GizmoSpace::Global;
GizmoHandle DebugUI::activeHandle = GizmoHandle::None;
glm::vec3 DebugUI::dragStartPointOnAxis = glm::vec3(0.0f);
float DebugUI::dragStartAngle = 0.0f;
glm::vec3 DebugUI::dragStartLocalPosition = glm::vec3(0.0f);
glm::vec3 DebugUI::dragStartLocalRotation = glm::vec3(0.0f);
glm::vec3 DebugUI::dragStartLocalScale = glm::vec3(1.0f);
TNode* DebugUI::dragNode = nullptr;
ImVec2 DebugUI::dragStartMousePos = ImVec2(0.0f, 0.0f);

char DebugUI::skyboxFolderBuffer[128] = "";
std::string DebugUI::skyboxLoadError = "";

char DebugUI::sceneTreeFilter[128] = "";

bool DebugUI::hasCopiedTransform = false;
Transform DebugUI::copiedTransform = Transform();
bool DebugUI::scaleAxisLocked[3] = { false, false, false };

float DebugUI::autoSaveTimer = 0.0f;
std::string DebugUI::lastAutoSaveStatus = "";

bool DebugUI::renamingScene = false;
char DebugUI::sceneRenameBuffer[128] = "";
std::string DebugUI::sceneRenameError = "";

void DebugUI::Init() {
    // ImGui context is already created by OpenGLRenderer
}

void DebugUI::ApplyTheme() {
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.IndentSpacing = 16.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;

    ImVec4* colors = style.Colors;
    const ImVec4 bgDarkest  = ImVec4(0.086f, 0.090f, 0.106f, 1.00f);
    const ImVec4 bgDark     = ImVec4(0.114f, 0.118f, 0.137f, 1.00f);
    const ImVec4 bgMid      = ImVec4(0.145f, 0.149f, 0.173f, 1.00f);
    const ImVec4 bgLight    = ImVec4(0.188f, 0.192f, 0.220f, 1.00f);
    const ImVec4 border     = ImVec4(0.243f, 0.247f, 0.278f, 1.00f);
    const ImVec4 textMain   = ImVec4(0.870f, 0.878f, 0.898f, 1.00f);
    const ImVec4 textDim    = ImVec4(0.520f, 0.533f, 0.573f, 1.00f);
    const ImVec4 accent     = ImVec4(0.259f, 0.588f, 0.980f, 1.00f);
    const ImVec4 accentDim  = ImVec4(0.259f, 0.588f, 0.980f, 0.50f);
    const ImVec4 accentHi   = ImVec4(0.380f, 0.680f, 1.000f, 1.00f);

    colors[ImGuiCol_Text]                  = textMain;
    colors[ImGuiCol_TextDisabled]          = textDim;
    colors[ImGuiCol_WindowBg]              = bgDark;
    colors[ImGuiCol_ChildBg]               = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_PopupBg]               = bgDarkest;
    colors[ImGuiCol_Border]                = border;
    colors[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_FrameBg]               = bgMid;
    colors[ImGuiCol_FrameBgHovered]        = bgLight;
    colors[ImGuiCol_FrameBgActive]         = accentDim;
    colors[ImGuiCol_TitleBg]               = bgDarkest;
    colors[ImGuiCol_TitleBgActive]         = bgDarkest;
    colors[ImGuiCol_TitleBgCollapsed]      = bgDarkest;
    colors[ImGuiCol_MenuBarBg]             = bgDarkest;
    colors[ImGuiCol_ScrollbarBg]           = bgDarkest;
    colors[ImGuiCol_ScrollbarGrab]         = bgLight;
    colors[ImGuiCol_ScrollbarGrabHovered]  = border;
    colors[ImGuiCol_ScrollbarGrabActive]   = accent;
    colors[ImGuiCol_CheckMark]             = accentHi;
    colors[ImGuiCol_SliderGrab]            = accent;
    colors[ImGuiCol_SliderGrabActive]      = accentHi;
    colors[ImGuiCol_Button]                = bgLight;
    colors[ImGuiCol_ButtonHovered]         = accentDim;
    colors[ImGuiCol_ButtonActive]          = accent;
    colors[ImGuiCol_Header]                = accentDim;
    colors[ImGuiCol_HeaderHovered]         = accent;
    colors[ImGuiCol_HeaderActive]          = accentHi;
    colors[ImGuiCol_Separator]             = border;
    colors[ImGuiCol_SeparatorHovered]      = accent;
    colors[ImGuiCol_SeparatorActive]       = accentHi;
    colors[ImGuiCol_ResizeGrip]            = ImVec4(accent.x, accent.y, accent.z, 0.25f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(accent.x, accent.y, accent.z, 0.55f);
    colors[ImGuiCol_ResizeGripActive]      = accent;
    colors[ImGuiCol_Tab]                   = bgDark;
    colors[ImGuiCol_TabHovered]            = accentDim;
    colors[ImGuiCol_TabSelected]           = bgMid;
    colors[ImGuiCol_TabSelectedOverline]   = accent;
    colors[ImGuiCol_TabDimmed]             = bgDarkest;
    colors[ImGuiCol_TabDimmedSelected]     = bgMid;
    colors[ImGuiCol_DockingPreview]        = accentDim;
    colors[ImGuiCol_DockingEmptyBg]        = bgDarkest;
    colors[ImGuiCol_PlotLines]             = accent;
    colors[ImGuiCol_PlotLinesHovered]      = accentHi;
    colors[ImGuiCol_PlotHistogram]         = accent;
    colors[ImGuiCol_PlotHistogramHovered]  = accentHi;
    colors[ImGuiCol_TextSelectedBg]        = accentDim;
    colors[ImGuiCol_DragDropTarget]        = accentHi;
    colors[ImGuiCol_NavHighlight]          = accent;
}

void DebugUI::Shutdown() {
    selectedNode = nullptr;
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

    if (!node->locked && node->GetComponent<MeshComponent>()) {
        auto* mesh = node->GetComponent<MeshComponent>();
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
        if (lightNode->locked) continue;
        if (auto* light = lightNode->GetComponent<LightComponent>()) {
            testCandidate(lightNode, light->GetPosition(), GizmoRenderer::kLightGizmoRadius);
        }
    }

    for (TNode* cameraNode : activeScene->GetCameras()) {
        if (cameraNode->locked) continue;
        testCandidate(cameraNode, cameraNode->getGlobalPosition(), GizmoRenderer::kCameraGizmoRadius);
    }

    WalkMeshCandidates(activeScene->GetRoot(), origin, direction, closestNode, closestT);

    return closestNode;
}

void DebugUI::SelectNode(TNode* node) {
    selectedNode = node;
    sceneSelected = false;
    gizmoMode = GizmoMode::Move;
    multiSelectedNodes.clear();
}

bool DebugUI::IsMultiSelected(TNode* node) {
    return std::find(multiSelectedNodes.begin(), multiSelectedNodes.end(), node) != multiSelectedNodes.end();
}

void DebugUI::ToggleNodeInMultiSelect(TNode* node) {
    if (!node) return;

    // Starting a fresh multi-select: seed it with whatever was already selected.
    if (multiSelectedNodes.empty() && selectedNode && selectedNode != node) {
        multiSelectedNodes.push_back(selectedNode);
    }

    auto it = std::find(multiSelectedNodes.begin(), multiSelectedNodes.end(), node);
    if (it != multiSelectedNodes.end()) {
        multiSelectedNodes.erase(it);
    } else {
        multiSelectedNodes.push_back(node);
    }

    sceneSelected = false;
    if (multiSelectedNodes.size() == 1) {
        selectedNode = multiSelectedNodes[0];
        multiSelectedNodes.clear();
    } else if (multiSelectedNodes.empty()) {
        selectedNode = nullptr;
    } else {
        selectedNode = node;
    }
}

void DebugUI::SelectScene() {
    selectedNode = nullptr;
    sceneSelected = true;
    multiSelectedNodes.clear();
}

void DebugUI::FocusOnSelected(Scene* activeScene) {
    if (!selectedNode || !activeScene) return;

    TNode* cameraNode = activeScene->GetMainCamera();
    if (!cameraNode) return;
    auto* camera = cameraNode->GetComponent<CameraComponent>();
    if (!camera) return;

    glm::vec3 center;
    float radius;

    if (auto* mesh = selectedNode->GetComponent<MeshComponent>()) {
        glm::vec3 localMin, localMax;
        mesh->GetLocalBounds(localMin, localMax);

        glm::mat4 model = selectedNode->getModelMatrix();
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

        center = (worldMin + worldMax) * 0.5f;
        radius = glm::length(worldMax - worldMin) * 0.5f;
    } else if (auto* light = selectedNode->GetComponent<LightComponent>()) {
        center = light->GetPosition();
        radius = GizmoRenderer::kLightGizmoRadius * 1.3f;
    } else {
        center = selectedNode->getGlobalPosition();
        radius = GizmoRenderer::kCameraGizmoRadius * 1.3f;
    }

    radius = std::max(radius, 0.5f);

    // Distance so the object's bounding sphere comfortably fits in the vertical FOV.
    float distance = radius / std::tan(glm::radians(camera->fov) * 0.5f);
    distance = std::max(distance, radius * 1.5f);

    cameraNode->transform.position = center - camera->GetForward() * distance;
}

namespace {

glm::vec3 WorldAxisDirection(TNode* node, int axis) {
    glm::vec3 local(axis == 0 ? 1.0f : 0.0f, axis == 1 ? 1.0f : 0.0f, axis == 2 ? 1.0f : 0.0f);
    if (DebugUI::GetGizmoSpace() == GizmoSpace::Global) {
        return local;
    }
    glm::vec4 world = node->getModelMatrix() * glm::vec4(local, 0.0f);
    return glm::normalize(glm::vec3(world));
}

float SnapValue(float value, float increment) {
    if (increment <= 0.0f) return value;
    return std::round(value / increment) * increment;
}

} // namespace

GizmoHandle DebugUI::PickGizmoHandle(Scene* activeScene) {
    TNode* node = selectedNode;
    if (!node || !activeScene) return GizmoHandle::None;

    glm::vec3 origin, direction;
    ComputePickRay(activeScene, origin, direction);

    glm::vec3 nodePos = node->getGlobalPosition();
    glm::vec3 cameraPos = origin;
    float scale = GizmoRenderer::ComputeGizmoScale(nodePos, cameraPos);
    float armLength = GizmoRenderer::kGizmoArmLength * scale;
    float ringRadius = GizmoRenderer::kGizmoRingRadius * scale;
    float tolerance = GizmoRenderer::kGizmoPickTolerance * scale;

    GizmoMode mode = gizmoMode;
    GizmoHandle bestHandle = GizmoHandle::None;
    float bestT = std::numeric_limits<float>::max();

    if (mode == GizmoMode::Scale) {
        float centerRadius = GizmoRenderer::kGizmoCenterCubeSize * scale;
        glm::vec3 oc = nodePos - origin;
        float tca = glm::dot(oc, direction);
        if (tca >= 0.0f) {
            float d2 = glm::dot(oc, oc) - tca * tca;
            if (d2 <= centerRadius * centerRadius) {
                bestT = tca;
                bestHandle = GizmoHandle::ScaleUniform;
            }
        }
    }

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
    TNode* node = selectedNode;
    if (!node || !activeScene || handle == GizmoHandle::None || node->locked) return;

    bool isMoveHandle = handle == GizmoHandle::MoveX || handle == GizmoHandle::MoveY || handle == GizmoHandle::MoveZ;
    if (isMoveHandle && ImGui::GetIO().KeyCtrl && node->parent) {
        TNode* duplicate = SceneSerializer::DuplicateNode(node, activeScene);
        if (duplicate) {
            node->parent->addChild(duplicate);
            SelectNode(duplicate);
            node = duplicate;
        }
    }

    activeHandle = handle;
    dragNode = node;
    dragStartLocalPosition = node->transform.position;
    dragStartLocalRotation = node->transform.rotation;
    dragStartLocalScale = node->transform.scale;

    if (handle == GizmoHandle::ScaleUniform) {
        dragStartMousePos = ImGui::GetIO().MousePos;
        return;
    }

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
        dragStartPointOnAxis = ClosestPointRayToLine(origin, direction, nodePos, axisDir);
    } else {
        glm::vec3 hit;
        if (RayIntersectsPlane(origin, direction, nodePos, axisDir, hit)) {
            glm::vec3 toHit = hit - nodePos;
            glm::vec3 basisA, basisB;
            glm::vec3 arbitrary = std::abs(axisDir.x) < 0.9f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
            basisA = glm::normalize(glm::cross(axisDir, arbitrary));
            basisB = glm::cross(axisDir, basisA);
            dragStartAngle = std::atan2(glm::dot(toHit, basisB), glm::dot(toHit, basisA));
        }
    }
}

void DebugUI::UpdateGizmoDrag(Scene* activeScene) {
    TNode* node = dragNode;
    if (!node || !activeScene || activeHandle == GizmoHandle::None) return;

    bool snap = ImGui::GetIO().KeyCtrl;

    if (activeHandle == GizmoHandle::ScaleUniform) {
        ImVec2 mousePos = ImGui::GetIO().MousePos;
        float pixelDelta = mousePos.x - dragStartMousePos.x;

        constexpr float kUniformScaleSensitivity = 0.01f;
        float multiplier = 1.0f + pixelDelta * kUniformScaleSensitivity;
        if (snap) multiplier = SnapValue(multiplier, EngineSettings::GetScaleSnap());

        glm::vec3 newScale = dragStartLocalScale * multiplier;
        newScale = glm::max(newScale, glm::vec3(0.01f));
        node->transform.scale = newScale;
        return;
    }

    glm::vec3 origin, direction;
    ComputePickRay(activeScene, origin, direction);
    glm::vec3 nodePos = node->getGlobalPosition();

    TNode* parent = node->parent;
    glm::mat4 parentModel = parent ? parent->getModelMatrix() : glm::mat4(1.0f);
    glm::mat4 parentInverse = glm::inverse(parentModel);

    if (activeHandle == GizmoHandle::MoveX || activeHandle == GizmoHandle::MoveY || activeHandle == GizmoHandle::MoveZ) {
        int axis = static_cast<int>(activeHandle) - static_cast<int>(GizmoHandle::MoveX);
        glm::vec3 axisDir = WorldAxisDirection(node, axis);

        glm::vec3 currentPoint = ClosestPointRayToLine(origin, direction, nodePos, axisDir);
        glm::vec3 worldDelta = currentPoint - dragStartPointOnAxis;

        glm::vec3 localDelta = glm::vec3(parentInverse * glm::vec4(worldDelta, 0.0f));
        glm::vec3 newPosition = dragStartLocalPosition + localDelta;
        if (snap) {
            float posSnap = EngineSettings::GetPositionSnap();
            newPosition.x = SnapValue(newPosition.x, posSnap);
            newPosition.y = SnapValue(newPosition.y, posSnap);
            newPosition.z = SnapValue(newPosition.z, posSnap);
        }
        node->transform.position = newPosition;
    } else if (activeHandle == GizmoHandle::ScaleX || activeHandle == GizmoHandle::ScaleY || activeHandle == GizmoHandle::ScaleZ) {
        int axis = static_cast<int>(activeHandle) - static_cast<int>(GizmoHandle::ScaleX);
        glm::vec3 axisDir = WorldAxisDirection(node, axis);

        glm::vec3 currentPoint = ClosestPointRayToLine(origin, direction, nodePos, axisDir);
        glm::vec3 worldDelta = currentPoint - dragStartPointOnAxis;
        float signedDistance = glm::dot(worldDelta, axisDir);

        constexpr float kScaleSensitivity = 1.0f;
        float multiplier = 1.0f + signedDistance * kScaleSensitivity;
        if (snap) multiplier = SnapValue(multiplier, EngineSettings::GetScaleSnap());

        glm::vec3 newScale = dragStartLocalScale;
        float* scaleAxis = &newScale.x + axis;
        float* startAxis = &dragStartLocalScale.x + axis;
        *scaleAxis = std::max(0.01f, (*startAxis) * multiplier);
        node->transform.scale = newScale;
    } else {
        int axis = static_cast<int>(activeHandle) - static_cast<int>(GizmoHandle::RotateX);
        glm::vec3 axisDir = WorldAxisDirection(node, axis);

        glm::vec3 hit;
        if (RayIntersectsPlane(origin, direction, nodePos, axisDir, hit)) {
            glm::vec3 toHit = hit - nodePos;
            glm::vec3 arbitrary = std::abs(axisDir.x) < 0.9f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
            glm::vec3 basisA = glm::normalize(glm::cross(axisDir, arbitrary));
            glm::vec3 basisB = glm::cross(axisDir, basisA);
            float currentAngle = std::atan2(glm::dot(toHit, basisB), glm::dot(toHit, basisA));

            float angleDelta = currentAngle - dragStartAngle;

            glm::vec3 newRotation = dragStartLocalRotation;
            float* rotAxis = &newRotation.x + axis;
            *rotAxis += glm::degrees(angleDelta);
            if (snap) *rotAxis = SnapValue(*rotAxis, EngineSettings::GetRotationSnapDegrees());
            node->transform.rotation = newRotation;
        }
    }
}

void DebugUI::DrawDockspace() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockspaceHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("MainDockspace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::End();
}

void DebugUI::DrawFrame(SceneManager* sceneManager) {
    if (!sceneManager) return;

    Scene* activeScene = sceneManager->GetActiveScene();
    if (!activeScene) return;

    DrawDockspace();

    UpdateAutoSave(sceneManager);

    if (!ImGui::GetIO().WantCaptureMouse) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && activeHandle == GizmoHandle::None) {
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
        } else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && activeHandle != GizmoHandle::None) {
            UpdateGizmoDrag(activeScene);
        }
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && activeHandle != GizmoHandle::None) {
        activeHandle = GizmoHandle::None;
        dragNode = nullptr;
    }

    if (!ImGui::GetIO().WantTextInput) {
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
            sceneToSave = sceneManager->GetActiveSceneName();
            showSaveConfirm = true;
        }

        if (selectedNode) {
            if (ImGui::IsKeyPressed(ImGuiKey_W)) gizmoMode = GizmoMode::Move;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) gizmoMode = GizmoMode::Rotate;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) gizmoMode = GizmoMode::Scale;
            if (ImGui::IsKeyPressed(ImGuiKey_F)) FocusOnSelected(activeScene);

            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
                TNode* parent = selectedNode->parent;
                if (parent) {
                    TNode* duplicate = SceneSerializer::DuplicateNode(selectedNode, activeScene);
                    if (duplicate) {
                        parent->addChild(duplicate);
                        SelectNode(duplicate);
                    }
                }
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            if (activeHandle != GizmoHandle::None && dragNode) {
                dragNode->transform.position = dragStartLocalPosition;
                dragNode->transform.rotation = dragStartLocalRotation;
                dragNode->transform.scale = dragStartLocalScale;
                activeHandle = GizmoHandle::None;
                dragNode = nullptr;
            } else {
                SelectNode(nullptr);
            }
        }
    }

    ImGui::SetNextWindowPos(ImVec2(10, 720), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260, 290), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Stats", nullptr)) {
        ImGui::Text("FPS: %.1f", Stats::GetFPS());
        ImGui::Text("Draw calls: %d", Stats::GetDrawCalls());
        ImGui::Separator();

        bool vsyncEnabled = EngineSettings::IsVSyncEnabled();
        if (ImGui::Checkbox("VSync", &vsyncEnabled)) {
            EngineSettings::SetVSyncEnabled(vsyncEnabled);
        }

        bool cullingEnabled = EngineSettings::IsFrustumCullingEnabled();
        if (ImGui::Checkbox("Frustum Culling", &cullingEnabled)) {
            EngineSettings::SetFrustumCullingEnabled(cullingEnabled);
        }

        bool wireframeEnabled = EngineSettings::IsWireframeEnabled();
        if (ImGui::Checkbox("Wireframe", &wireframeEnabled)) {
            EngineSettings::SetWireframeEnabled(wireframeEnabled);
        }

        bool shadowsEnabled = EngineSettings::AreShadowsEnabled();
        if (ImGui::Checkbox("Shadows", &shadowsEnabled)) {
            EngineSettings::SetShadowsEnabled(shadowsEnabled);
        }

        if (shadowsEnabled) {
            ImGui::Indent();

            static const int kResolutions2D[] = { 512, 1024, 2048, 4096 };
            static const char* kResolutionLabels2D[] = { "512", "1024", "2048", "4096" };
            int current2D = EngineSettings::GetShadowResolution2D();
            int currentIndex2D = 2;
            for (int i = 0; i < 4; ++i) if (kResolutions2D[i] == current2D) currentIndex2D = i;
            if (ImGui::Combo("2D Resolution", &currentIndex2D, kResolutionLabels2D, 4)) {
                EngineSettings::SetShadowResolution2D(kResolutions2D[currentIndex2D]);
            }

            static const int kResolutionsCube[] = { 256, 512, 1024, 2048 };
            static const char* kResolutionLabelsCube[] = { "256", "512", "1024", "2048" };
            int currentCube = EngineSettings::GetShadowResolutionCube();
            int currentIndexCube = 2;
            for (int i = 0; i < 4; ++i) if (kResolutionsCube[i] == currentCube) currentIndexCube = i;
            if (ImGui::Combo("Cube Resolution", &currentIndexCube, kResolutionLabelsCube, 4)) {
                EngineSettings::SetShadowResolutionCube(kResolutionsCube[currentIndexCube]);
            }

            float boxSize = EngineSettings::GetDirectionalShadowBoxSize();
            if (ImGui::DragFloat("Dir. Shadow Area", &boxSize, 1.0f, 5.0f, 500.0f)) {
                EngineSettings::SetDirectionalShadowBoxSize(boxSize);
            }
            ImGui::TextDisabled("Smaller area = sharper shadows,\nbut covers less around origin");

            ImGui::Unindent();
        }

        bool iblEnabled = EngineSettings::IsIBLEnabled();
        if (ImGui::Checkbox("IBL", &iblEnabled)) {
            EngineSettings::SetIBLEnabled(iblEnabled);
        }

        if (ImGui::TreeNode("Gizmo Snap (Ctrl+Drag)")) {
            float posSnap = EngineSettings::GetPositionSnap();
            if (ImGui::DragFloat("Position", &posSnap, 0.05f, 0.01f, 100.0f)) {
                EngineSettings::SetPositionSnap(posSnap);
            }

            float rotSnap = EngineSettings::GetRotationSnapDegrees();
            if (ImGui::DragFloat("Rotation (deg)", &rotSnap, 0.5f, 1.0f, 180.0f)) {
                EngineSettings::SetRotationSnapDegrees(rotSnap);
            }

            float scaleSnap = EngineSettings::GetScaleSnap();
            if (ImGui::DragFloat("Scale", &scaleSnap, 0.01f, 0.01f, 10.0f)) {
                EngineSettings::SetScaleSnap(scaleSnap);
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Auto-Save")) {
            bool autoSaveEnabled = EngineSettings::IsAutoSaveEnabled();
            if (ImGui::Checkbox("Enabled##autosave", &autoSaveEnabled)) {
                EngineSettings::SetAutoSaveEnabled(autoSaveEnabled);
            }

            float intervalSeconds = EngineSettings::GetAutoSaveIntervalSeconds();
            if (ImGui::DragFloat("Interval (s)", &intervalSeconds, 5.0f, 10.0f, 3600.0f)) {
                EngineSettings::SetAutoSaveIntervalSeconds(intervalSeconds);
            }

            if (!lastAutoSaveStatus.empty()) {
                ImGui::TextDisabled("%s", lastAutoSaveStatus.c_str());
            }

            ImGui::TreePop();
        }

        if (ImGui::Button("Save Settings")) {
            EngineConfig::Save();
        }
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500, 700), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Scene Debug", nullptr)) {
        // Scene Selector at top
        DrawSceneSelector(sceneManager);
        ImGui::Separator();

        if (ImGui::BeginTabBar("DebugTabs")) {
            // Tab 1: Scene Tree
            if (ImGui::BeginTabItem("Scene Tree")) {
                ImGui::Text("Scene Hierarchy:");
                ImGui::Separator();

                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##SceneTreeFilter", "Filter by name...", sceneTreeFilter, sizeof(sceneTreeFilter));

                ImGui::BeginChild("SceneTreeChild", ImVec2(0, 400), true);

                if (sceneTreeFilter[0] == '\0') {
                    ImGuiTreeNodeFlags sceneFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    if (sceneSelected) sceneFlags |= ImGuiTreeNodeFlags_Selected;
                    ImGui::TreeNodeEx("Scene##sceneEntry", sceneFlags);
                    if (ImGui::IsItemClicked()) {
                        SelectScene();
                    }
                    ImGui::Separator();
                }

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

            // Tab 2: Camera
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

    ImGui::SetNextWindowPos(ImVec2(10, 730), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(700, 320), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Project", nullptr)) {
        ProjectBrowser::Draw(sceneManager, activeScene);
    }
    ImGui::End();

    DrawInspector(activeScene);
    DrawDeleteConfirmation();
    DrawNodeDeleteConfirmation();
    DrawMultiDeleteConfirmation();
    DrawSaveConfirmation();
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

bool DebugUI::NodeMatchesFilter(TNode* node, const std::string& filter) {
    if (!node) return false;
    if (filter.empty()) return true;

    std::string name = node->name;
    std::string lowerName(name.size(), '\0');
    std::transform(name.begin(), name.end(), lowerName.begin(), [](unsigned char c) { return std::tolower(c); });

    if (lowerName.find(filter) != std::string::npos) return true;

    for (TNode* child : node->children) {
        if (NodeMatchesFilter(child, filter)) return true;
    }
    return false;
}

void DebugUI::DrawSceneTree(TNode* node, Scene* activeScene, int depth) {
    if (!node) return;

    std::string activeFilter = sceneTreeFilter;
    std::transform(activeFilter.begin(), activeFilter.end(), activeFilter.begin(),
        [](unsigned char c) { return std::tolower(c); });
    if (!NodeMatchesFilter(node, activeFilter)) return;

    std::string idSuffix = "##" + std::to_string(reinterpret_cast<uintptr_t>(node));

    if (renamingNode == node) {
        if (renameJustStarted) {
            ImGui::SetKeyboardFocusHere();
            renameJustStarted = false;
        }
        ImGui::SetNextItemWidth(200);
        bool commit = ImGui::InputText(("##rename" + idSuffix).c_str(), renameBuffer,
            sizeof(renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        if (commit) {
            node->name = renameBuffer;
            renamingNode = nullptr;
        } else if (ImGui::IsItemDeactivated()) {
            renamingNode = nullptr;
        }
        return;
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    if (node->children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    if (!activeFilter.empty()) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }

    if (node == selectedNode || IsMultiSelected(node)) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    std::string label = node->name.empty() ? "Unnamed" : node->name;
    label += "  [" + DescribeNode(node) + "]";
    label += idSuffix;

    bool opened = ImGui::TreeNodeEx(label.c_str(), flags);

    if (ImGui::IsItemClicked()) {
        if (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift) {
            ToggleNodeInMultiSelect(node);
        } else {
            SelectNode(node);
        }
    }

    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 44.0f);
    std::string lockLabel = std::string(node->locked ? "L" : "U") + idSuffix + "lock";
    if (ImGui::SmallButton(lockLabel.c_str())) {
        node->locked = !node->locked;
    }

    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 20.0f);
    std::string visLabel = std::string(node->visible ? "O" : "-") + idSuffix + "vis";
    if (ImGui::SmallButton(visLabel.c_str())) {
        node->visible = !node->visible;
    }

    if (ImGui::BeginPopupContextItem(("NodeContextMenu" + idSuffix).c_str())) {
        if (multiSelectedNodes.empty() || !IsMultiSelected(node)) {
            SelectNode(node);
        }

        if (!multiSelectedNodes.empty()) {
            if (ImGui::MenuItem(("Delete " + std::to_string(multiSelectedNodes.size()) + " Selected").c_str())) {
                showMultiDeleteConfirm = true;
            }
        } else {
            if (ImGui::MenuItem("Rename")) {
                renamingNode = node;
                renameJustStarted = true;
                std::string current = node->name.empty() ? "Unnamed" : node->name;
                std::snprintf(renameBuffer, sizeof(renameBuffer), "%s", current.c_str());
            }

            if (ImGui::MenuItem("Delete")) {
                nodeToDelete = node;
                showNodeDeleteConfirm = true;
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
        }

        ImGui::EndPopup();
    }

    if ((selectedNode == node || IsMultiSelected(node)) && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
        && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        if (!multiSelectedNodes.empty()) {
            showMultiDeleteConfirm = true;
        } else {
            nodeToDelete = node;
            showNodeDeleteConfirm = true;
        }
    }

    if (opened) {
        for (TNode* child : node->children) {
            DrawSceneTree(child, activeScene, depth + 1);
        }
        ImGui::TreePop();
    }
}

void DebugUI::DrawInspector(Scene* activeScene) {
    if (!selectedNode && !sceneSelected) return;

    ImGui::SetNextWindowPos(ImVec2(520, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 700), ImGuiCond_FirstUseEver);

    if (sceneSelected) {
        if (ImGui::Begin("Inspector", nullptr)) {
            ImGui::Text("Scene: %s", activeScene ? "Scene properties" : "");
            ImGui::Separator();
            ImGui::Spacing();

            if (activeScene) {
                bool gridVisible = activeScene->IsGridVisible();
                if (ImGui::Checkbox("Show Grid", &gridVisible)) {
                    activeScene->SetGridVisible(gridVisible);
                }

                glm::vec3 clearColor = activeScene->GetClearColor();
                if (ImGui::ColorEdit3("Background Color", &clearColor.x)) {
                    activeScene->SetClearColor(clearColor);
                }
            }

            ImGui::Spacing();
            DrawSkyboxInspector(activeScene);
        }
        ImGui::End();
        return;
    }

    if (ImGui::Begin("Inspector", nullptr)) {
        ImGui::Text("Node: %s", selectedNode->name.empty() ? "Unnamed" : selectedNode->name.c_str());
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Gizmo", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();

            ImGui::TextDisabled("Mode");
            if (ImGui::RadioButton("Move (W)", gizmoMode == GizmoMode::Move)) gizmoMode = GizmoMode::Move;
            ImGui::SameLine();
            if (ImGui::RadioButton("Rotate (E)", gizmoMode == GizmoMode::Rotate)) gizmoMode = GizmoMode::Rotate;
            ImGui::SameLine();
            if (ImGui::RadioButton("Scale (R)", gizmoMode == GizmoMode::Scale)) gizmoMode = GizmoMode::Scale;

            ImGui::Spacing();
            ImGui::TextDisabled("Space");
            if (ImGui::RadioButton("Global", gizmoSpace == GizmoSpace::Global)) gizmoSpace = GizmoSpace::Global;
            ImGui::SameLine();
            if (ImGui::RadioButton("Local", gizmoSpace == GizmoSpace::Local)) gizmoSpace = GizmoSpace::Local;

            ImGui::Spacing();
            ImGui::TextDisabled("Hold Ctrl while dragging to snap");

            ImGui::Unindent();
        }

        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();

            ImGui::DragFloat3("Position##inspector", &selectedNode->transform.position.x, 0.1f);
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset##pos")) selectedNode->transform.position = glm::vec3(0.0f);

            ImGui::DragFloat3("Rotation##inspector", &selectedNode->transform.rotation.x, 1.0f);
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset##rot")) selectedNode->transform.rotation = glm::vec3(0.0f);

            {
                glm::vec3 beforeScale = selectedNode->transform.scale;
                if (ImGui::DragFloat3("Scale##inspector", &selectedNode->transform.scale.x, 0.1f)) {
                    glm::vec3 afterScale = selectedNode->transform.scale;
                    for (int axis = 0; axis < 3; ++axis) {
                        if (!scaleAxisLocked[axis]) continue;
                        float delta = afterScale[axis] - beforeScale[axis];
                        if (delta == 0.0f) continue;
                        for (int other = 0; other < 3; ++other) {
                            if (other != axis && scaleAxisLocked[other]) {
                                selectedNode->transform.scale[other] = beforeScale[other] + delta;
                            }
                        }
                        break;
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset##scale")) selectedNode->transform.scale = glm::vec3(1.0f);

            ImGui::TextDisabled("Scale Lock");
            ImGui::SameLine();
            ImGui::Checkbox("X##scaleLockX", &scaleAxisLocked[0]);
            ImGui::SameLine();
            ImGui::Checkbox("Y##scaleLockY", &scaleAxisLocked[1]);
            ImGui::SameLine();
            ImGui::Checkbox("Z##scaleLockZ", &scaleAxisLocked[2]);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Check 2 or more axes to scale them together");
            }

            ImGui::Spacing();
            if (ImGui::SmallButton("Reset Transform")) {
                selectedNode->transform.position = glm::vec3(0.0f);
                selectedNode->transform.rotation = glm::vec3(0.0f);
                selectedNode->transform.scale = glm::vec3(1.0f);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Copy Transform")) {
                copiedTransform = selectedNode->transform;
                hasCopiedTransform = true;
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(!hasCopiedTransform);
            if (ImGui::SmallButton("Paste Transform")) {
                selectedNode->transform = copiedTransform;
            }
            ImGui::EndDisabled();

            if (auto* meshForPivot = selectedNode->GetComponent<MeshComponent>()) {
                if (ImGui::SmallButton("Recenter Pivot")) {
                    glm::vec3 localShift = meshForPivot->RecenterPivot();
                    glm::vec3 rotatedScaledShift = glm::vec3(selectedNode->transform.getModelMatrix() * glm::vec4(localShift, 0.0f));
                    selectedNode->transform.position += rotatedScaledShift;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Moves this node's origin to the center of its mesh bounds,\nwithout visually moving the geometry.");
                }
            }

            ImGui::Spacing();
            ImGui::TextDisabled("Global Position: (%.2f, %.2f, %.2f)",
                selectedNode->getGlobalPosition().x,
                selectedNode->getGlobalPosition().y,
                selectedNode->getGlobalPosition().z);

            ImGui::Unindent();
        }

        if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();

            if (selectedNode->components.empty()) {
                ImGui::BulletText("No components (Group Node)");
            }

            if (auto* mesh = selectedNode->GetComponent<MeshComponent>()) {
                ImGui::Text("Mesh:");
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove##mesh")) {
                    selectedNode->RemoveComponent<MeshComponent>();
                } else {
                    ImGui::Indent();
                    ImGui::BulletText("Vertices: %zu", mesh->GetVertexCount());
                    ImGui::BulletText("Indices: %zu", mesh->GetIndexCount());
                    ImGui::BulletText("Triangles: %zu", mesh->GetIndexCount() / 3);
                    ImGui::Unindent();
                }
            }

            if (auto* materialComp = selectedNode->GetComponent<MaterialComponent>()) {
                ImGui::Spacing();
                ImGui::Text("Material:");
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove##material")) {
                    selectedNode->RemoveComponent<MaterialComponent>();
                } else {
                    ImGui::Indent();
                    if (const auto& mat = materialComp->material) {
                        auto shader = mat->GetShader();
                        ImGui::BulletText("Shader: %s", shader ? shader->GetName().c_str() : "None");
                        ImGui::BulletText("Base Color: (%.2f, %.2f, %.2f, %.2f)",
                            mat->baseColor.r, mat->baseColor.g, mat->baseColor.b, mat->baseColor.a);
                        auto dropTextureSlot = [](const char* label, std::shared_ptr<Texture>& slot) {
                            std::string buttonLabel = std::string(label) + ": " + (slot ? "Yes" : "None");
                            ImGui::Button(buttonLabel.c_str(), ImVec2(220, 0));
                            if (ImGui::BeginDragDropTarget()) {
                                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ProjectBrowser::kTexturePayloadType)) {
                                    std::string filePath(static_cast<const char*>(payload->Data));
                                    std::string texName = filePath;
                                    std::replace(texName.begin(), texName.end(), '/', '_');
                                    std::replace(texName.begin(), texName.end(), '\\', '_');
                                    slot = ResourceManager::LoadTexture(texName, filePath);
                                }
                                ImGui::EndDragDropTarget();
                            }
                        };

                        dropTextureSlot("Albedo Texture", mat->albedo);
                        if (mat->albedo) {
                            ImGui::Indent();
                            ImGui::BulletText("%dx%d", mat->albedo->GetWidth(), mat->albedo->GetHeight());
                            ImGui::Unindent();
                        }
                        dropTextureSlot("Normal Texture", mat->normal);
                        dropTextureSlot("MetallicRoughness Texture", mat->metallicRoughness);
                        ImGui::SliderFloat("Metallic##material", &mat->metallicFactor, 0.0f, 1.0f);
                        ImGui::SliderFloat("Roughness##material", &mat->roughnessFactor, 0.0f, 1.0f);
                    } else {
                        ImGui::BulletText("No material resource");
                    }
                    ImGui::Unindent();
                }
            }

            if (auto* camera = selectedNode->GetComponent<CameraComponent>()) {
                ImGui::Spacing();
                ImGui::Text("Camera:");
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove##camera")) {
                    if (activeScene) {
                        activeScene->UnregisterCamera(selectedNode);
                    }
                    selectedNode->RemoveComponent<CameraComponent>();
                } else {
                    ImGui::Indent();
                    ImGui::BulletText("FOV: %.1f", camera->fov);
                    ImGui::BulletText("Yaw/Pitch: %.1f / %.1f", camera->yaw, camera->pitch);
                    ImGui::BulletText("Near/Far: %.2f / %.2f", camera->nearPlane, camera->farPlane);
                    ImGui::Unindent();
                }
            }

            if (auto* light = selectedNode->GetComponent<LightComponent>()) {
                const char* typeName = light->type == LightType::Directional ? "Directional"
                                      : light->type == LightType::Point ? "Point" : "Spot";

                ImGui::Spacing();
                ImGui::Text("Light (%s):", typeName);
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove##light")) {
                    if (activeScene) {
                        activeScene->UnregisterLight(selectedNode);
                    }
                    selectedNode->RemoveComponent<LightComponent>();
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
                    ImGui::Checkbox("Casts Shadow##light", &light->castsShadow);
                    ImGui::Unindent();
                }
            }

            ImGui::Unindent();
        }

        if (ImGui::CollapsingHeader("Hierarchy", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();

            ImGui::BulletText("Parent: %s", selectedNode->parent ? (selectedNode->parent->name.empty() ? "Root" : selectedNode->parent->name.c_str()) : "None");
            ImGui::BulletText("Children: %zu", selectedNode->children.size());
            if (!selectedNode->children.empty()) {
                ImGui::Indent();
                for (TNode* child : selectedNode->children) {
                    ImGui::BulletText("%s", child->name.empty() ? "Unnamed" : child->name.c_str());
                }
                ImGui::Unindent();
            }

            ImGui::Unindent();
        }
    }
    ImGui::End();
}

void DebugUI::DrawDeleteConfirmation() {
    if (!showDeleteConfirm) return;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, -1), ImGuiCond_FirstUseEver);

    bool open = true;
    if (ImGui::Begin("Delete Scene?", &open, ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Are you sure you want to delete:\n\n\"%s\"\n\nThis action cannot be undone.", sceneToDelete.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float buttonWidth = 140.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float totalWidth = (buttonWidth * 2) + spacing;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - totalWidth) * 0.5f);

        if (ImGui::Button("Delete Forever", ImVec2(buttonWidth, 0))) {
            SceneManager::Instance().UnloadScene(sceneToDelete);
            sceneToDelete = "";
            showDeleteConfirm = false;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            showDeleteConfirm = false;
            sceneToDelete = "";
        }

        ImGui::End();
    }

    if (!open) {
        showDeleteConfirm = false;
    }
}

void DebugUI::DrawSaveConfirmation() {
    if (!showSaveConfirm) return;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, -1), ImGuiCond_FirstUseEver);

    bool open = true;
    if (ImGui::Begin("Overwrite Scene?", &open, ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Save and overwrite:\n\n\"%s\"", sceneToSave.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float buttonWidth = 140.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float totalWidth = (buttonWidth * 2) + spacing;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - totalWidth) * 0.5f);

        if (ImGui::Button("Save", ImVec2(buttonWidth, 0))) {
            std::filesystem::create_directories("scenes");
            std::string fileName = "scenes/" + sceneToSave + ".scene";
            SceneSerializer::SaveScene(SceneManager::Instance().GetActiveScene(), fileName);
            showSaveConfirm = false;
            sceneToSave = "";
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            showSaveConfirm = false;
            sceneToSave = "";
        }

        ImGui::End();
    }

    if (!open) {
        showSaveConfirm = false;
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

    if (selectedNode == node) {
        selectedNode = nullptr;
    }
    if (renamingNode == node) {
        renamingNode = nullptr;
    }
    multiSelectedNodes.erase(std::remove(multiSelectedNodes.begin(), multiSelectedNodes.end(), node), multiSelectedNodes.end());

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
            auto* mesh = node->AddComponent<MeshComponent>(vertices, indices);

            if (!node->boundingBox) {
                glm::vec3 localMin, localMax;
                mesh->GetLocalBounds(localMin, localMax);
                node->boundingBox = new AABB(localMin, localMax);
            }

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

    // Converts a desired world-space spawn position into the new node's
    // local transform, accounting for the parent's own transform.
    auto placeAtSpawnPoint = [&](TNode* node) {
        glm::vec3 worldPos = ComputeSpawnWorldPosition(activeScene);
        glm::mat4 parentInverse = glm::inverse(parent->getModelMatrix());
        node->transform.position = glm::vec3(parentInverse * glm::vec4(worldPos, 1.0f));
    };

    if (ImGui::MenuItem("Cube")) {
        TNode* node = SpawnCubeNode();
        placeAtSpawnPoint(node);
        parent->addChild(node);
        SelectNode(node);
    }

    if (ImGui::MenuItem("Camera")) {
        TNode* node = SpawnCameraNode();
        placeAtSpawnPoint(node);
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
            placeAtSpawnPoint(node);
            parent->addChild(node);
            if (activeScene) activeScene->RegisterLight(node);
            SelectNode(node);
        }
        if (ImGui::MenuItem("Spot")) {
            TNode* node = SpawnLightNode(LightType::Spot);
            placeAtSpawnPoint(node);
            parent->addChild(node);
            if (activeScene) activeScene->RegisterLight(node);
            SelectNode(node);
        }
        ImGui::EndMenu();
    }
}

void DebugUI::DrawNodeDeleteConfirmation() {
    if (!showNodeDeleteConfirm || !nodeToDelete) return;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, -1), ImGuiCond_FirstUseEver);

    std::string nodeName = nodeToDelete->name.empty() ? "Unnamed" : nodeToDelete->name;

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
            DeleteNode(nodeToDelete, SceneManager::Instance().GetActiveScene());
            nodeToDelete = nullptr;
            showNodeDeleteConfirm = false;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel##node", ImVec2(buttonWidth, 0))) {
            nodeToDelete = nullptr;
            showNodeDeleteConfirm = false;
        }

        ImGui::End();
    }

    if (!open) {
        nodeToDelete = nullptr;
        showNodeDeleteConfirm = false;
    }
}

void DebugUI::DrawMultiDeleteConfirmation() {
    if (!showMultiDeleteConfirm || multiSelectedNodes.empty()) return;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, -1), ImGuiCond_FirstUseEver);

    bool open = true;
    if (ImGui::Begin("Delete Nodes?", &open, ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Are you sure you want to delete %zu selected nodes?\n\nThis will also delete all of their children. This action cannot be undone.", multiSelectedNodes.size());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float buttonWidth = 140.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float totalWidth = (buttonWidth * 2) + spacing;
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - totalWidth) * 0.5f);

        if (ImGui::Button("Delete Forever##multi", ImVec2(buttonWidth, 0))) {
            std::vector<TNode*> toDelete = multiSelectedNodes;
            Scene* activeScene = SceneManager::Instance().GetActiveScene();
            for (TNode* node : toDelete) {
                DeleteNode(node, activeScene);
            }
            multiSelectedNodes.clear();
            showMultiDeleteConfirm = false;
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel##multi", ImVec2(buttonWidth, 0))) {
            showMultiDeleteConfirm = false;
        }

        ImGui::End();
    }

    if (!open) {
        showMultiDeleteConfirm = false;
    }
}

void DebugUI::UpdateAutoSave(SceneManager* sceneManager) {
    if (!EngineSettings::IsAutoSaveEnabled() || !sceneManager) {
        autoSaveTimer = 0.0f;
        return;
    }

    autoSaveTimer += ImGui::GetIO().DeltaTime;
    float interval = EngineSettings::GetAutoSaveIntervalSeconds();
    if (interval <= 0.0f || autoSaveTimer < interval) return;

    autoSaveTimer = 0.0f;

    Scene* activeScene = sceneManager->GetActiveScene();
    if (!activeScene) return;

    std::filesystem::create_directories("scenes");
    std::string fileName = "scenes/" + sceneManager->GetActiveSceneName() + "_autosave.scene";
    SceneSerializer::SaveScene(activeScene, fileName);
    lastAutoSaveStatus = "Auto-saved at " + std::to_string(static_cast<int>(ImGui::GetTime())) + "s";
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

void DebugUI::DrawSkyboxInspector(Scene* activeScene) {
    if (!activeScene) return;

    ImGui::Text("Skybox:");

    Skybox* skybox = activeScene->GetSkybox();
    if (skybox) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove##skybox")) {
            activeScene->SetSkybox(nullptr);
        }
        ImGui::Indent();
        ImGui::BulletText("Folder: %s", skybox->GetName().c_str());
        ImGui::Unindent();
    } else {
        ImGui::Indent();
        ImGui::BulletText("None");
        ImGui::Unindent();
    }

    ImGui::Spacing();
    ImGui::Indent();
    ImGui::TextWrapped(
        "Folder under resources/textures/skybox/ containing "
        "right/left/top/bottom/front/back.jpg or .png");
    ImGui::InputText("Folder##skybox", skyboxFolderBuffer, sizeof(skyboxFolderBuffer));

    if (ImGui::Button("Load Skybox")) {
        std::string folder = skyboxFolderBuffer;
        if (folder.empty()) {
            skyboxLoadError = "Folder name is empty.";
        } else {
            auto cubemap = ResourceManager::LoadSkyboxFromFolder(folder);
            if (cubemap) {
                activeScene->SetSkybox(std::make_shared<Skybox>(cubemap, folder));
                skyboxLoadError.clear();
            } else {
                skyboxLoadError = "Failed to load skybox '" + folder + "' (check console for missing faces).";
            }
        }
    }

    if (!skyboxLoadError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", skyboxLoadError.c_str());
    }
    ImGui::Unindent();
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
                EngineSettings::SetLastActiveScene(name);
                EngineConfig::Save();
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
        EngineSettings::SetLastActiveScene(newSceneName);
        EngineConfig::Save();
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
            sceneToDelete = sceneManager->GetActiveSceneName();
            showDeleteConfirm = true;
            ImGui::OpenPopup("Delete Scene Confirmation");
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Rename##btn")) {
        if (sceneManager->GetActiveScene()) {
            renamingScene = true;
            sceneRenameError = "";
            std::snprintf(sceneRenameBuffer, sizeof(sceneRenameBuffer), "%s", sceneManager->GetActiveSceneName().c_str());
        }
    }

    if (renamingScene) {
        ImGui::SetNextItemWidth(200);
        bool commit = ImGui::InputText("##SceneRename", sceneRenameBuffer, sizeof(sceneRenameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        ImGui::SameLine();
        bool confirmed = commit || ImGui::SmallButton("OK##sceneRename");
        ImGui::SameLine();
        bool cancelled = ImGui::SmallButton("Cancel##sceneRename");

        if (confirmed) {
            std::string newName = sceneRenameBuffer;
            std::string oldName = sceneManager->GetActiveSceneName();
            if (newName.empty()) {
                sceneRenameError = "Name cannot be empty.";
            } else if (newName != oldName && sceneManager->GetScene(newName)) {
                sceneRenameError = "A scene with that name already exists.";
            } else {
                if (newName != oldName && sceneManager->RenameScene(oldName, newName)) {
                    EngineSettings::SetLastActiveScene(newName);
                    EngineConfig::Save();
                }
                renamingScene = false;
            }
        } else if (cancelled) {
            renamingScene = false;
            sceneRenameError = "";
        }

        if (!sceneRenameError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", sceneRenameError.c_str());
        }
    }
}
