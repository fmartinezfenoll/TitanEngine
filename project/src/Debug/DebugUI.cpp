#include "Debug/DebugUI.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/TNode.h"
#include "Scene/SceneSerializer.h"
#include "Scene/PrefabSerializer.h"
#include "Scene/CameraComponent.h"
#include "Scene/MeshComponent.h"
#include "Scene/MeshPrimitives.h"
#include "Scene/MaterialComponent.h"
#include "Scene/LightComponent.h"
#include "Scene/AnimationComponent.h"
#include "Scene/AnimationStateMachine.h"
#include "Scene/CameraPathComponent.h"
#include "Scene/BillboardComponent.h"
#include "Scene/GrassComponent.h"
#include "Scene/ParticleSystemComponent.h"
#include "Scene/TerrainComponent.h"
#include "Scene/PatrolComponent.h"
#include "ResourceManager/Material.h"
#include "ResourceManager/MaterialSerializer.h"
#include "ResourceManager/Texture.h"
#include "ResourceManager/OpenGLShader.h"
#include "ResourceManager/ResourceManager.h"
#include "Renderer/GizmoRenderer.h"
#include "Renderer/Viewport.h"
#include "Renderer/Skybox.h"
#include "ResourceManager/CubemapTexture.h"
#include "Debug/ProjectBrowser.h"
#include "Debug/MaterialIcons.h"
#include "Debug/ImGuiLayoutUtils.h"
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
#include <cfloat>

