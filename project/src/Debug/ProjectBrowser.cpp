#include "Debug/ProjectBrowser.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/TNode.h"
#include "Scene/GLTFLoader.h"
#include "Core/EngineSettings.h"
#include "Core/EngineConfig.h"
#include <imgui.h>
#include <cstdio>
#include <algorithm>
#include <vector>

namespace {

enum class AssetKind { Folder, Scene, Shader, Model, Texture, Other };

AssetKind ClassifyPath(const std::filesystem::path& path) {
    if (std::filesystem::is_directory(path)) return AssetKind::Folder;

    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

    if (ext == ".scene") return AssetKind::Scene;
    if (ext == ".vert" || ext == ".frag" || ext == ".geom") return AssetKind::Shader;
    if (ext == ".glb" || ext == ".gltf") return AssetKind::Model;
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") return AssetKind::Texture;
    return AssetKind::Other;
}

ImVec4 ColorForKind(AssetKind kind) {
    switch (kind) {
        case AssetKind::Folder:  return ImVec4(0.85f, 0.75f, 0.35f, 1.0f);
        case AssetKind::Scene:   return ImVec4(0.35f, 0.65f, 0.90f, 1.0f);
        case AssetKind::Shader:  return ImVec4(0.75f, 0.45f, 0.85f, 1.0f);
        case AssetKind::Model:   return ImVec4(0.40f, 0.80f, 0.55f, 1.0f);
        case AssetKind::Texture: return ImVec4(0.90f, 0.60f, 0.35f, 1.0f);
        default:                 return ImVec4(0.55f, 0.55f, 0.58f, 1.0f);
    }
}

const char* LabelForKind(AssetKind kind) {
    switch (kind) {
        case AssetKind::Folder:  return "DIR";
        case AssetKind::Scene:   return "SCN";
        case AssetKind::Shader:  return "GLSL";
        case AssetKind::Model:   return "MDL";
        case AssetKind::Texture: return "TEX";
        default:                 return "FILE";
    }
}

constexpr float kCellWidth = 78.0f;
constexpr float kSwatchSize = 56.0f;

} // namespace

std::filesystem::path ProjectBrowser::currentDir = "resources";
std::filesystem::path ProjectBrowser::selectedPath;

bool ProjectBrowser::renamingItem = false;
std::filesystem::path ProjectBrowser::renameTarget;
char ProjectBrowser::renameBuffer[256] = "";
std::string ProjectBrowser::renameError = "";

bool ProjectBrowser::showDeleteConfirm = false;
std::filesystem::path ProjectBrowser::deleteTarget;

void ProjectBrowser::Draw(SceneManager* sceneManager, Scene* activeScene) {
    DrawRoots();
    ImGui::SameLine();
    DrawBreadcrumb();

    DrawToolbar(sceneManager);

    ImGui::Separator();
    ImGui::BeginChild("ProjectGrid", ImVec2(0, 0), true);
    DrawGrid(sceneManager, activeScene);
    ImGui::EndChild();

    DrawRenamePopup(sceneManager);
    DrawDeleteConfirm(sceneManager);
}

void ProjectBrowser::DrawRoots() {
    if (ImGui::Button("Resources##root")) {
        currentDir = "resources";
        selectedPath.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Scenes##root")) {
        currentDir = "scenes";
        selectedPath.clear();
    }
}

void ProjectBrowser::DrawBreadcrumb() {
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextUnformatted(currentDir.generic_string().c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Up##breadcrumb")) {
        std::filesystem::path parent = currentDir.parent_path();
        if (!parent.empty() && parent != currentDir) {
            currentDir = parent;
            selectedPath.clear();
        }
    }
}

void ProjectBrowser::DrawToolbar(SceneManager* sceneManager) {
    bool insideScenes = false;
    for (auto it = currentDir.begin(); it != currentDir.end(); ++it) {
        if (*it == "scenes") { insideScenes = true; break; }
    }

    if (insideScenes) {
        if (ImGui::Button("New Scene##project")) {
            static int sceneCounter = 1;
            std::string newSceneName = "Scene_" + std::to_string(sceneCounter++);
            while (sceneManager->GetScene(newSceneName)) {
                newSceneName = "Scene_" + std::to_string(sceneCounter++);
            }
            sceneManager->CreateScene(newSceneName);
            sceneManager->LoadScene(newSceneName);
            EngineSettings::SetLastActiveScene(newSceneName);
            EngineConfig::Save();
        }
    }
}

void ProjectBrowser::DrawGrid(SceneManager* sceneManager, Scene* activeScene) {
    if (!std::filesystem::exists(currentDir)) {
        ImGui::TextDisabled("(folder does not exist)");
        return;
    }

    float availWidth = ImGui::GetContentRegionAvail().x;
    int columns = std::max(1, static_cast<int>(availWidth / kCellWidth));
    int column = 0;

    std::vector<std::filesystem::directory_entry> entries;
    for (const auto& entry : std::filesystem::directory_iterator(currentDir)) {
        entries.push_back(entry);
    }
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        bool aDir = a.is_directory(), bDir = b.is_directory();
        if (aDir != bDir) return aDir > bDir;
        return a.path().filename().string() < b.path().filename().string();
    });

    for (const auto& entry : entries) {
        DrawItem(entry, sceneManager, activeScene);
        ++column;
        if (column < columns) {
            ImGui::SameLine();
        } else {
            column = 0;
        }
    }
}

