#include "Debug/DebugUI.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/TNode.h"
#include "Scene/SceneSerializer.h"
#include "Scene/CameraComponent.h"
#include "Scene/MeshComponent.h"
#include "Scene/MaterialComponent.h"
#include "ResourceManager/Material.h"
#include "ResourceManager/Texture.h"
#include "ResourceManager/OpenGLShader.h"
#include "ResourceManager/ResourceManager.h"
#include <imgui.h>
#include <glm/glm.hpp>
#include <filesystem>
#include <cstdio>

TNode* DebugUI::m_selectedNode = nullptr;
bool DebugUI::m_showDeleteConfirm = false;
std::string DebugUI::m_sceneToDelete = "";

TNode* DebugUI::m_nodeToDelete = nullptr;
bool DebugUI::m_showNodeDeleteConfirm = false;

TNode* DebugUI::m_renamingNode = nullptr;
char DebugUI::m_renameBuffer[256] = "";
bool DebugUI::m_renameJustStarted = false;

void DebugUI::Init() {
    // ImGui context is already created by OpenGLRenderer
}

void DebugUI::Shutdown() {
    m_selectedNode = nullptr;
}

void DebugUI::DrawFrame(SceneManager* sceneManager) {
    if (!sceneManager) return;

    Scene* activeScene = sceneManager->GetActiveScene();
    if (!activeScene) return;

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
        m_selectedNode = node;
    }

    if (ImGui::BeginPopupContextItem(("NodeContextMenu" + idSuffix).c_str())) {
        m_selectedNode = node;

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
        // Unregister any cameras anywhere in this node's subtree before it's freed.
        std::vector<TNode*> stack = { node };
        while (!stack.empty()) {
            TNode* current = stack.back();
            stack.pop_back();
            if (current->GetComponent<CameraComponent>()) {
                activeScene->UnregisterCamera(current);
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
