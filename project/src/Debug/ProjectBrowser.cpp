#include "Debug/ProjectBrowser.h"
#include "Debug/DebugUI.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/TNode.h"
#include "Scene/MaterialComponent.h"
#include "Scene/GLTFLoader.h"
#include "Scene/PrefabSerializer.h"
#include "Core/EngineSettings.h"
#include "Core/EngineConfig.h"
#include "ResourceManager/Material.h"
#include "ResourceManager/MaterialSerializer.h"
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/Texture.h"
#include "ResourceManager/OpenGLShader.h"
#include "Debug/MaterialIcons.h"
#include "Debug/ImGuiLayoutUtils.h"
#include <imgui.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cctype>
#include <ctime>
#include <algorithm>
#include <chrono>
#include <vector>

namespace {

enum class AssetKind { Folder, Scene, Shader, Model, Texture, Material, Prefab, Other };

AssetKind ClassifyPath(const std::filesystem::path& path) {
    if (std::filesystem::is_directory(path)) return AssetKind::Folder;

    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

    if (ext == ".scene") return AssetKind::Scene;
    if (ext == ".vert" || ext == ".frag" || ext == ".geom") return AssetKind::Shader;
    if (ext == ".glb" || ext == ".gltf") return AssetKind::Model;
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") return AssetKind::Texture;
    if (ext == ".material") return AssetKind::Material;
    if (ext == ".prefab") return AssetKind::Prefab;
    return AssetKind::Other;
}

ImVec4 ColorForKind(AssetKind kind) {
    switch (kind) {
        case AssetKind::Folder:   return ImVec4(0.85f, 0.75f, 0.35f, 1.0f);
        case AssetKind::Scene:    return ImVec4(0.35f, 0.65f, 0.90f, 1.0f);
        case AssetKind::Shader:   return ImVec4(0.75f, 0.45f, 0.85f, 1.0f);
        case AssetKind::Model:    return ImVec4(0.40f, 0.80f, 0.55f, 1.0f);
        case AssetKind::Texture:  return ImVec4(0.90f, 0.60f, 0.35f, 1.0f);
        case AssetKind::Material: return ImVec4(0.85f, 0.40f, 0.45f, 1.0f);
        case AssetKind::Prefab:   return ImVec4(0.45f, 0.55f, 0.90f, 1.0f);
        default:                  return ImVec4(0.55f, 0.55f, 0.58f, 1.0f);
    }
}

// Icon glyph (Material Symbols, see MaterialIcons.h) shown while DebugUI::GetIconFont()
// is the active font. Textures use their own thumbnail instead (see DrawItem) --
// ICON_IMAGE only applies as a fallback if that thumbnail fails to load.
const char* IconForKind(AssetKind kind) {
    switch (kind) {
        case AssetKind::Folder:   return ICON_FOLDER;
        case AssetKind::Scene:    return ICON_SCENE;
        case AssetKind::Shader:   return ICON_CODE;
        case AssetKind::Model:    return ICON_MODEL;
        case AssetKind::Texture:  return ICON_IMAGE;
        case AssetKind::Material: return ICON_PALETTE;
        case AssetKind::Prefab:   return ICON_MODEL;
        default:                  return ICON_FILE;
    }
}

constexpr float kCellWidth = 78.0f;
constexpr float kSwatchSize = 56.0f;

bool NameContainsFilter(const std::string& name, const std::string& filter) {
    if (filter.empty()) return true;
    auto toLower = [](const std::string& s) {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
        return out;
    };
    return toLower(name).find(toLower(filter)) != std::string::npos;
}

} // namespace

std::filesystem::path ProjectBrowser::currentDir = "resources";
std::filesystem::path ProjectBrowser::selectedPath;
std::vector<std::filesystem::path> ProjectBrowser::multiSelectedPaths;
bool ProjectBrowser::showMultiDeleteConfirm = false;

bool ProjectBrowser::renamingItem = false;
std::filesystem::path ProjectBrowser::renameTarget;
char ProjectBrowser::renameBuffer[256] = "";
std::string ProjectBrowser::renameError = "";

bool ProjectBrowser::showDeleteConfirm = false;
std::filesystem::path ProjectBrowser::deleteTarget;
std::string ProjectBrowser::deleteError = "";

bool ProjectBrowser::creatingFolder = false;
char ProjectBrowser::newFolderBuffer[128] = "";
std::string ProjectBrowser::newFolderError = "";

char ProjectBrowser::searchFilter[128] = "";

int ProjectBrowser::sortMode = 0;
bool ProjectBrowser::sortAscending = true;

std::vector<std::string> ProjectBrowser::pendingDrops;
std::filesystem::path ProjectBrowser::importConflictSrc;
bool ProjectBrowser::importApplyToAll = false;
int ProjectBrowser::importRememberedChoice = -1;