void ProjectBrowser::DrawItem(const std::filesystem::directory_entry& entry, SceneManager* sceneManager, Scene* activeScene) {
    const std::filesystem::path& path = entry.path();
    AssetKind kind = ClassifyPath(path);
    std::string idSuffix = "##" + path.string();
    std::string name = path.filename().string();

    ImGui::BeginGroup();
    ImGui::PushID(idSuffix.c_str());

    ImGui::PushStyleColor(ImGuiCol_Button, ColorForKind(kind));
    ImVec4 hoverColor = ColorForKind(kind);
    hoverColor.x = std::min(1.0f, hoverColor.x + 0.1f);
    hoverColor.y = std::min(1.0f, hoverColor.y + 0.1f);
    hoverColor.z = std::min(1.0f, hoverColor.z + 0.1f);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);

    ImGui::Button(LabelForKind(kind), ImVec2(kSwatchSize, kSwatchSize));

    ImGui::PopStyleColor(2);

    if (ImGui::IsItemClicked()) {
        selectedPath = path;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        HandleActivate(path, sceneManager, activeScene);
    }

    if (kind == AssetKind::Texture) {
        if (ImGui::BeginDragDropSource()) {
            std::string pathStr = path.string();
            ImGui::SetDragDropPayload(ProjectBrowser::kTexturePayloadType, pathStr.c_str(), pathStr.size() + 1);
            ImGui::Text("%s", name.c_str());
            ImGui::EndDragDropSource();
        }
    }

    if (ImGui::BeginPopupContextItem("ItemContextMenu")) {
        selectedPath = path;
        DrawContextMenu(path, sceneManager);
        ImGui::EndPopup();
    }

    ImGui::PopID();

    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kCellWidth - 8.0f);
    ImGui::TextWrapped("%s", name.c_str());
    ImGui::PopTextWrapPos();

    ImGui::Dummy(ImVec2(kCellWidth, 4.0f));
    ImGui::EndGroup();
}

void ProjectBrowser::HandleActivate(const std::filesystem::path& path, SceneManager* sceneManager, Scene* activeScene) {
    if (std::filesystem::is_directory(path)) {
        currentDir = path;
        selectedPath.clear();
        return;
    }

    switch (ClassifyPath(path)) {
        case AssetKind::Scene: {
            std::string sceneName = path.stem().string();
            if (!sceneManager->GetScene(sceneName)) {
                sceneManager->LoadSceneNamed(sceneName, path.string());
            }
            sceneManager->LoadScene(sceneName);
            EngineSettings::SetLastActiveScene(sceneName);
            EngineConfig::Save();
            break;
        }
        case AssetKind::Model: {
            if (activeScene) {
                for (TNode* node : GLTFLoader::LoadModel(path.string())) {
                    activeScene->AddNodeToRoot(node);
                }
            }
            break;
        }
        default:
            break;
    }
}

void ProjectBrowser::DrawContextMenu(const std::filesystem::path& path, SceneManager* sceneManager) {
    bool isScene = ClassifyPath(path) == AssetKind::Scene;

    if (isScene) {
        if (ImGui::MenuItem("Rename")) {
            renamingItem = true;
            renameTarget = path;
            renameError = "";
            std::snprintf(renameBuffer, sizeof(renameBuffer), "%s", path.stem().string().c_str());
        }

        if (ImGui::MenuItem("Duplicate")) {
            std::string baseName = path.stem().string();
            std::string candidate = baseName + "_copy";
            int suffix = 1;
            while (sceneManager->GetScene(candidate) || std::filesystem::exists(path.parent_path() / (candidate + ".scene"))) {
                candidate = baseName + "_copy" + std::to_string(++suffix);
            }
            std::filesystem::path destPath = path.parent_path() / (candidate + ".scene");
            std::error_code ec;
            std::filesystem::copy_file(path, destPath, ec);
            if (!ec) {
                sceneManager->LoadSceneNamed(candidate, destPath.string());
            }
        }

        if (ImGui::MenuItem("Delete")) {
            showDeleteConfirm = true;
            deleteTarget = path;
        }
    } else {
        ImGui::TextDisabled("No actions available");
    }
}

void ProjectBrowser::DrawRenamePopup(SceneManager* sceneManager) {
    if (!renamingItem) return;

    ImGui::OpenPopup("Rename Asset##popup");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Rename Asset##popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetNextItemWidth(240);
        bool commit = ImGui::InputText("##RenameAssetInput", renameBuffer, sizeof(renameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        if (!renameError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", renameError.c_str());
        }

        bool confirmed = ImGui::Button("OK") || commit;
        ImGui::SameLine();
        bool cancelled = ImGui::Button("Cancel");

        if (confirmed) {
            std::string newName = renameBuffer;
            std::string oldName = renameTarget.stem().string();
            if (newName.empty()) {
                renameError = "Name cannot be empty.";
            } else if (newName != oldName && sceneManager->GetScene(newName)) {
                renameError = "A scene with that name already exists.";
            } else {
                if (newName != oldName && sceneManager->RenameScene(oldName, newName)) {
                    if (sceneManager->GetActiveSceneName() == newName) {
                        EngineSettings::SetLastActiveScene(newName);
                        EngineConfig::Save();
                    }
                }
                renamingItem = false;
                ImGui::CloseCurrentPopup();
            }
        } else if (cancelled) {
            renamingItem = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ProjectBrowser::DrawDeleteConfirm(SceneManager* sceneManager) {
    if (!showDeleteConfirm) return;

    ImGui::OpenPopup("Delete Asset?##popup");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Delete Asset?##popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Delete \"%s\"? This cannot be undone.", deleteTarget.filename().string().c_str());
        ImGui::Spacing();

        if (ImGui::Button("Delete Forever", ImVec2(120, 0))) {
            sceneManager->UnloadScene(deleteTarget.stem().string());
            showDeleteConfirm = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showDeleteConfirm = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
