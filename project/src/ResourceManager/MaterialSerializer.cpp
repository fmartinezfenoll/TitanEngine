#include "ResourceManager/MaterialSerializer.h"
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/Material.h"
#include "ResourceManager/OpenGLShader.h"
#include "ResourceManager/Texture.h"
#include "Core/Log.h"
#include <json.hpp>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;

bool MaterialSerializer::Save(const std::shared_ptr<Material>& material, const std::string& filePath) {
    if (!material) return false;

    json j;
    auto shader = material->GetShader();
    j["shader"] = shader ? shader->GetName() : "";
    j["baseColor"] = {material->baseColor.r, material->baseColor.g, material->baseColor.b, material->baseColor.a};
    j["metallicFactor"] = material->metallicFactor;
    j["roughnessFactor"] = material->roughnessFactor;
    j["transparent"] = material->transparent;

    auto serializeTextureSlot = [&](const char* key, const std::shared_ptr<Texture>& tex) {
        if (!tex || tex->GetFilePath().empty()) return;
        json t;
        t["name"] = tex->GetName();
        t["path"] = tex->GetFilePath();
        j[key] = t;
    };
    serializeTextureSlot("albedo", material->albedo);
    serializeTextureSlot("normal", material->normal);
    serializeTextureSlot("metallicRoughness", material->metallicRoughness);

    std::filesystem::path path(filePath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream file(filePath);
    if (!file) {
        Log::Error("MaterialSerializer: failed to open '" + filePath + "' for writing");
        return false;
    }
    file << j.dump(4);
    return true;
}

std::shared_ptr<Material> MaterialSerializer::Load(const std::string& filePath) {
    if (auto cached = ResourceManager::GetMaterial(filePath)) {
        return cached;
    }

    std::ifstream file(filePath);
    if (!file) {
        Log::Error("MaterialSerializer: cannot open '" + filePath + "'");
        return nullptr;
    }

    json j;
    try {
        file >> j;
    } catch (const std::exception& e) {
        Log::Error("MaterialSerializer: failed to parse '" + filePath + "': " + e.what());
        return nullptr;
    }

    std::string shaderName = j.contains("shader") ? j["shader"].get<std::string>() : "pbr";
    auto shader = ResourceManager::LoadShader(shaderName);
    auto material = std::make_shared<Material>(shader);

    if (j.contains("baseColor") && j["baseColor"].is_array()) {
        auto c = j["baseColor"];
        material->baseColor = {c[0], c[1], c[2], c[3]};
    }
    if (j.contains("metallicFactor")) material->metallicFactor = j["metallicFactor"];
    if (j.contains("roughnessFactor")) material->roughnessFactor = j["roughnessFactor"];
    if (j.contains("transparent")) material->transparent = j["transparent"];

    auto deserializeTextureSlot = [&](const char* key, std::shared_ptr<Texture>& slot) {
        if (!j.contains(key)) return;
        const auto& t = j[key];
        if (!t.contains("name") || !t.contains("path")) return;
        slot = ResourceManager::LoadTexture(t["name"], t["path"]);
    };
    deserializeTextureSlot("albedo", material->albedo);
    deserializeTextureSlot("normal", material->normal);
    deserializeTextureSlot("metallicRoughness", material->metallicRoughness);

    ResourceManager::AddMaterial(filePath, material);
    return material;
}