namespace {

// Derives a stable, filesystem-safe cache name from a path (same scheme DebugUI uses),
// so thumbnail loads share ResourceManager's texture cache with material-slot loads.
std::string TextureCacheName(const std::filesystem::path& path) {
    std::string name = path.string();
    std::replace(name.begin(), name.end(), '/', '_');
    std::replace(name.begin(), name.end(), '\\', '_');
    return name;
}

std::string HumanFileSize(std::uintmax_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    double size = static_cast<double>(bytes);
    int unit = 0;
    while (size >= 1024.0 && unit < 3) {
        size /= 1024.0;
        ++unit;
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), unit == 0 ? "%.0f %s" : "%.1f %s", size, units[unit]);
    return buf;
}

} // namespace

void ProjectBrowser::Draw(SceneManager* sceneManager, Scene* activeScene) {
    ProcessPendingDrops();

    DrawRoots();
    DrawBreadcrumb();

    DrawToolbar(sceneManager);

    ImGui::Separator();

    constexpr float kDetailsWidth = 220.0f;
    // Below this width, a fixed 220px side panel would squeeze the grid to
    // nothing (or negative width) -- stack the details panel under the grid
    // instead, and only when something's actually selected to show there.
    bool sideBySide = ImGui::GetContentRegionAvail().x >= kDetailsWidth * 2.0f;

    ImVec2 gridSize = sideBySide ? ImVec2(-kDetailsWidth, 0) : ImVec2(0, selectedPath.empty() ? 0 : -150.0f);
    ImGui::BeginChild("ProjectGrid", gridSize, true);
    DrawGrid(sceneManager, activeScene);
    bool gridFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    ImGui::EndChild();

    // Delete key removes the selected asset(s), mirroring the Scene Tree's Delete-key
    // handling. Skipped while a text field (rename/search/new folder) has keyboard
    // focus, so Delete still works as expected while typing.
    if (gridFocused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        if (!multiSelectedPaths.empty()) {
            showMultiDeleteConfirm = true;
        } else if (!selectedPath.empty()) {
            showDeleteConfirm = true;
            deleteTarget = selectedPath;
        }
    }

    if (sideBySide) {
        ImGui::SameLine();
        ImGui::BeginChild("AssetDetails", ImVec2(0, 0), true);
        DrawDetailsPanel();
        ImGui::EndChild();
    } else if (!selectedPath.empty()) {
        ImGui::BeginChild("AssetDetails", ImVec2(0, 0), true);
        DrawDetailsPanel();
        ImGui::EndChild();
    }

    DrawRenamePopup(sceneManager);
    DrawDeleteConfirm(sceneManager);
    DrawMultiDeleteConfirm(sceneManager);
    DrawNewFolderPopup();
    DrawImportConflictPopup();
}

void ProjectBrowser::ToggleAssetInMultiSelect(const std::filesystem::path& path) {
    // Starting a fresh multi-select: seed it with whatever was already selected.
    if (multiSelectedPaths.empty() && !selectedPath.empty() && selectedPath != path) {
        multiSelectedPaths.push_back(selectedPath);
    }

    auto it = std::find(multiSelectedPaths.begin(), multiSelectedPaths.end(), path);
    if (it != multiSelectedPaths.end()) {
        multiSelectedPaths.erase(it);
    } else {
        multiSelectedPaths.push_back(path);
    }

    if (multiSelectedPaths.size() == 1) {
        selectedPath = multiSelectedPaths[0];
        multiSelectedPaths.clear();
    } else if (multiSelectedPaths.empty()) {
        selectedPath.clear();
    } else {
        selectedPath = path;
    }
}

bool ProjectBrowser::IsAssetMultiSelected(const std::filesystem::path& path) {
    return std::find(multiSelectedPaths.begin(), multiSelectedPaths.end(), path) != multiSelectedPaths.end();
}

void ProjectBrowser::EnqueueDroppedPaths(int count, const char** paths) {
    for (int i = 0; i < count; ++i) {
        if (paths[i]) pendingDrops.emplace_back(paths[i]);
    }
}

void ProjectBrowser::ImportOne(const std::filesystem::path& src, bool keepBoth) {
    std::error_code ec;
    std::filesystem::path dest = currentDir / src.filename();

    if (keepBoth && std::filesystem::exists(dest, ec)) {
        std::string stem = src.stem().string();
        std::string ext = src.extension().string();
        int suffix = 1;
        do {
            dest = currentDir / (stem + "_" + std::to_string(suffix++) + ext);
        } while (std::filesystem::exists(dest, ec));
    }

    auto options = std::filesystem::copy_options::overwrite_existing;
    if (std::filesystem::is_directory(src, ec)) {
        std::filesystem::copy(src, dest, options | std::filesystem::copy_options::recursive, ec);
    } else {
        std::filesystem::copy_file(src, dest, options, ec);
    }
}