namespace {

struct SceneStats {
    int nodeCount = 0;
    int meshCount = 0;
    size_t vertexCount = 0;
    size_t triangleCount = 0;
};

void AccumulateSceneStats(TNode* node, SceneStats& out) {
    if (!node) return;
    ++out.nodeCount;
    if (auto* mesh = node->GetComponent<MeshComponent>()) {
        ++out.meshCount;
        out.vertexCount += mesh->GetVertexCount();
        out.triangleCount += mesh->GetIndexCount() / 3;
    }
    for (TNode* child : node->children) {
        AccumulateSceneStats(child, out);
    }
}

SceneStats ComputeSceneStats(Scene* scene) {
    SceneStats stats;
    if (scene) AccumulateSceneStats(scene->GetRoot(), stats);
    return stats;
}

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

// Builds a node from a procedurally-generated primitive mesh + a default PBR
// material, matching SpawnCubeNode's setup (bounds, material).
TNode* SpawnPrimitiveNode(const char* baseName, const std::vector<MeshVertex>& vertices,
                          const std::vector<uint32_t>& indices) {
    TNode* node = new TNode(nullptr, NextName(baseName));
    auto* mesh = node->AddComponent<MeshComponent>(vertices, indices);

    glm::vec3 localMin, localMax;
    mesh->GetLocalBounds(localMin, localMax);
    node->boundingBox = new AABB(localMin, localMax);

    auto material = std::make_shared<Material>(ResourceManager::LoadShader("pbr"));
    node->AddComponent<MaterialComponent>(material);
    return node;
}

TNode* SpawnSphereNode() {
    std::vector<MeshVertex> v; std::vector<uint32_t> i;
    MeshPrimitives::Sphere(v, i);
    return SpawnPrimitiveNode("Sphere", v, i);
}

TNode* SpawnPlaneNode() {
    std::vector<MeshVertex> v; std::vector<uint32_t> i;
    MeshPrimitives::Plane(v, i, 2.0f, 1);
    return SpawnPrimitiveNode("Plane", v, i);
}

TNode* SpawnCylinderNode() {
    std::vector<MeshVertex> v; std::vector<uint32_t> i;
    MeshPrimitives::Cylinder(v, i);
    return SpawnPrimitiveNode("Cylinder", v, i);
}

TNode* SpawnConeNode() {
    std::vector<MeshVertex> v; std::vector<uint32_t> i;
    MeshPrimitives::Cone(v, i);
    return SpawnPrimitiveNode("Cone", v, i);
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
std::string DebugUI::inspectingMaterialPath = "";
std::shared_ptr<Material> DebugUI::inspectingMaterial = nullptr;
ImFont* DebugUI::iconFont = nullptr;
bool DebugUI::showDeleteConfirm = false;
std::string DebugUI::sceneToDelete = "";

TNode* DebugUI::nodeToDelete = nullptr;
bool DebugUI::showNodeDeleteConfirm = false;

std::vector<TNode*> DebugUI::multiSelectedNodes;
bool DebugUI::showMultiDeleteConfirm = false;
std::string DebugUI::copiedNodeJson = "";

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

char DebugUI::saveMaterialBuffer[128] = "";
std::string DebugUI::saveMaterialError = "";

TNode* DebugUI::creatingPrefabFrom = nullptr;
char DebugUI::createPrefabBuffer[128] = "";
std::string DebugUI::createPrefabError = "";

std::string DebugUI::editingClipName = "";
float DebugUI::editorScrubTime = 0.0f;
char DebugUI::newClipNameBuffer[128] = "";
char DebugUI::newStateNameBuffer[128] = "";

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

    // SetNextWindowSizeConstraints (used per-window below) only limits a
    // FLOATING window's own resize grip -- ImGui's docking splitters ignore
    // it and instead clamp against this global minimum when the user drags
    // the divider between two docked panels. Set to the largest per-window
    // minimum (Inspector's) so no docked panel can be squeezed small enough
    // to clip its content.
    style.WindowMinSize = ImVec2(300.0f, 260.0f);

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

    // Icon font: a small subset of Google Material Symbols (see MaterialIcons.h),
    // loaded as a SEPARATE font (MergeMode = false) rather than merged into the
    // default font, so normal UI text is unaffected. Activated explicitly via
    // PushFont(DebugUI::GetIconFont())/PopFont() wherever an icon is drawn.
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();

    static const ImWchar iconRanges[] = { MATERIAL_ICONS_CODEPOINTS, 0 };
    ImFontConfig iconFontConfig;
    iconFontConfig.MergeMode = false;
    iconFont = io.Fonts->AddFontFromFileTTF(
        "resources/fonts/materialsymbols/MaterialSymbolsOutlined.ttf",
        18.0f, &iconFontConfig, iconRanges);
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
    inspectingMaterialPath.clear();
    inspectingMaterial = nullptr;
    gizmoMode = GizmoMode::Move;
    multiSelectedNodes.clear();
    editingClipName.clear();
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
    inspectingMaterialPath.clear();
    inspectingMaterial = nullptr;
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
    inspectingMaterialPath.clear();
    inspectingMaterial = nullptr;
    multiSelectedNodes.clear();
}

void DebugUI::SelectMaterialAsset(const std::string& path) {
    selectedNode = nullptr;
    sceneSelected = false;
    multiSelectedNodes.clear();
    inspectingMaterialPath = path;
    inspectingMaterial = MaterialSerializer::Load(path);
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

void DebugUI::LookAtSelected(Scene* activeScene) {
    if (!selectedNode || !activeScene) return;

    TNode* cameraNode = activeScene->GetMainCamera();
    if (!cameraNode) return;
    auto* camera = cameraNode->GetComponent<CameraComponent>();
    if (!camera) return;

    glm::vec3 target = selectedNode->getGlobalPosition();
    if (auto* light = selectedNode->GetComponent<LightComponent>()) {
        target = light->GetPosition();
    }

    glm::vec3 dir = target - cameraNode->getGlobalPosition();
    if (glm::length(dir) < 1e-4f) return; // camera is already at the target
    dir = glm::normalize(dir);

    camera->yaw = glm::degrees(std::atan2(dir.z, dir.x));
    camera->pitch = glm::degrees(std::asin(std::clamp(dir.y, -1.0f, 1.0f)));
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

    bool snap = EngineSettings::IsAlwaysSnapEnabled() ? !ImGui::GetIO().KeyCtrl : ImGui::GetIO().KeyCtrl;

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

            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
                copiedNodeJson = SceneSerializer::SerializeNodeToString(selectedNode);
            }
        }

        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V) && !copiedNodeJson.empty()) {
            TNode* parent = selectedNode ? selectedNode : activeScene->GetRoot();
            if (TNode* pasted = SceneSerializer::DeserializeNodeFromString(copiedNodeJson, activeScene)) {
                parent->addChild(pasted);
                SelectNode(pasted);
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
    ImGui::SetNextWindowSizeConstraints(ImVec2(340, 220), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::Begin("Stats", nullptr)) {
        ImGui::Text("FPS: %.1f", Stats::GetFPS());
        ImGui::Text("Draw calls: %d", Stats::GetDrawCalls());

        SceneStats sceneStats = ComputeSceneStats(activeScene);
        ImGui::Text("Nodes: %d  Meshes: %d", sceneStats.nodeCount, sceneStats.meshCount);
        ImGui::Text("Vertices: %zu  Triangles: %zu", sceneStats.vertexCount, sceneStats.triangleCount);
        ImGui::Separator();

        bool vsyncEnabled = EngineSettings::IsVSyncEnabled();
        if (ImGui::Checkbox("VSync", &vsyncEnabled)) {
            EngineSettings::SetVSyncEnabled(vsyncEnabled);
        }

        bool cullingEnabled = EngineSettings::IsFrustumCullingEnabled();
        if (ImGui::Checkbox("Frustum Culling", &cullingEnabled)) {
            EngineSettings::SetFrustumCullingEnabled(cullingEnabled);
        }

        bool distanceCull = EngineSettings::IsDistanceCullEnabled();
        if (ImGui::Checkbox("Distance Culling", &distanceCull)) {
            EngineSettings::SetDistanceCullEnabled(distanceCull);
        }
        if (distanceCull) {
            float maxDist = EngineSettings::GetMaxDrawDistance();
            ImGui::SetNextItemWidth(120);
            if (ImGui::DragFloat("Max Draw Distance", &maxDist, 1.0f, 1.0f, 10000.0f, "%.0f")) {
                EngineSettings::SetMaxDrawDistance(maxDist);
            }
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

        if (ImGui::TreeNode("Gizmo Snap")) {
            bool alwaysSnap = EngineSettings::IsAlwaysSnapEnabled();
            if (ImGui::Checkbox("Always Snap", &alwaysSnap)) {
                EngineSettings::SetAlwaysSnapEnabled(alwaysSnap);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(alwaysSnap
                    ? "Dragging snaps by default; hold Ctrl to move freely."
                    : "Dragging moves freely by default; hold Ctrl to snap.");
            }

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
    ImGui::SetNextWindowSizeConstraints(ImVec2(520, 340), ImVec2(FLT_MAX, FLT_MAX));

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

                // Drop a .prefab from the Project Browser, or a node dragged from
                // elsewhere in the tree, anywhere in this window -- both re-parent
                // to the scene root (top level).
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ProjectBrowser::kPrefabPayloadType)) {
                        std::string filePath(static_cast<const char*>(payload->Data));
                        if (root) {
                            if (TNode* instance = PrefabSerializer::Instantiate(filePath, activeScene)) {
                                root->addChild(instance);
                                SelectNode(instance);
                            }
                        }
                    }
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kNodeDragPayloadType)) {
                        TNode* dragged = *static_cast<TNode**>(payload->Data);
                        if (root && dragged && dragged != root) {
                            root->addChild(dragged);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (ImGui::BeginPopupContextWindow("SceneRootContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                    if (root) {
                        if (ImGui::MenuItem("Create Empty")) {
                            TNode* empty = new TNode(nullptr, "Empty");
                            root->addChild(empty);
                            SelectNode(empty);
                        }
                        DrawCreateMenu(root, activeScene);          // 3D Object / Camera / Light
                        DrawCreateWithComponentMenu(root, activeScene); // empty node + any component
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
    ImGui::SetNextWindowSizeConstraints(ImVec2(580, 340), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::Begin("Project", nullptr)) {
        ProjectBrowser::Draw(sceneManager, activeScene);
    }
    ImGui::End();

    DrawInspector(activeScene);
    DrawDeleteConfirmation();
    DrawNodeDeleteConfirmation();
    DrawMultiDeleteConfirmation();
    DrawSaveConfirmation();
    DrawCreatePrefabPopup();
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
    float labelRightEdge = ImGui::GetItemRectMax().x;

    if (ImGui::IsItemClicked()) {
        if (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift) {
            ToggleNodeInMultiSelect(node);
        } else {
            SelectNode(node);
        }
    }

    // Drag this node's row to re-parent it elsewhere in the tree.
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload(kNodeDragPayloadType, &node, sizeof(TNode*));
        ImGui::Text("%s", label.c_str());
        ImGui::EndDragDropSource();
    }

    // Drop a .prefab, or another node from this tree, onto this node's row.
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ProjectBrowser::kPrefabPayloadType)) {
            std::string filePath(static_cast<const char*>(payload->Data));
            if (TNode* instance = PrefabSerializer::Instantiate(filePath, activeScene)) {
                node->addChild(instance);
                SelectNode(instance);
            }
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kNodeDragPayloadType)) {
            TNode* dragged = *static_cast<TNode**>(payload->Data);
            // Refuse no-op (dropping onto itself) and cycles (dropping onto one
            // of its own descendants, which would detach the subtree it's in).
            bool isDescendant = false;
            for (TNode* ancestor = node; ancestor; ancestor = ancestor->parent) {
                if (ancestor == dragged) { isDescendant = true; break; }
            }
            if (dragged && dragged != node && !isDescendant) {
                node->addChild(dragged); // addChild already detaches from its old parent
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Anchored to the TreeNodeEx row itself (via BeginDragDropTarget above, which
    // doesn't change ImGui's "last item" tracking) so right-clicking anywhere on
    // the node's name/label opens the menu -- not just the small lock/vis buttons
    // drawn after it, which BeginPopupContextItem() would otherwise anchor to.
    if (ImGui::BeginPopupContextItem(("NodeContextMenu" + idSuffix).c_str())) {
        if (multiSelectedNodes.empty() || !IsMultiSelected(node)) {
            SelectNode(node);
        }

        if (!multiSelectedNodes.empty()) {
            if (ImGui::MenuItem(("Delete " + std::to_string(multiSelectedNodes.size()) + " Selected").c_str())) {
                showMultiDeleteConfirm = true;
            }

            if (ImGui::MenuItem(("Duplicate " + std::to_string(multiSelectedNodes.size()) + " Selected").c_str())) {
                std::vector<TNode*> toDuplicate = multiSelectedNodes;
                for (TNode* original : toDuplicate) {
                    if (!original->parent) continue;
                    if (TNode* duplicate = SceneSerializer::DuplicateNode(original, activeScene)) {
                        original->parent->addChild(duplicate);
                    }
                }
            }
        } else {
            // --- This node's own actions ---
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

            if (ImGui::MenuItem("Duplicate")) {
                if (node->parent) {
                    if (TNode* duplicate = SceneSerializer::DuplicateNode(node, activeScene)) {
                        node->parent->addChild(duplicate);
                        SelectNode(duplicate);
                    }
                }
            }

            if (ImGui::MenuItem("Copy")) {
                copiedNodeJson = SceneSerializer::SerializeNodeToString(node);
            }

            if (ImGui::MenuItem("Paste", nullptr, false, !copiedNodeJson.empty())) {
                if (TNode* pasted = SceneSerializer::DeserializeNodeFromString(copiedNodeJson, activeScene)) {
                    node->addChild(pasted);
                    SelectNode(pasted);
                }
            }

            if (node->GetComponent<CameraComponent>()) {
                bool isMain = activeScene && activeScene->GetMainCamera() == node;
                if (ImGui::MenuItem("Set as Main Camera", nullptr, false, !isMain)) {
                    activeScene->SetMainCamera(node);
                }
            }

            if (ImGui::MenuItem("Create Prefab...")) {
                // Only raises the flag; DrawCreatePrefabPopup() (called every frame from
                // DrawFrame) issues the actual OpenPopup. This avoids ID-stack issues when
                // triggered from inside a context-menu popup that's about to close.
                creatingPrefabFrom = node;
                std::string base = node->name.empty() ? "Prefab" : node->name;
                std::snprintf(createPrefabBuffer, sizeof(createPrefabBuffer), "%s", base.c_str());
                createPrefabError.clear();
            }

            ImGui::Separator();

            // --- Create a new child node ---
            if (ImGui::MenuItem("Create Empty")) {
                TNode* empty = new TNode(nullptr, "Empty");
                node->addChild(empty);
                SelectNode(empty);
            }
            DrawCreateMenu(node, activeScene);          // 3D Object / Camera / Light
            DrawCreateWithComponentMenu(node, activeScene); // empty node + any component in one pick

            ImGui::Separator();

            // --- Modify this node ---
            DrawAddComponentMenu(node, activeScene);
        }

        ImGui::EndPopup();
    }

    // Clamped against the label's own right edge (converted from screen-space
    // to window-space) so these never overlap the node's name/arrow when the
    // panel is narrow or deeply indented -- GetWindowContentRegionMax().x -
    // fixedPixels alone can fall behind a long/indented label.
    float afterLabelX = labelRightEdge - ImGui::GetWindowPos().x;
    float rightEdge = ImGui::GetWindowContentRegionMax().x;

    ImGui::SameLine(std::max(afterLabelX + 4.0f, rightEdge - 44.0f));
    std::string lockLabel = std::string(node->locked ? "L" : "U") + idSuffix + "lock";
    if (ImGui::SmallButton(lockLabel.c_str())) {
        node->locked = !node->locked;
    }

    ImGui::SameLine(std::max(afterLabelX + 4.0f, rightEdge - 20.0f));
    std::string visLabel = std::string(node->visible ? "O" : "-") + idSuffix + "vis";
    if (ImGui::SmallButton(visLabel.c_str())) {
        node->visible = !node->visible;
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
    if (!selectedNode && !sceneSelected && inspectingMaterialPath.empty()) return;

    ImGui::SetNextWindowPos(ImVec2(520, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 700), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(440, 340), ImVec2(FLT_MAX, FLT_MAX));

    if (!inspectingMaterialPath.empty()) {
        if (ImGui::Begin("Inspector", nullptr)) {
            ImGui::Text("Material Asset: %s", inspectingMaterialPath.c_str());
            ImGui::Separator();
            ImGui::Spacing();
            if (inspectingMaterial) {
                DrawMaterialFields(inspectingMaterial, &inspectingMaterialPath);
            } else {
                ImGui::TextDisabled("Failed to load material.");
            }
        }
        ImGui::End();
        return;
    }

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

                ImGui::Spacing();
                ImGui::Text("Fog:");
                FogSettings& fog = activeScene->GetFog();
                ImGui::Checkbox("Enabled##fog", &fog.enabled);
                if (fog.enabled) {
                    ImGui::Indent();
                    const char* fogModes[] = { "Linear", "Exp", "Exp2" };
                    int modeIdx = static_cast<int>(fog.mode);
                    ImGui::SetNextItemWidth(120);
                    if (ImGui::Combo("Mode##fog", &modeIdx, fogModes, IM_ARRAYSIZE(fogModes))) {
                        fog.mode = static_cast<FogMode>(modeIdx);
                    }
                    ImGui::ColorEdit3("Color##fog", &fog.color.x);
                    if (fog.mode == FogMode::Linear) {
                        ImGui::DragFloat("Start##fog", &fog.start, 0.5f, 0.0f, 10000.0f);
                        ImGui::DragFloat("End##fog", &fog.end, 0.5f, 0.0f, 10000.0f);
                    } else {
                        ImGui::DragFloat("Density##fog", &fog.density, 0.001f, 0.0f, 1.0f, "%.4f");
                    }
                    ImGui::Unindent();
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

            auto radioWidth = [](const char* label) {
                return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(label).x;
            };

            ImGui::TextDisabled("Mode");
            if (ImGui::RadioButton("Move (W)", gizmoMode == GizmoMode::Move)) gizmoMode = GizmoMode::Move;
            ImGuiLayoutUtils::SameLineOrWrap(radioWidth("Rotate (E)"), false);
            if (ImGui::RadioButton("Rotate (E)", gizmoMode == GizmoMode::Rotate)) gizmoMode = GizmoMode::Rotate;
            ImGuiLayoutUtils::SameLineOrWrap(radioWidth("Scale (R)"), false);
            if (ImGui::RadioButton("Scale (R)", gizmoMode == GizmoMode::Scale)) gizmoMode = GizmoMode::Scale;

            ImGui::Spacing();
            ImGui::TextDisabled("Space");
            if (ImGui::RadioButton("Global", gizmoSpace == GizmoSpace::Global)) gizmoSpace = GizmoSpace::Global;
            ImGuiLayoutUtils::SameLineOrWrap(radioWidth("Local"), false);
            if (ImGui::RadioButton("Local", gizmoSpace == GizmoSpace::Local)) gizmoSpace = GizmoSpace::Local;

            ImGui::Spacing();
            ImGui::TextDisabled(EngineSettings::IsAlwaysSnapEnabled()
                ? "Always Snap is on -- hold Ctrl while dragging to move freely"
                : "Hold Ctrl while dragging to snap");

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

            float checkboxWidth = ImGui::GetFrameHeight();
            ImGui::TextDisabled("Scale Lock");
            ImGuiLayoutUtils::SameLineOrWrap(checkboxWidth, false);
            ImGui::Checkbox("X##scaleLockX", &scaleAxisLocked[0]);
            ImGuiLayoutUtils::SameLineOrWrap(checkboxWidth, false);
            ImGui::Checkbox("Y##scaleLockY", &scaleAxisLocked[1]);
            ImGuiLayoutUtils::SameLineOrWrap(checkboxWidth, false);
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
            if (ImGui::SmallButton("Focus (F)")) {
                FocusOnSelected(activeScene);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Moves the active camera toward this object,\nkeeping its current facing direction.");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Look At")) {
                LookAtSelected(activeScene);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Rotates the active camera in place to face this object,\nwithout moving it.");
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
                        if (ImGui::BeginDragDropTarget()) {
                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ProjectBrowser::kMaterialPayloadType)) {
                                std::string filePath(static_cast<const char*>(payload->Data));
                                if (auto loaded = MaterialSerializer::Load(filePath)) {
                                    materialComp->material = loaded;
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }
                        DrawMaterialFields(mat, nullptr);

                        if (ImGui::Button("Save Material As...##material")) {
                            saveMaterialBuffer[0] = '\0';
                            saveMaterialError.clear();
                            ImGui::OpenPopup("Save Material##popup");
                        }
                        DrawSaveMaterialPopup(mat);
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

            if (auto* anim = selectedNode->GetComponent<AnimationComponent>()) {
                ImGui::Spacing();
                ImGui::Text("Animation:");
                ImGui::Indent();

                for (const AnimationClip& clip : anim->GetClips()) {
                    bool isCurrent = anim->GetCurrentClip() == clip.name;
                    ImGui::PushID(clip.name.c_str());
                    ImGui::BulletText("%s (%.2fs)", clip.name.c_str(), clip.duration);

                    bool firstOnRow = true;
                    if (isCurrent && anim->IsPlaying()) {
                        ImGuiLayoutUtils::SameLineOrWrap(60.0f, firstOnRow); firstOnRow = false;
                        if (ImGui::SmallButton("Pause")) anim->Pause();
                    } else if (isCurrent && anim->IsPaused()) {
                        ImGuiLayoutUtils::SameLineOrWrap(60.0f, firstOnRow); firstOnRow = false;
                        if (ImGui::SmallButton("Resume")) anim->Resume();
                    } else {
                        ImGuiLayoutUtils::SameLineOrWrap(50.0f, firstOnRow); firstOnRow = false;
                        if (ImGui::SmallButton("Play")) anim->Play(clip.name, anim->IsLooping());
                    }

                    if (isCurrent) {
                        ImGuiLayoutUtils::SameLineOrWrap(60.0f, false);
                        if (ImGui::SmallButton("Restart")) anim->Restart();
                    }

                    bool isEditing = editingClipName == clip.name;
                    ImGuiLayoutUtils::SameLineOrWrap(50.0f, false);
                    if (ImGui::SmallButton(isEditing ? "Editing" : "Edit")) {
                        editingClipName = isEditing ? "" : clip.name;
                        editorScrubTime = 0.0f;
                    }
                    ImGui::PopID();
                }

                if (ImGui::Button("New Clip...##animation")) {
                    newClipNameBuffer[0] = '\0';
                    ImGui::OpenPopup("New Animation Clip##popup");
                }
                DrawNewAnimationClipPopup(anim);

                bool looping = anim->IsLooping();
                if (ImGui::Checkbox("Loop##animation", &looping)) {
                    anim->SetLooping(looping);
                }

                float speed = anim->GetSpeed();
                if (ImGui::DragFloat("Speed##animation", &speed, 0.05f, 0.0f, 5.0f)) {
                    anim->SetSpeed(speed);
                }

                bool playOnStart = anim->GetPlayOnStart();
                if (ImGui::Checkbox("Play On Start##animation", &playOnStart)) {
                    anim->SetPlayOnStart(playOnStart, anim->GetPlayOnStartClip());
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Auto-plays a clip when the scene loads, without needing code.\nIgnored if this component has a State Machine (its initial state\nplays automatically instead).");
                }
                if (playOnStart) {
                    ImGui::Indent();
                    ImGui::SetNextItemWidth(160);
                    std::string startClip = anim->GetPlayOnStartClip();
                    if (ImGui::BeginCombo("Clip##playOnStart", startClip.empty() ? "(none)" : startClip.c_str())) {
                        for (const AnimationClip& clip : anim->GetClips()) {
                            if (ImGui::Selectable(clip.name.c_str(), clip.name == startClip)) {
                                anim->SetPlayOnStart(true, clip.name);
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::Unindent();
                }

                if (!anim->GetCurrentClip().empty()) {
                    float duration = 0.0f;
                    for (const AnimationClip& clip : anim->GetClips()) {
                        if (clip.name == anim->GetCurrentClip()) { duration = clip.duration; break; }
                    }
                    float progress = duration > 0.0f ? anim->GetCurrentTime() / duration : 0.0f;
                    ImGui::ProgressBar(progress, ImVec2(-1, 0),
                        (anim->GetCurrentClip() + " " + std::to_string(anim->GetCurrentTime()).substr(0, 4) + "s").c_str());
                }

                if (!editingClipName.empty()) {
                    const AnimationClip* editingClip = nullptr;
                    for (const AnimationClip& clip : anim->GetClips()) {
                        if (clip.name == editingClipName) { editingClip = &clip; break; }
                    }

                    if (editingClip) {
                        ImGui::Separator();
                        ImGui::Text("Editing: %s", editingClipName.c_str());

                        // DragFloat, not SliderFloat: a slider's max hard-caps the value you
                        // can drag to, which would make it impossible to ever place a
                        // keyframe past the clip's current duration (0s for a brand-new
                        // clip) -- there'd be no way to extend it. v_max=0 here means no
                        // upper bound at all (Ctrl+Click still works to type an exact value).
                        if (ImGui::DragFloat("Time##kfeditor", &editorScrubTime, 0.05f, 0.0f, 0.0f, "%.2fs")) {
                            editorScrubTime = std::max(editorScrubTime, 0.0f);
                            anim->PreviewPose(editingClipName, editorScrubTime);
                        }

                        if (ImGui::Button("Add Keyframe##kfeditor")) {
                            anim->SetKeyframe(editingClipName, editorScrubTime, selectedNode->transform);
                        }

                        ImGui::Spacing();
                        ImGui::TextDisabled("Keyframes:");
                        for (float t : anim->GetKeyframeTimes(editingClipName)) {
                            std::string label = std::to_string(t).substr(0, 5) + "s";
                            ImGui::BulletText("%s", label.c_str());

                            ImGui::Indent();
                            if (ImGui::SmallButton(("Go##kf" + std::to_string(t)).c_str())) {
                                editorScrubTime = t;
                                anim->PreviewPose(editingClipName, t);
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton(("Update##kf" + std::to_string(t)).c_str())) {
                                anim->SetKeyframe(editingClipName, t, selectedNode->transform);
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton(("Delete##kf" + std::to_string(t)).c_str())) {
                                anim->RemoveKeyframe(editingClipName, t);
                            }
                            ImGui::Unindent();
                        }
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                DrawStateMachineEditor(anim);

                ImGui::Unindent();
            }

            if (auto* patrol = selectedNode->GetComponent<PatrolComponent>()) {
                ImGui::Spacing();
                ImGui::Text("Patrol:");
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove##patrol")) {
                    if (activeScene) {
                        bool stillAnimated = selectedNode->GetComponent<AnimationComponent>() != nullptr
                            || selectedNode->GetComponent<CameraPathComponent>() != nullptr;
                        if (!stillAnimated) activeScene->UnregisterAnimator(selectedNode);
                    }
                    selectedNode->RemoveComponent<PatrolComponent>();
                } else {
                    ImGui::Indent();

                    bool patrolActive = patrol->IsActive();
                    if (ImGui::Checkbox("Active##patrol", &patrolActive)) {
                        patrol->SetActive(patrolActive);
                    }

                    float patrolSpeed = patrol->GetSpeed();
                    if (ImGui::DragFloat("Speed##patrol", &patrolSpeed, 0.05f, 0.0f, 50.0f)) {
                        patrol->SetSpeed(patrolSpeed);
                    }

                    float turnSpeed = patrol->GetTurnSpeed();
                    if (ImGui::DragFloat("Turn Speed##patrol", &turnSpeed, 1.0f, 0.0f, 720.0f)) {
                        patrol->SetTurnSpeed(turnSpeed);
                    }

                    float forwardOffset = patrol->GetForwardOffset();
                    if (ImGui::DragFloat("Forward Offset##patrol", &forwardOffset, 1.0f, -180.0f, 180.0f, "%.0f deg")) {
                        patrol->SetForwardOffset(forwardOffset);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("If the character walks facing backward or sideways relative to\nits travel direction, adjust this (e.g. 180 to flip front/back).");
                    }

                    ImGui::Text("Waypoint %d/%d  %s", patrol->GetCurrentWaypointIndex() + 1,
                        static_cast<int>(patrol->GetWaypoints().size()), patrol->IsPaused() ? "(paused)" : "");

                    if (ImGui::Button("Add Waypoint at Current Position##patrol")) {
                        patrol->AddWaypoint(selectedNode->transform.position, 1.0f);
                    }

                    ImGui::Spacing();
                    ImGui::TextDisabled("Waypoints:");
                    auto& waypoints = patrol->GetWaypointsMutable();
                    int removeIndex = -1;
                    for (size_t i = 0; i < waypoints.size(); ++i) {
                        ImGui::PushID(static_cast<int>(i));
                        PatrolWaypoint& wp = waypoints[i];
                        ImGui::DragFloat3("Position##wp", &wp.position.x, 0.1f);
                        ImGui::SetNextItemWidth(80);
                        ImGui::DragFloat("Pause##wp", &wp.pauseSeconds, 0.05f, 0.0f, 60.0f);
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Remove##wp")) {
                            removeIndex = static_cast<int>(i);
                        }
                        ImGui::PopID();
                    }
                    if (removeIndex >= 0) {
                        patrol->RemoveWaypoint(static_cast<size_t>(removeIndex));
                    }

                    ImGui::Unindent();
                }
            }

            if (auto* cameraPath = selectedNode->GetComponent<CameraPathComponent>()) {
                ImGui::Spacing();
                ImGui::Text("Camera Path:");
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove##camerapath")) {
                    if (activeScene) {
                        bool stillAnimated = selectedNode->GetComponent<AnimationComponent>() != nullptr
                            || selectedNode->GetComponent<PatrolComponent>() != nullptr;
                        if (!stillAnimated) activeScene->UnregisterAnimator(selectedNode);
                    }
                    selectedNode->RemoveComponent<CameraPathComponent>();
                } else {
                    ImGui::Indent();

                    if (!selectedNode->GetComponent<CameraComponent>()) {
                        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                            "No Camera component on this node -- path will move it\nbut nothing will look through it.");
                    }

                    bool loop = cameraPath->IsLooping();
                    if (ImGui::Checkbox("Loop##camerapath", &loop)) {
                        cameraPath->SetLooping(loop);
                    }

                    ImGui::SameLine();
                    if (cameraPath->IsPlaying()) {
                        if (ImGui::SmallButton("Stop##camerapath")) cameraPath->Stop();
                    } else {
                        if (ImGui::SmallButton("Play##camerapath")) cameraPath->Play();
                    }

                    ImGui::Text("Segment %d/%d", cameraPath->GetCurrentSegment() + 1,
                        static_cast<int>(cameraPath->GetPoints().size()));

                    if (ImGui::Button("Add Point at Current Position##camerapath")) {
                        glm::vec3 lookAt = selectedNode->transform.position + glm::vec3(0.0f, 0.0f, -1.0f);
                        if (auto* camera = selectedNode->GetComponent<CameraComponent>()) {
                            lookAt = selectedNode->transform.position + camera->GetForward();
                        }
                        cameraPath->AddPoint(selectedNode->transform.position, lookAt, 2.0f, 0.0f);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Look-at defaults to the camera's current facing direction\n(or forward along -Z if this node has no Camera component).");
                    }

                    ImGui::Spacing();
                    ImGui::TextDisabled("Points:");
                    auto& points = cameraPath->GetPointsMutable();
                    int removePointIndex = -1;
                    for (size_t i = 0; i < points.size(); ++i) {
                        ImGui::PushID(static_cast<int>(i));
                        CameraPathPoint& point = points[i];

                        ImGui::Text("Point %d", static_cast<int>(i));
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Go##camerapath")) {
                            selectedNode->transform.position = point.position;
                            if (auto* camera = selectedNode->GetComponent<CameraComponent>()) {
                                glm::vec3 dir = point.lookAt - point.position;
                                if (glm::length(dir) > 1e-4f) {
                                    dir = glm::normalize(dir);
                                    camera->yaw = glm::degrees(std::atan2(dir.z, dir.x));
                                    camera->pitch = glm::degrees(std::asin(std::clamp(dir.y, -1.0f, 1.0f)));
                                }
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Remove##camerapath")) {
                            removePointIndex = static_cast<int>(i);
                        }

                        ImGui::DragFloat3("Position##camerapath", &point.position.x, 0.1f);
                        ImGui::DragFloat3("Look At##camerapath", &point.lookAt.x, 0.1f);

                        ImGui::SetNextItemWidth(90);
                        ImGui::DragFloat("Travel (s)##camerapath", &point.travelSeconds, 0.05f, 0.01f, 60.0f);
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(90);
                        ImGui::DragFloat("Hold (s)##camerapath", &point.holdSeconds, 0.05f, 0.0f, 60.0f);

                        ImGui::Separator();
                        ImGui::PopID();
                    }
                    if (removePointIndex >= 0) {
                        cameraPath->RemovePoint(static_cast<size_t>(removePointIndex));
                    }

                    ImGui::Unindent();
                }
            }

            // Shared texture drop-target for the VFX components below: drag a
            // texture asset from the Project browser onto the button to assign.
            auto vfxTextureSlot = [&](const char* label, std::shared_ptr<Texture>& slot, std::string& pathOut) {
                std::string buttonLabel = std::string(label) + ": " + (slot ? "Yes" : "None");
                ImGui::Button(buttonLabel.c_str(), ImVec2(200, 0));
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ProjectBrowser::kTexturePayloadType)) {
                        std::string filePath(static_cast<const char*>(payload->Data));
                        std::string texName = filePath;
                        std::replace(texName.begin(), texName.end(), '/', '_');
                        std::replace(texName.begin(), texName.end(), '\\', '_');
                        slot = ResourceManager::LoadTexture(texName, filePath);
                        pathOut = filePath;
                    }
                    ImGui::EndDragDropTarget();
                }
                if (slot) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton((std::string("Clear##") + label).c_str())) {
                        slot = nullptr;
                        pathOut.clear();
                    }
                }
            };

            if (auto* billboard = selectedNode->GetComponent<BillboardComponent>()) {
                ImGui::Spacing();
                ImGui::Text("Billboard:");
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove##billboard")) {
                    selectedNode->RemoveComponent<BillboardComponent>();
                } else {
                    ImGui::Indent();
                    vfxTextureSlot("Texture##billboard", billboard->texture, billboard->texturePath);
                    ImGui::DragFloat2("Size##billboard", &billboard->size.x, 0.05f, 0.01f, 100.0f);
                    ImGui::ColorEdit4("Tint##billboard", &billboard->tint.x);
                    ImGui::DragFloat("Alpha Cutoff##billboard", &billboard->alphaCutoff, 0.01f, 0.0f, 1.0f);
                    ImGui::Unindent();
                }
            }

            if (auto* grass = selectedNode->GetComponent<GrassComponent>()) {
                ImGui::Spacing();
                ImGui::Text("Grass:");
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove##grass")) {
                    selectedNode->RemoveComponent<GrassComponent>();
                } else {
                    ImGui::Indent();
                    vfxTextureSlot("Texture##grass", grass->texture, grass->texturePath);

                    bool needsRebuild = false;
                    needsRebuild |= ImGui::DragFloat2("Area Size##grass", &grass->areaSize.x, 0.5f, 1.0f, 500.0f);
                    needsRebuild |= ImGui::DragInt("Density##grass", &grass->density, 5.0f, 0, 20000);
                    int seedInt = static_cast<int>(grass->seed);
                    if (ImGui::DragInt("Seed##grass", &seedInt, 1.0f, 0, 1000000)) {
                        grass->seed = static_cast<unsigned int>(std::max(0, seedInt));
                        needsRebuild = true;
                    }
                    ImGui::DragFloat2("Blade Size##grass", &grass->bladeSize.x, 0.01f, 0.01f, 10.0f);
                    ImGui::ColorEdit3("Tint##grass", &grass->tint.x);
                    ImGui::DragFloat("Alpha Cutoff##grass", &grass->alphaCutoff, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("Wind Strength##grass", &grass->windStrength, 0.01f, 0.0f, 2.0f);
                    ImGui::DragFloat("Wind Speed##grass", &grass->windSpeed, 0.05f, 0.0f, 20.0f);

                    if (needsRebuild) grass->Rebuild();
                    if (ImGui::Button("Rebuild##grass")) grass->Rebuild();
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%d blades)", grass->density);
                    ImGui::Unindent();
                }
            }

            if (auto* particles = selectedNode->GetComponent<ParticleSystemComponent>()) {
                ImGui::Spacing();
                ImGui::Text("Particle System:");
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove##particles")) {
                    if (activeScene) {
                        bool stillAnimated = selectedNode->GetComponent<AnimationComponent>() != nullptr
                            || selectedNode->GetComponent<PatrolComponent>() != nullptr
                            || selectedNode->GetComponent<CameraPathComponent>() != nullptr;
                        if (!stillAnimated) activeScene->UnregisterAnimator(selectedNode);
                    }
                    selectedNode->RemoveComponent<ParticleSystemComponent>();
                } else {
                    ImGui::Indent();

                    const char* presetLabels[] = { "Custom", "Fire", "Smoke", "Sparks" };
                    int presetIdx = static_cast<int>(particles->preset);
                    ImGui::SetNextItemWidth(140);
                    if (ImGui::Combo("Preset##particles", &presetIdx, presetLabels, IM_ARRAYSIZE(presetLabels))) {
                        particles->ApplyPreset(static_cast<ParticleSystemComponent::Preset>(presetIdx));
                    }

                    const char* blendLabels[] = { "Alpha", "Additive" };
                    int blendIdx = static_cast<int>(particles->blendMode);
                    ImGui::SetNextItemWidth(140);
                    if (ImGui::Combo("Blend##particles", &blendIdx, blendLabels, IM_ARRAYSIZE(blendLabels))) {
                        particles->blendMode = static_cast<ParticleSystemComponent::BlendMode>(blendIdx);
                    }

                    ImGui::Checkbox("Playing##particles", &particles->playing);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%d live)", particles->GetLiveCount());

                    vfxTextureSlot("Texture##particles", particles->texture, particles->texturePath);

                    ImGui::DragInt("Max##particles", &particles->maxParticles, 5.0f, 1, 100000);
                    ImGui::DragFloat("Emit Rate##particles", &particles->emitRate, 1.0f, 0.0f, 5000.0f);
                    ImGui::DragFloat("Lifetime##particles", &particles->lifetime, 0.05f, 0.05f, 60.0f);
                    ImGui::DragFloat("Lifetime Spread##particles", &particles->lifetimeSpread, 0.02f, 0.0f, 10.0f);
                    ImGui::DragFloat3("Start Velocity##particles", &particles->startVelocity.x, 0.05f);
                    ImGui::DragFloat3("Velocity Spread##particles", &particles->velocitySpread.x, 0.05f, 0.0f, 20.0f);
                    ImGui::DragFloat3("Gravity##particles", &particles->gravity.x, 0.05f);
                    ImGui::DragFloat("Emit Radius##particles", &particles->emitRadius, 0.01f, 0.0f, 20.0f);
                    ImGui::ColorEdit4("Start Color##particles", &particles->startColor.x);
                    ImGui::ColorEdit4("End Color##particles", &particles->endColor.x);
                    ImGui::DragFloat("Start Size##particles", &particles->startSize, 0.01f, 0.0f, 20.0f);
                    ImGui::DragFloat("End Size##particles", &particles->endSize, 0.01f, 0.0f, 20.0f);

                    ImGui::Unindent();
                }
            }

            if (auto* terrain = selectedNode->GetComponent<TerrainComponent>()) {
                ImGui::Spacing();
                ImGui::Text("Terrain:");
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove##terrain")) {
                    selectedNode->RemoveComponent<TerrainComponent>();
                } else {
                    ImGui::Indent();

                    // Heightmap drop target (path only -- the terrain reloads it
                    // itself with stb to read raw heights, no GPU texture kept).
                    std::string hmLabel = "Heightmap: " +
                        (terrain->GetHeightmapPath().empty() ? std::string("None") : terrain->GetHeightmapPath());
                    ImGui::Button(hmLabel.c_str(), ImVec2(240, 0));
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ProjectBrowser::kTexturePayloadType)) {
                            std::string filePath(static_cast<const char*>(payload->Data));
                            terrain->SetHeightmap(filePath);
                            terrain->Generate();
                        }
                        ImGui::EndDragDropTarget();
                    }

                    ImGui::DragFloat("Size##terrain", &terrain->size, 0.5f, 1.0f, 2000.0f);
                    ImGui::DragInt("Resolution##terrain", &terrain->resolution, 1.0f, 2, 1024);
                    ImGui::DragFloat("Height Scale##terrain", &terrain->heightScale, 0.1f, 0.0f, 500.0f);

                    if (ImGui::Button("Generate##terrain")) {
                        terrain->Generate();
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Rebuilds the terrain mesh from the heightmap + parameters.\nDrag a grayscale image from the Project browser onto Heightmap first.");
                    }
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
        ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetWindowSize().x - totalWidth) * 0.5f));

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
        ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetWindowSize().x - totalWidth) * 0.5f));

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
            if (current->GetComponent<AnimationComponent>() || current->GetComponent<PatrolComponent>()
                || current->GetComponent<CameraPathComponent>() || current->GetComponent<ParticleSystemComponent>()) {
                activeScene->UnregisterAnimator(current);
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

bool DebugUI::DrawComponentItems(TNode* node, Scene* activeScene) {
    if (!node) return false;
    bool added = false;

    // --- Rendering ---
    if (!node->GetComponent<MeshComponent>() && ImGui::MenuItem("Mesh (Cube)")) {
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
            node->AddComponent<MaterialComponent>(std::make_shared<Material>(ResourceManager::LoadShader("pbr")));
        }
        added = true;
    }

    if (!node->GetComponent<MaterialComponent>() && ImGui::MenuItem("Material")) {
        node->AddComponent<MaterialComponent>(std::make_shared<Material>(ResourceManager::LoadShader("pbr")));
        added = true;
    }

    if (!node->GetComponent<TerrainComponent>() && ImGui::MenuItem("Terrain")) {
        node->AddComponent<TerrainComponent>(node);
        added = true;
    }

    ImGui::Separator();

    // --- Scene ---
    if (!node->GetComponent<CameraComponent>() && ImGui::MenuItem("Camera")) {
        node->AddComponent<CameraComponent>(node);
        if (activeScene) activeScene->RegisterCamera(node);
        added = true;
    }

    if (!node->GetComponent<LightComponent>() && ImGui::BeginMenu("Light")) {
        if (ImGui::MenuItem("Directional")) {
            node->AddComponent<LightComponent>(node, LightType::Directional);
            if (activeScene) activeScene->RegisterLight(node);
            added = true;
        }
        if (ImGui::MenuItem("Point")) {
            node->AddComponent<LightComponent>(node, LightType::Point);
            if (activeScene) activeScene->RegisterLight(node);
            added = true;
        }
        if (ImGui::MenuItem("Spot")) {
            node->AddComponent<LightComponent>(node, LightType::Spot);
            if (activeScene) activeScene->RegisterLight(node);
            added = true;
        }
        ImGui::EndMenu();
    }

    ImGui::Separator();

    // --- VFX ---
    if (!node->GetComponent<BillboardComponent>() && ImGui::MenuItem("Billboard")) {
        node->AddComponent<BillboardComponent>();
        added = true;
    }

    if (!node->GetComponent<GrassComponent>() && ImGui::MenuItem("Grass")) {
        node->AddComponent<GrassComponent>();
        added = true;
    }

    if (!node->GetComponent<ParticleSystemComponent>() && ImGui::MenuItem("Particle System")) {
        auto* ps = node->AddComponent<ParticleSystemComponent>();
        ps->ApplyPreset(ParticleSystemComponent::Preset::Fire);
        if (activeScene) activeScene->RegisterAnimator(node);
        added = true;
    }

    ImGui::Separator();

    // --- Logic / animation ---
    if (!node->GetComponent<AnimationComponent>() && ImGui::MenuItem("Animation")) {
        node->AddComponent<AnimationComponent>(node);
        if (activeScene) activeScene->RegisterAnimator(node);
        added = true;
    }

    if (!node->GetComponent<PatrolComponent>() && ImGui::MenuItem("Patrol")) {
        node->AddComponent<PatrolComponent>(node);
        if (activeScene) activeScene->RegisterAnimator(node);
        added = true;
    }

    if (!node->GetComponent<CameraPathComponent>() && ImGui::MenuItem("Camera Path")) {
        node->AddComponent<CameraPathComponent>(node);
        if (activeScene) activeScene->RegisterAnimator(node);
        added = true;
    }

    return added;
}

