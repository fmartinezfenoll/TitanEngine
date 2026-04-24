#include "Debug/DebugUI.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/TNode.h"
#include "Scene/SceneSerializer.h"
#include "Scene/SimpleEntities.h"
#include "ResourceManager/ResourceManager.h"
#include <imgui.h>
#include <glm/glm.hpp>
#include <filesystem>

TNode* DebugUI::m_selectedNode = nullptr;
bool DebugUI::m_showDeleteConfirm = false;
std::string DebugUI::m_sceneToDelete = "";

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
                        DrawSceneTree(child);
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

            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    DrawInspector();
    DrawDeleteConfirmation();
}

void DebugUI::DrawSceneTree(TNode* node, int depth) {
    if (!node) return;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    if (node->children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    if (node == m_selectedNode) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    std::string label;
    if (!node->name.empty()) {
        label = node->name;
    } else if (node->entity != nullptr) {
        if (dynamic_cast<TriangleEntity*>(node->entity)) {
            label = "Triangle";
        } else if (dynamic_cast<SquareEntity*>(node->entity)) {
            label = "Square";
        } else {
            label = "Entity";
        }
    } else {
        label = "Empty Node";
    }
    label += "##" + std::to_string(reinterpret_cast<uintptr_t>(node));

    bool opened = ImGui::TreeNodeEx(label.c_str(), flags);

    if (ImGui::IsItemClicked()) {
        m_selectedNode = node;
    }

    if (opened) {
        for (TNode* child : node->children) {
            DrawSceneTree(child, depth + 1);
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

    ImGui::Text("Node Type: %s", node->entity ? "Entity" : "Group");

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

void DebugUI::DrawInspector() {
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
        ImGui::Text("Entity:");
        ImGui::Separator();
        if (m_selectedNode->entity) {
            std::string entityType = "Unknown";
            if (dynamic_cast<TriangleEntity*>(m_selectedNode->entity)) {
                entityType = "Triangle";
            } else if (dynamic_cast<SquareEntity*>(m_selectedNode->entity)) {
                entityType = "Square";
            }
            ImGui::BulletText("Type: %s", entityType.c_str());
        } else {
            ImGui::BulletText("No entity (Empty Node)");
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