void ProjectBrowser::ProcessPendingDrops() {
    // A conflict is currently being resolved by the modal -- wait for it.
    if (!importConflictSrc.empty()) return;
    if (pendingDrops.empty()) return;

    std::error_code ec;
    while (!pendingDrops.empty()) {
        std::filesystem::path src(pendingDrops.front());

        if (!std::filesystem::exists(src, ec) || !std::filesystem::exists(currentDir, ec)) {
            pendingDrops.erase(pendingDrops.begin());
            continue;
        }

        bool collides = std::filesystem::exists(currentDir / src.filename(), ec);
        if (!collides) {
            ImportOne(src, false);
            pendingDrops.erase(pendingDrops.begin());
            continue;
        }

        // Name collision. Use a remembered "apply to all" choice if the user set one,
        // otherwise raise the modal and stop until it resolves this source.
        if (importApplyToAll && importRememberedChoice != -1) {
            if (importRememberedChoice == 0) ImportOne(src, false);      // overwrite
            else if (importRememberedChoice == 1) ImportOne(src, true);  // keep both
            // choice 2 (cancel) skips it
            pendingDrops.erase(pendingDrops.begin());
            continue;
        }

        importConflictSrc = src;
        return; // DrawImportConflictPopup() takes over
    }

    // Batch fully drained -- reset the per-batch "apply to all" memory.
    importApplyToAll = false;
    importRememberedChoice = -1;
}

void ProjectBrowser::DrawRoots() {
    if (ImGui::Button("Resources##root")) {
        currentDir = "resources";
        selectedPath.clear();
        multiSelectedPaths.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Scenes##root")) {
        currentDir = "resources/scenes";
        selectedPath.clear();
        multiSelectedPaths.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Materials##root")) {
        std::filesystem::create_directories("resources/materials");
        currentDir = "resources/materials";
        selectedPath.clear();
        multiSelectedPaths.clear();
    }
}