void DebugUI::DrawAddComponentMenu(TNode* node, Scene* activeScene) {
    if (!node) return;
    if (ImGui::BeginMenu("Add Component")) {
        DrawComponentItems(node, activeScene);
        ImGui::EndMenu();
    }
}

void DebugUI::DrawCreateWithComponentMenu(TNode* parent, Scene* activeScene) {
    if (!parent) return;
    if (ImGui::BeginMenu("Create with Component")) {
        // Run the shared component list against a detached scratch node. Only if
        // an item is actually picked (DrawComponentItems returns true) do we
        // parent + keep it -- so navigating the menu without picking creates
        // nothing and never mutates the live tree. The RegisterCamera/Light/
        // Animator calls inside DrawComponentItems just add the node to the
        // scene's tracking lists (they don't require it to be in the tree yet),
        // so registering here and parenting immediately after is consistent.
        TNode* scratch = new TNode(nullptr, "Object");
        if (DrawComponentItems(scratch, activeScene)) {
            parent->addChild(scratch);
            SelectNode(scratch);
        } else {
            delete scratch;
        }
        ImGui::EndMenu();
    }
}

void DebugUI::DrawMaterialFields(const std::shared_ptr<Material>& mat, const std::string* assetPath) {
    if (!mat) return;

    bool changed = false;
    changed |= ImGui::ColorEdit4("Base Color##material", &mat->baseColor.x);

    auto dropTextureSlot = [](const char* label, std::shared_ptr<Texture>& slot) {
        bool dropped = false;
        std::string buttonLabel = std::string(label) + ": " + (slot ? "Yes" : "None");
        ImGui::Button(buttonLabel.c_str(), ImVec2(220, 0));
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ProjectBrowser::kTexturePayloadType)) {
                std::string filePath(static_cast<const char*>(payload->Data));
                std::string texName = filePath;
                std::replace(texName.begin(), texName.end(), '/', '_');
                std::replace(texName.begin(), texName.end(), '\\', '_');
                slot = ResourceManager::LoadTexture(texName, filePath);
                dropped = true;
            }
            ImGui::EndDragDropTarget();
        }
        return dropped;
    };

    changed |= dropTextureSlot("Albedo Texture", mat->albedo);
    if (mat->albedo) {
        ImGui::Indent();
        ImGui::BulletText("%dx%d", mat->albedo->GetWidth(), mat->albedo->GetHeight());
        ImGui::Unindent();
    }
    changed |= dropTextureSlot("Normal Texture", mat->normal);
    changed |= dropTextureSlot("MetallicRoughness Texture", mat->metallicRoughness);
    changed |= ImGui::SliderFloat("Metallic##material", &mat->metallicFactor, 0.0f, 1.0f);
    changed |= ImGui::SliderFloat("Roughness##material", &mat->roughnessFactor, 0.0f, 1.0f);
    changed |= ImGui::Checkbox("Transparent##material", &mat->transparent);

    changed |= ImGui::ColorEdit3("Emissive Color##material", &mat->emissiveColor.x);
    changed |= ImGui::DragFloat("Emissive Intensity##material", &mat->emissiveIntensity, 0.05f, 0.0f, 50.0f);

    if (changed && assetPath) {
        MaterialSerializer::Save(mat, *assetPath);
    }
}

