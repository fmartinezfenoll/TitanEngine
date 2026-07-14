#include "Scene/PrefabSerializer.h"
#include "Scene/SceneSerializer.h"
#include "Scene/TNode.h"
#include "Core/Log.h"
#include <fstream>
#include <sstream>
#include <filesystem>

bool PrefabSerializer::Save(const TNode* node, const std::string& filePath) {
    if (!node) return false;

    std::filesystem::path path(filePath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream file(filePath);
    if (!file.is_open()) {
        Log::Error("PrefabSerializer: failed to open '" + filePath + "' for writing");
        return false;
    }

    file << SceneSerializer::SerializeNodeToString(node);
    return true;
}

TNode* PrefabSerializer::Instantiate(const std::string& filePath, Scene* scene) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        Log::Error("PrefabSerializer: cannot open '" + filePath + "'");
        return nullptr;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return SceneSerializer::DeserializeNodeFromString(buffer.str(), scene);
}