void ProjectBrowser::DrawBreadcrumb() {
    ImGuiLayoutUtils::SameLineOrWrap(ImGui::CalcTextSize("|").x, false);
    ImGui::TextDisabled("|");

    auto buttonWidth = [](const std::string& label) {
        return ImGui::CalcTextSize(label.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    };

    // Each path segment is a clickable button that jumps to that ancestor folder.
    // Wraps to a new breadcrumb line instead of overflowing for deeply nested paths.
    std::filesystem::path accumulated;
    int index = 0;
    for (auto it = currentDir.begin(); it != currentDir.end(); ++it, ++index) {
        accumulated /= *it;
        std::string segment = it->string();

        if (index > 0) {
            ImGuiLayoutUtils::SameLineOrWrap(ImGui::CalcTextSize("/").x, false);
            ImGui::TextUnformatted("/");
        }
        ImGuiLayoutUtils::SameLineOrWrap(buttonWidth(segment), false);
        std::string label = segment + "##crumb" + std::to_string(index);
        if (ImGui::SmallButton(label.c_str())) {
            currentDir = accumulated;
            selectedPath.clear();
            multiSelectedPaths.clear();
        }
    }

    ImGuiLayoutUtils::SameLineOrWrap(buttonWidth("Up"), false);
    if (ImGui::SmallButton("Up##breadcrumb")) {
        std::filesystem::path parent = currentDir.parent_path();
        if (!parent.empty() && parent != currentDir) {
            currentDir = parent;
            selectedPath.clear();
            multiSelectedPaths.clear();
        }
    }
}

static bool CurrentDirHasSegment(const std::filesystem::path& dir, const char* segment) {
    for (auto it = dir.begin(); it != dir.end(); ++it) {
        if (*it == segment) return true;
    }
    return false;
}

void ProjectBrowser::CreateNewScene(SceneManager* sceneManager) {
    int sceneCounter = 1;
    std::string newSceneName = "Scene_" + std::to_string(sceneCounter);
    while (sceneManager->SceneFileExists(newSceneName)) {
        newSceneName = "Scene_" + std::to_string(++sceneCounter);
    }
    DebugUI::QueueSceneAction([sceneManager, newSceneName]() {
        sceneManager->NewScene(newSceneName);
        EngineSettings::SetLastActiveScene(newSceneName);
        EngineConfig::Save();
    });
}

void ProjectBrowser::CreateNewMaterial() {
    static int materialCounter = 1;
    std::string newMaterialName = "Material_" + std::to_string(materialCounter++);
    std::string destPath = (currentDir / (newMaterialName + ".material")).string();
    while (std::filesystem::exists(destPath)) {
        newMaterialName = "Material_" + std::to_string(materialCounter++);
        destPath = (currentDir / (newMaterialName + ".material")).string();
    }
    auto material = std::make_shared<Material>(ResourceManager::LoadShader("pbr"));
    MaterialSerializer::Save(material, destPath);
}

void ProjectBrowser::OpenNewFolderPopup() {
    // Only raises the flag; DrawNewFolderPopup() (called every frame from Draw)
    // issues the actual OpenPopup. This avoids ID-stack issues when triggered
    // from inside a context-menu popup that's about to close.
    newFolderBuffer[0] = '\0';
    newFolderError.clear();
    creatingFolder = true;
}

void ProjectBrowser::DrawToolbar(SceneManager* sceneManager) {
    bool insideScenes = CurrentDirHasSegment(currentDir, "scenes");
    bool insideMaterials = CurrentDirHasSegment(currentDir, "materials");

    auto buttonWidth = [](const char* label) {
        return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    };

    bool isFirst = true;

    if (insideScenes) {
        if (ImGui::Button("New Scene##project")) {
            CreateNewScene(sceneManager);
        }
        isFirst = false;
    }

    if (insideMaterials) {
        ImGuiLayoutUtils::SameLineOrWrap(buttonWidth("New Material"), isFirst);
        if (ImGui::Button("New Material##project")) {
            CreateNewMaterial();
        }
        isFirst = false;
    }

    ImGuiLayoutUtils::SameLineOrWrap(buttonWidth("New Folder"), isFirst);
    if (ImGui::Button("New Folder##project")) {
        OpenNewFolderPopup();
    }

    ImGuiLayoutUtils::SameLineOrWrap(200.0f, false);
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##ProjectSearch", "Search...", searchFilter, sizeof(searchFilter));

    ImGuiLayoutUtils::SameLineOrWrap(110.0f, false);
    ImGui::SetNextItemWidth(110);
    const char* sortLabels[] = {"Name", "Type", "Date"};
    ImGui::Combo("##ProjectSort", &sortMode, sortLabels, IM_ARRAYSIZE(sortLabels));
    ImGuiLayoutUtils::SameLineOrWrap(buttonWidth("Desc##sort"), false);
    if (ImGui::SmallButton(sortAscending ? "Asc##sort" : "Desc##sort")) {
        sortAscending = !sortAscending;
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
        // Folders always show (so filtering never blocks navigation); files are
        // filtered by name substring when a search filter is active.
        if (!entry.is_directory() && !NameContainsFilter(entry.path().filename().string(), searchFilter)) {
            continue;
        }
        entries.push_back(entry);
    }
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        // Folders always come first, regardless of sort mode/direction.
        bool aDir = a.is_directory(), bDir = b.is_directory();
        if (aDir != bDir) return aDir > bDir;

        bool less;
        switch (sortMode) {
            case 1: { // Type: order by AssetKind, tie-break by name.
                AssetKind ka = ClassifyPath(a.path()), kb = ClassifyPath(b.path());
                if (ka != kb) less = static_cast<int>(ka) < static_cast<int>(kb);
                else less = a.path().filename().string() < b.path().filename().string();
                break;
            }
            case 2: { // Date modified.
                std::error_code ec;
                auto ta = std::filesystem::last_write_time(a.path(), ec);
                auto tb = std::filesystem::last_write_time(b.path(), ec);
                less = ta < tb;
                break;
            }
            default: // Name.
                less = a.path().filename().string() < b.path().filename().string();
                break;
        }
        return sortAscending ? less : !less;
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

    // Right-click on empty space in the grid: create/paste actions for the
    // current folder (ImGuiPopupFlags_NoOpenOverItems keeps per-item menus winning).
    if (ImGui::BeginPopupContextWindow("GridBackgroundMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        DrawBackgroundContextMenu(sceneManager);
        ImGui::EndPopup();
    }
}

void ProjectBrowser::DrawItem(const std::filesystem::directory_entry& entry, SceneManager* sceneManager, Scene* activeScene) {
    const std::filesystem::path& path = entry.path();
    AssetKind kind = ClassifyPath(path);
    std::string idSuffix = "##" + path.string();
    std::string name = path.filename().string();

    ImGui::BeginGroup();
    ImGui::PushID(idSuffix.c_str());

    // Textures show their actual image as a thumbnail; everything else shows a
    // colored swatch with a type icon (Material Symbols). Thumbnail loads hit the
    // shared ResourceManager texture cache (keyed by the path-derived name), so
    // this is a cheap map lookup after the first frame.
    std::shared_ptr<Texture> thumbnail;
    if (kind == AssetKind::Texture) {
        thumbnail = ResourceManager::LoadTexture(TextureCacheName(path), path.string());
    }

    if (thumbnail) {
        ImGui::ImageButton(idSuffix.c_str(),
                           (ImTextureID)(intptr_t)thumbnail->GetID(),
                           ImVec2(kSwatchSize, kSwatchSize));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ColorForKind(kind));
        ImVec4 hoverColor = ColorForKind(kind);
        hoverColor.x = std::min(1.0f, hoverColor.x + 0.1f);
        hoverColor.y = std::min(1.0f, hoverColor.y + 0.1f);
        hoverColor.z = std::min(1.0f, hoverColor.z + 0.1f);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);

        if (ImFont* iconFont = DebugUI::GetIconFont()) ImGui::PushFont(iconFont);
        ImGui::Button(IconForKind(kind), ImVec2(kSwatchSize, kSwatchSize));
        if (DebugUI::GetIconFont()) ImGui::PopFont();

        ImGui::PopStyleColor(2);
    }

    // Selection highlight: a border around the swatch/thumbnail for the single
    // selection or any item in the active multi-selection.
    bool isSelected = (path == selectedPath) || IsAssetMultiSelected(path);
    if (isSelected) {
        ImVec2 minR = ImGui::GetItemRectMin();
        ImVec2 maxR = ImGui::GetItemRectMax();
        ImU32 accent = ImGui::GetColorU32(ImGuiCol_NavHighlight);
        ImGui::GetWindowDrawList()->AddRect(minR, maxR, accent, 3.0f, 0, 2.0f);
    }

    if (ImGui::IsItemClicked()) {
        if (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift) {
            ToggleAssetInMultiSelect(path);
        } else {
            multiSelectedPaths.clear();
            selectedPath = path;
            if (kind == AssetKind::Material) {
                DebugUI::SelectMaterialAsset(path.string());
            }
        }
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        HandleActivate(path, sceneManager, activeScene);
    }

    if (kind != AssetKind::Folder) {
        if (ImGui::BeginDragDropSource()) {
            std::string pathStr = path.string();
            if (kind == AssetKind::Texture) {
                ImGui::SetDragDropPayload(ProjectBrowser::kTexturePayloadType, pathStr.c_str(), pathStr.size() + 1);
            } else if (kind == AssetKind::Material) {
                ImGui::SetDragDropPayload(ProjectBrowser::kMaterialPayloadType, pathStr.c_str(), pathStr.size() + 1);
            } else if (kind == AssetKind::Prefab) {
                ImGui::SetDragDropPayload(ProjectBrowser::kPrefabPayloadType, pathStr.c_str(), pathStr.size() + 1);
            }
            ImGui::SetDragDropPayload(ProjectBrowser::kAssetMovePayloadType, pathStr.c_str(), pathStr.size() + 1);
            ImGui::Text("%s", name.c_str());
            ImGui::EndDragDropSource();
        }
    }

    if (kind == AssetKind::Folder) {
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ProjectBrowser::kAssetMovePayloadType)) {
                std::filesystem::path src(static_cast<const char*>(payload->Data));
                std::filesystem::path dest = path / src.filename();
                if (src != dest) {
                    std::error_code ec;
                    std::filesystem::rename(src, dest, ec);
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    if (ImGui::BeginPopupContextItem("ItemContextMenu")) {
        if (multiSelectedPaths.empty() || !IsAssetMultiSelected(path)) {
            multiSelectedPaths.clear();
            selectedPath = path;
        }
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

void ProjectBrowser::DrawDetailsPanel() {
    ImGui::TextDisabled("Details");
    ImGui::Separator();

    if (selectedPath.empty()) {
        ImGui::TextDisabled("No asset selected.");
        return;
    }

    AssetKind kind = ClassifyPath(selectedPath);
    std::error_code ec;

    // Large type icon next to the name (skipped for textures -- they get their
    // own real preview image further down instead).
    if (kind != AssetKind::Texture) {
        if (ImFont* iconFont = DebugUI::GetIconFont()) {
            ImGui::PushFont(iconFont, 32.0f);
            ImGui::TextUnformatted(IconForKind(kind));
            ImGui::PopFont();
            ImGui::SameLine();
        }
    }
    ImGui::TextWrapped("%s", selectedPath.filename().string().c_str());
    ImGui::Spacing();

    const char* kindNames[] = {"Folder", "Scene", "Shader", "Model", "Texture", "Material", "Prefab", "File"};
    int kindIdx = std::min(static_cast<int>(kind), 7);
    ImGui::Text("Type: %s", kindNames[kindIdx]);

    if (!std::filesystem::is_directory(selectedPath, ec)) {
        std::uintmax_t size = std::filesystem::file_size(selectedPath, ec);
        if (!ec) ImGui::Text("Size: %s", HumanFileSize(size).c_str());
    }

    // Last-modified time, formatted as a local date/time string.
    auto ftime = std::filesystem::last_write_time(selectedPath, ec);
    if (!ec) {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
        char buf[64];
        if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&cftime))) {
            ImGui::Text("Modified: %s", buf);
        }
    }

    ImGui::Spacing();

    if (kind == AssetKind::Texture) {
        if (auto tex = ResourceManager::LoadTexture(TextureCacheName(selectedPath), selectedPath.string())) {
            ImGui::Text("%d x %d", tex->GetWidth(), tex->GetHeight());
            float avail = ImGui::GetContentRegionAvail().x;
            float side = std::min(avail, 180.0f);
            ImGui::Image((ImTextureID)(intptr_t)tex->GetID(), ImVec2(side, side));
        }
    } else if (kind == AssetKind::Material) {
        if (auto mat = MaterialSerializer::Load(selectedPath.string())) {
            ImGui::ColorButton("##matcolor", ImVec4(mat->baseColor.r, mat->baseColor.g, mat->baseColor.b, mat->baseColor.a));
            ImGui::SameLine();
            ImGui::Text("Base Color");
            ImGui::Text("Metallic: %.2f", mat->metallicFactor);
            ImGui::Text("Roughness: %.2f", mat->roughnessFactor);
            ImGui::Text("Transparent: %s", mat->transparent ? "yes" : "no");
            ImGui::Spacing();
            ImGui::TextDisabled("Click to edit in Inspector.");
        }
    }
}