void DebugUI::DrawSaveMaterialPopup(const std::shared_ptr<Material>& material) {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Save Material##popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetNextItemWidth(240);
        bool commit = ImGui::InputText("##SaveMaterialInput", saveMaterialBuffer, sizeof(saveMaterialBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        if (!saveMaterialError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", saveMaterialError.c_str());
        }

        bool confirmed = ImGui::Button("OK") || commit;
        ImGui::SameLine();
        bool cancelled = ImGui::Button("Cancel");

        if (confirmed) {
            std::string name = saveMaterialBuffer;
            if (name.empty()) {
                saveMaterialError = "Name cannot be empty.";
            } else {
                std::string filePath = "resources/materials/" + name + ".material";
                if (MaterialSerializer::Save(material, filePath)) {
                    ImGui::CloseCurrentPopup();
                } else {
                    saveMaterialError = "Failed to save material.";
                }
            }
        } else if (cancelled) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void DebugUI::DrawStateMachineEditor(AnimationComponent* anim) {
    ImGui::Spacing();
    AnimationStateMachine* machine = anim->GetStateMachine();

    if (!machine) {
        ImGui::TextDisabled("No state machine.");
        ImGui::SameLine();
        if (ImGui::SmallButton("Add State Machine##sm")) {
            anim->GetOrCreateStateMachine();
        }
        return;
    }

    ImGui::Text("State Machine:");
    ImGui::SameLine();
    if (ImGui::SmallButton("Remove##sm")) {
        anim->RemoveStateMachine();
        return;
    }
    ImGui::Indent();

    if (!machine->GetCurrentState().empty()) {
        ImGui::TextDisabled("Current: %s", machine->GetCurrentState().c_str());
    }

    // --- States ---
    ImGui::TextDisabled("States:");
    auto& states = machine->GetStatesMutable();
    int removeStateIndex = -1;
    for (size_t i = 0; i < states.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        AnimationStateMachine::State& state = states[i];

        char nameBuf[128];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", state.name.c_str());
        ImGui::SetNextItemWidth(140);
        // Commit the rename on deactivation (not per-keystroke): RenameState
        // rewrites every transition/initialState reference, so doing it while
        // the user is mid-typing would churn references against half-typed
        // names. Note InputText returns false on the deactivation frame, so
        // check IsItemDeactivatedAfterEdit separately rather than && with it.
        ImGui::InputText("##stateName", nameBuf, sizeof(nameBuf));
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            machine->RenameState(state.name, nameBuf);
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        if (ImGui::BeginCombo("##stateClip", state.clipName.empty() ? "(clip)" : state.clipName.c_str())) {
            for (const AnimationClip& clip : anim->GetClips()) {
                bool isSelected = clip.name == state.clipName;
                if (ImGui::Selectable(clip.name.c_str(), isSelected)) {
                    state.clipName = clip.name;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        ImGui::Checkbox("Loop##state", &state.loop);

        ImGui::SameLine();
        if (ImGui::SmallButton("X##removeState")) {
            removeStateIndex = static_cast<int>(i);
        }

        ImGui::PopID();
    }
    if (removeStateIndex >= 0) {
        machine->RemoveState(states[static_cast<size_t>(removeStateIndex)].name);
    }

    ImGui::SetNextItemWidth(160);
    ImGui::InputText("##newStateName", newStateNameBuffer, sizeof(newStateNameBuffer));
    ImGui::SameLine();
    if (ImGui::SmallButton("Add State##sm") && newStateNameBuffer[0] != '\0') {
        std::string defaultClip = anim->GetClips().empty() ? "" : anim->GetClips().front().name;
        machine->AddState(newStateNameBuffer, defaultClip, true);
        newStateNameBuffer[0] = '\0';
    }

    // --- Initial state ---
    ImGui::SetNextItemWidth(160);
    std::string initial = machine->GetInitialState();
    if (ImGui::BeginCombo("Initial State##sm", initial.empty() ? "(none)" : initial.c_str())) {
        for (const auto& state : states) {
            bool isSelected = state.name == initial;
            if (ImGui::Selectable(state.name.c_str(), isSelected)) {
                machine->SetInitialState(state.name);
            }
        }
        ImGui::EndCombo();
    }

    // --- Transitions ---
    ImGui::Spacing();
    ImGui::TextDisabled("Transitions:");
    auto& transitions = machine->GetTransitionsMutable();
    int removeTransitionIndex = -1;

    static const char* opLabels[] = { ">", "<", "==", "!=" };

    for (size_t i = 0; i < transitions.size(); ++i) {
        ImGui::PushID(static_cast<int>(1000 + i));
        AnimationStateMachine::Transition& transition = transitions[i];

        ImGui::SetNextItemWidth(110);
        if (ImGui::BeginCombo("##fromState", transition.fromState.empty() ? "Any State" : transition.fromState.c_str())) {
            if (ImGui::Selectable("Any State", transition.fromState.empty())) transition.fromState = "";
            for (const auto& state : states) {
                if (ImGui::Selectable(state.name.c_str(), transition.fromState == state.name)) {
                    transition.fromState = state.name;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        ImGui::Text("->");
        ImGui::SameLine();

        ImGui::SetNextItemWidth(110);
        if (ImGui::BeginCombo("##toState", transition.toState.empty() ? "(none)" : transition.toState.c_str())) {
            for (const auto& state : states) {
                if (ImGui::Selectable(state.name.c_str(), transition.toState == state.name)) {
                    transition.toState = state.name;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("X##removeTransition")) {
            removeTransitionIndex = static_cast<int>(i);
        }

        char paramBuf[128];
        std::snprintf(paramBuf, sizeof(paramBuf), "%s", transition.parameter.c_str());
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputText("Param##transition", paramBuf, sizeof(paramBuf))) {
            transition.parameter = paramBuf;
        }

        ImGui::SameLine();
        int opIndex = static_cast<int>(transition.op);
        ImGui::SetNextItemWidth(60);
        if (ImGui::Combo("##transitionOp", &opIndex, opLabels, IM_ARRAYSIZE(opLabels))) {
            transition.op = static_cast<AnimationStateMachine::ConditionOp>(opIndex);
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::DragFloat("Threshold##transition", &transition.threshold, 0.05f);

        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("Blend (s)##transition", &transition.blendSeconds, 0.02f, 0.0f, 5.0f);

        ImGui::Separator();
        ImGui::PopID();
    }
    if (removeTransitionIndex >= 0) {
        machine->RemoveTransition(static_cast<size_t>(removeTransitionIndex));
    }

    if (ImGui::SmallButton("Add Transition##sm") && !states.empty()) {
        machine->AddTransition("", states.front().name, "", AnimationStateMachine::ConditionOp::Equals, 1.0f, 0.2f);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Parameter names are arbitrary strings set at runtime via code\n(AnimationStateMachine::SetBool/SetFloat), e.g. \"isMoving\".");
    }

    ImGui::Unindent();
}

void DebugUI::DrawNewAnimationClipPopup(AnimationComponent* anim) {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("New Animation Clip##popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetNextItemWidth(240);
        bool commit = ImGui::InputText("##NewClipInput", newClipNameBuffer, sizeof(newClipNameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        bool confirmed = ImGui::Button("OK") || commit;
        ImGui::SameLine();
        bool cancelled = ImGui::Button("Cancel");

        if (confirmed && newClipNameBuffer[0] != '\0') {
            AnimationClip clip;
            clip.name = newClipNameBuffer;
            anim->AddClip(clip);
            editingClipName = clip.name;
            editorScrubTime = 0.0f;
            ImGui::CloseCurrentPopup();
        } else if (cancelled) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void DebugUI::DrawCreatePrefabPopup() {
    if (!creatingPrefabFrom) return;

    ImGui::OpenPopup("Create Prefab##popup");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Create Prefab##popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetNextItemWidth(240);
        bool commit = ImGui::InputText("##CreatePrefabInput", createPrefabBuffer, sizeof(createPrefabBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        if (!createPrefabError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", createPrefabError.c_str());
        }

        bool confirmed = ImGui::Button("OK") || commit;
        ImGui::SameLine();
        bool cancelled = ImGui::Button("Cancel");

        if (confirmed) {
            std::string name = createPrefabBuffer;
            if (name.empty()) {
                createPrefabError = "Name cannot be empty.";
            } else {
                std::string filePath = "resources/prefabs/" + name + ".prefab";
                if (PrefabSerializer::Save(creatingPrefabFrom, filePath)) {
                    creatingPrefabFrom = nullptr;
                    ImGui::CloseCurrentPopup();
                } else {
                    createPrefabError = "Failed to save prefab.";
                }
            }
        } else if (cancelled) {
            creatingPrefabFrom = nullptr;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
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

    // Spawns a primitive node, places it at the spawn point, parents+selects it.
    auto spawnPrimitive = [&](TNode* node) {
        placeAtSpawnPoint(node);
        parent->addChild(node);
        SelectNode(node);
    };

    if (ImGui::BeginMenu("3D Object")) {
        if (ImGui::MenuItem("Cube"))     spawnPrimitive(SpawnCubeNode());
        if (ImGui::MenuItem("Sphere"))   spawnPrimitive(SpawnSphereNode());
        if (ImGui::MenuItem("Plane"))    spawnPrimitive(SpawnPlaneNode());
        if (ImGui::MenuItem("Cylinder")) spawnPrimitive(SpawnCylinderNode());
        if (ImGui::MenuItem("Cone"))     spawnPrimitive(SpawnConeNode());
        ImGui::EndMenu();
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
        ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetWindowSize().x - totalWidth) * 0.5f));

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
        ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetWindowSize().x - totalWidth) * 0.5f));

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

    auto buttonWidth = [](const char* label) {
        return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    };

    if (ImGui::Button("New Scene##btn")) {
        static int sceneCounter = 1;
        std::string newSceneName = "Scene_" + std::to_string(sceneCounter++);
        sceneManager->CreateScene(newSceneName);
        sceneManager->LoadScene(newSceneName);
        EngineSettings::SetLastActiveScene(newSceneName);
        EngineConfig::Save();
    }

    ImGuiLayoutUtils::SameLineOrWrap(buttonWidth("Save"), false);
    if (ImGui::Button("Save##btn")) {
        if (sceneManager->GetActiveScene()) {
            static int saveCounter = 0;
            std::string fileName = "scenes/" + sceneManager->GetActiveSceneName() + "_" + std::to_string(saveCounter++) + ".scene";
            std::filesystem::create_directories("scenes");
            SceneSerializer::SaveScene(sceneManager->GetActiveScene(), fileName);
        }
    }

    ImGuiLayoutUtils::SameLineOrWrap(buttonWidth("Delete"), false);
    if (ImGui::Button("Delete##btn")) {
        if (sceneManager->GetActiveScene()) {
            sceneToDelete = sceneManager->GetActiveSceneName();
            showDeleteConfirm = true;
            ImGui::OpenPopup("Delete Scene Confirmation");
        }
    }

    ImGuiLayoutUtils::SameLineOrWrap(buttonWidth("Rename"), false);
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