void ProjectBrowser::HandleActivate(const std::filesystem::path& path, SceneManager* sceneManager, Scene* activeScene) {
    if (std::filesystem::is_directory(path)) {
        currentDir = path;
        selectedPath.clear();
        multiSelectedPaths.clear();
        return;
    }

    switch (ClassifyPath(path)) {
        case AssetKind::Scene: {
            std::string sceneName = path.stem().string();
            DebugUI::QueueSceneAction([sceneManager, sceneName]() {
                sceneManager->LoadScene(sceneName);
                EngineSettings::SetLastActiveScene(sceneName);
                EngineConfig::Save();
            });
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
        case AssetKind::Prefab: {
            if (activeScene) {
                if (TNode* instance = PrefabSerializer::Instantiate(path.string(), activeScene)) {
                    activeScene->GetRoot()->addChild(instance);
                }
            }
            break;
        }
        default:
            break;
    }
}

void ProjectBrowser::DrawContextMenu(const std::filesystem::path& path, SceneManager* sceneManager) {
    if (!multiSelectedPaths.empty()) {
        if (ImGui::MenuItem(("Delete " + std::to_string(multiSelectedPaths.size()) + " Selected").c_str())) {
            showMultiDeleteConfirm = true;
        }
        return;
    }

    AssetKind kind = ClassifyPath(path);

    // --- Kind-specific "activate" action at the top ---
    switch (kind) {
        case AssetKind::Folder:
            if (ImGui::MenuItem("Open")) {
                currentDir = path;
                selectedPath.clear();
            }
            break;
        case AssetKind::Scene:
            if (ImGui::MenuItem("Load Scene")) {
                std::string sceneName = path.stem().string();
                DebugUI::QueueSceneAction([sceneManager, sceneName]() {
                    sceneManager->LoadScene(sceneName);
                    EngineSettings::SetLastActiveScene(sceneName);
                    EngineConfig::Save();
                });
            }
            break;
        case AssetKind::Model:
            if (ImGui::MenuItem("Import to Scene")) {
                if (Scene* active = SceneManager::Instance().GetActiveScene()) {
                    for (TNode* node : GLTFLoader::LoadModel(path.string())) {
                        active->AddNodeToRoot(node);
                    }
                }
            }
            break;
        case AssetKind::Material: {
            if (ImGui::MenuItem("Edit in Inspector")) {
                DebugUI::SelectMaterialAsset(path.string());
            }
            TNode* selected = DebugUI::GetSelectedNode();
            if (ImGui::MenuItem("Assign to Selected Object", nullptr, false, selected != nullptr)) {
                if (selected) {
                    if (auto loaded = MaterialSerializer::Load(path.string())) {
                        if (auto* matComp = selected->GetComponent<MaterialComponent>()) {
                            matComp->material = loaded;
                        } else {
                            selected->AddComponent<MaterialComponent>(loaded);
                        }
                    }
                }
            }
            break;
        }
        case AssetKind::Prefab:
            if (ImGui::MenuItem("Instantiate")) {
                if (Scene* active = SceneManager::Instance().GetActiveScene()) {
                    if (TNode* instance = PrefabSerializer::Instantiate(path.string(), active)) {
                        active->GetRoot()->addChild(instance);
                    }
                }
            }
            break;
        default:
            break;
    }

    // --- Rename / Duplicate (not for folders -- see plan's out-of-scope note) ---
    if (kind == AssetKind::Scene) {
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
            while (std::filesystem::exists(path.parent_path() / (candidate + ".scene"))) {
                candidate = baseName + "_copy" + std::to_string(++suffix);
            }
            std::filesystem::path destPath = path.parent_path() / (candidate + ".scene");
            std::error_code ec;
            std::filesystem::copy_file(path, destPath, ec);
            // The copy on disk is enough: the scene selector lists from disk, so
            // it shows up without needing to be loaded into memory.
        }
    } else if (kind != AssetKind::Folder) {
        // Generic file kinds (Material, Texture, Model, Shader, Other): plain
        // filesystem ops, no manager involved, preserving the real extension.
        if (ImGui::MenuItem("Rename")) {
            renamingItem = true;
            renameTarget = path;
            renameError = "";
            std::snprintf(renameBuffer, sizeof(renameBuffer), "%s", path.stem().string().c_str());
        }
        if (ImGui::MenuItem("Duplicate")) {
            std::string ext = path.extension().string();
            std::string baseName = path.stem().string();
            std::string candidate = baseName + "_copy";
            int suffix = 1;
            while (std::filesystem::exists(path.parent_path() / (candidate + ext))) {
                candidate = baseName + "_copy" + std::to_string(++suffix);
            }
            std::filesystem::path destPath = path.parent_path() / (candidate + ext);
            std::error_code ec;
            std::filesystem::copy_file(path, destPath, ec);
        }
    }

    // --- Common actions for every kind ---
    ImGui::Separator();
    if (ImGui::MenuItem("Reveal in Explorer")) {
        RevealInExplorer(path);
    }
    if (ImGui::MenuItem("Delete")) {
        showDeleteConfirm = true;
        deleteTarget = path;
    }
}

void ProjectBrowser::DrawBackgroundContextMenu(SceneManager* sceneManager) {
    bool insideScenes = CurrentDirHasSegment(currentDir, "scenes");
    bool insideMaterials = CurrentDirHasSegment(currentDir, "materials");

    if (insideScenes && ImGui::MenuItem("New Scene")) {
        CreateNewScene(sceneManager);
    }
    if (insideMaterials && ImGui::MenuItem("New Material")) {
        CreateNewMaterial();
    }
    if (ImGui::MenuItem("New Folder")) {
        OpenNewFolderPopup();
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Reveal Folder in Explorer")) {
        RevealInExplorer(currentDir);
    }
}

void ProjectBrowser::RevealInExplorer(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::path absolute = std::filesystem::absolute(path, ec);
    if (ec) absolute = path;

#if defined(_WIN32)
    // Windows Explorer wants backslashes; /select highlights the item (for files).
    std::string winPath = absolute.string();
    std::replace(winPath.begin(), winPath.end(), '/', '\\');
    std::string cmd = std::filesystem::is_directory(absolute)
        ? "explorer \"" + winPath + "\""
        : "explorer /select,\"" + winPath + "\"";
    std::system(cmd.c_str());
#elif defined(__APPLE__)
    std::system(("open \"" + absolute.string() + "\"").c_str());
#else
    std::system(("xdg-open \"" + absolute.string() + "\"").c_str());
#endif
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
            } else if (ClassifyPath(renameTarget) != AssetKind::Scene) {
                std::string ext = renameTarget.extension().string();
                std::filesystem::path destPath = renameTarget.parent_path() / (newName + ext);
                if (newName != oldName && std::filesystem::exists(destPath)) {
                    renameError = "An asset with that name already exists.";
                } else {
                    std::error_code ec;
                    if (newName != oldName) {
                        std::filesystem::rename(renameTarget, destPath, ec);
                    }
                    renamingItem = false;
                    ImGui::CloseCurrentPopup();
                }
            } else if (newName != oldName && sceneManager->SceneFileExists(newName)) {
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

bool ProjectBrowser::FolderContainsActiveScene(const std::filesystem::path& folderPath, SceneManager* sceneManager) {
    const std::string& activeName = sceneManager->GetActiveSceneName();
    if (activeName.empty()) return false;

    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath, ec)) {
        if (!entry.is_directory() && entry.path().extension() == ".scene" &&
            entry.path().stem().string() == activeName) {
            return true;
        }
    }
    return false;
}

std::string ProjectBrowser::DeleteOneAsset(const std::filesystem::path& target, SceneManager* sceneManager) {
    AssetKind kind = ClassifyPath(target);
    if (kind == AssetKind::Folder) {
        if (FolderContainsActiveScene(target, sceneManager)) {
            return "Cannot delete: contains the active scene.";
        }
        std::error_code ec;
        std::filesystem::remove_all(target, ec);
    } else if (kind == AssetKind::Scene) {
        std::string sceneName = target.stem().string();
        if (sceneName == sceneManager->GetActiveSceneName()) {
            // Deleting the active scene switches to another one, which destroys
            // the current scene -- defer to the end of the frame so the panels
            // still using it this frame don't dangle.
            DebugUI::QueueSceneAction([sceneManager, sceneName]() { sceneManager->UnloadScene(sceneName); });
        } else {
            // Non-active scene: UnloadScene just removes the file, no switch.
            sceneManager->UnloadScene(sceneName);
        }
    } else {
        std::error_code ec;
        std::filesystem::remove(target, ec);
    }
    return "";
}

void ProjectBrowser::DrawDeleteConfirm(SceneManager* sceneManager) {
    if (!showDeleteConfirm) return;

    ImGui::OpenPopup("Delete Asset?##popup");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Delete Asset?##popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Delete \"%s\"? This cannot be undone.", deleteTarget.filename().string().c_str());
        ImGui::Spacing();

        if (!deleteError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", deleteError.c_str());
        }

        float buttonWidth = 120.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float totalWidth = (buttonWidth * 2) + spacing;
        ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetWindowSize().x - totalWidth) * 0.5f));

        if (ImGui::Button("Delete Forever", ImVec2(buttonWidth, 0))) {
            std::string err = DeleteOneAsset(deleteTarget, sceneManager);
            if (!err.empty()) {
                deleteError = err;
            } else {
                showDeleteConfirm = false;
                deleteError.clear();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            showDeleteConfirm = false;
            deleteError.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ProjectBrowser::DrawMultiDeleteConfirm(SceneManager* sceneManager) {
    if (!showMultiDeleteConfirm || multiSelectedPaths.empty()) return;

    ImGui::OpenPopup("Delete Assets?##popup");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Delete Assets?##popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Delete %zu selected items? This cannot be undone.", multiSelectedPaths.size());
        ImGui::Spacing();

        if (!deleteError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", deleteError.c_str());
        }

        float buttonWidth = 120.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float totalWidth = (buttonWidth * 2) + spacing;
        ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetWindowSize().x - totalWidth) * 0.5f));

        if (ImGui::Button("Delete Forever##multi", ImVec2(buttonWidth, 0))) {
            std::vector<std::filesystem::path> toDelete = multiSelectedPaths;
            std::string lastError;
            for (const auto& target : toDelete) {
                std::string err = DeleteOneAsset(target, sceneManager);
                if (!err.empty()) lastError = err;
            }
            multiSelectedPaths.clear();
            selectedPath.clear();
            showMultiDeleteConfirm = false;
            deleteError = lastError;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##multi", ImVec2(buttonWidth, 0))) {
            showMultiDeleteConfirm = false;
            deleteError.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ProjectBrowser::DrawNewFolderPopup() {
    if (!creatingFolder) return;

    ImGui::OpenPopup("New Folder##popup");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("New Folder##popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetNextItemWidth(240);
        bool commit = ImGui::InputText("##NewFolderInput", newFolderBuffer, sizeof(newFolderBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        if (!newFolderError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", newFolderError.c_str());
        }

        bool confirmed = ImGui::Button("OK") || commit;
        ImGui::SameLine();
        bool cancelled = ImGui::Button("Cancel");

        if (confirmed) {
            std::string name = newFolderBuffer;
            if (name.empty()) {
                newFolderError = "Name cannot be empty.";
            } else if (std::filesystem::exists(currentDir / name)) {
                newFolderError = "A folder with that name already exists.";
            } else {
                std::error_code ec;
                std::filesystem::create_directory(currentDir / name, ec);
                creatingFolder = false;
                ImGui::CloseCurrentPopup();
            }
        } else if (cancelled) {
            creatingFolder = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ProjectBrowser::DrawImportConflictPopup() {
    if (importConflictSrc.empty()) return;

    ImGui::OpenPopup("Import Conflict##popup");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Import Conflict##popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("\"%s\" already exists in this folder.", importConflictSrc.filename().string().c_str());
        ImGui::Spacing();

        // Offer "apply to all" only when more than one file is still pending.
        if (pendingDrops.size() > 1) {
            ImGui::Checkbox("Apply to all remaining", &importApplyToAll);
            ImGui::Spacing();
        }

        // Resolves the current conflict with the given choice (0=overwrite, 1=keep both,
        // 2=cancel), consumes it from the queue, and remembers it if "apply to all" is on.
        auto resolve = [](int choice) {
            std::filesystem::path src = importConflictSrc;
            if (choice == 0) ImportOne(src, false);
            else if (choice == 1) ImportOne(src, true);
            // choice == 2 (cancel/skip): do nothing

            if (!pendingDrops.empty()) pendingDrops.erase(pendingDrops.begin());
            if (importApplyToAll) importRememberedChoice = choice;
            importConflictSrc.clear();
            ImGui::CloseCurrentPopup();
        };

        float buttonWidth = 110.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float totalWidth = (buttonWidth * 3) + (spacing * 2);
        ImGui::SetCursorPosX(std::max(0.0f, (ImGui::GetWindowSize().x - totalWidth) * 0.5f));

        if (ImGui::Button("Overwrite", ImVec2(buttonWidth, 0))) resolve(0);
        ImGui::SameLine();
        if (ImGui::Button("Keep Both", ImVec2(buttonWidth, 0))) resolve(1);
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) resolve(2);

        ImGui::EndPopup();
    }
}
