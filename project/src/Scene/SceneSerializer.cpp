#include "Scene/SceneSerializer.h"
#include "Scene/Scene.h"
#include "Scene/TNode.h"
#include "Scene/MeshComponent.h"
#include "Scene/MaterialComponent.h"
#include "Scene/CameraComponent.h"
#include "Scene/LightComponent.h"
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/Material.h"
#include "ResourceManager/OpenGLShader.h"
#include "ResourceManager/Texture.h"
#include "Core/Log.h"
#include <json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace {

json SerializeTransform(const Transform& transform) {
    json j;
    j["position"] = {transform.position.x, transform.position.y, transform.position.z};
    j["rotation"] = {transform.rotation.x, transform.rotation.y, transform.rotation.z};
    j["scale"] = {transform.scale.x, transform.scale.y, transform.scale.z};
    return j;
}

Transform DeserializeTransform(const json& j) {
    Transform t;
    if (j.contains("position") && j["position"].is_array()) {
        auto pos = j["position"];
        t.position = {pos[0], pos[1], pos[2]};
    }
    if (j.contains("rotation") && j["rotation"].is_array()) {
        auto rot = j["rotation"];
        t.rotation = {rot[0], rot[1], rot[2]};
    }
    if (j.contains("scale") && j["scale"].is_array()) {
        auto scl = j["scale"];
        t.scale = {scl[0], scl[1], scl[2]};
    }
    return t;
}

json SerializeBoundingVolume(const BoundingVolume* boundingBox) {
    if (!boundingBox) {
        return json::object();
    }

    // Try to cast to known types
    if (const Sphere* sphere = dynamic_cast<const Sphere*>(boundingBox)) {
        json j;
        j["type"] = "sphere";
        j["center"] = {sphere->center.x, sphere->center.y, sphere->center.z};
        j["radius"] = sphere->radius;
        return j;
    }

    if (const AABB* aabb = dynamic_cast<const AABB*>(boundingBox)) {
        json j;
        j["type"] = "aabb";
        j["center"] = {aabb->center.x, aabb->center.y, aabb->center.z};
        j["extents"] = {aabb->extents.x, aabb->extents.y, aabb->extents.z};
        return j;
    }

    return json::object();
}

BoundingVolume* DeserializeBoundingVolume(const json& j) {
    if (j.is_null() || j.empty()) {
        return nullptr;
    }

    if (!j.contains("type")) {
        return nullptr;
    }

    std::string type = j["type"];

    if (type == "sphere" && j.contains("center") && j.contains("radius")) {
        auto center_arr = j["center"];
        glm::vec3 center = {center_arr[0], center_arr[1], center_arr[2]};
        float radius = j["radius"];
        return new Sphere(center, radius);
    }

    if (type == "aabb" && j.contains("center") && j.contains("extents")) {
        auto center_arr = j["center"];
        auto extents_arr = j["extents"];
        glm::vec3 center = {center_arr[0], center_arr[1], center_arr[2]};
        glm::vec3 extents = {extents_arr[0], extents_arr[1], extents_arr[2]};
        glm::vec3 min = center - extents;
        glm::vec3 max = center + extents;
        return new AABB(min, max);
    }

    return nullptr;
}

json SerializeComponents(const TNode* node) {
    json arr = json::array();

    if (auto* mesh = node->GetComponent<MeshComponent>()) {
        json j;
        j["type"] = "mesh";

        json vertices = json::array();
        for (const MeshVertex& v : mesh->GetVertices()) {
            vertices.push_back(v.position.x);
            vertices.push_back(v.position.y);
            vertices.push_back(v.position.z);
            vertices.push_back(v.normal.x);
            vertices.push_back(v.normal.y);
            vertices.push_back(v.normal.z);
            vertices.push_back(v.uv.x);
            vertices.push_back(v.uv.y);
        }
        j["vertices"] = vertices;
        j["indices"] = mesh->GetIndices();

        arr.push_back(j);
    }

    if (auto* materialComp = node->GetComponent<MaterialComponent>()) {
        if (const auto& mat = materialComp->material) {
            json j;
            j["type"] = "material";

            auto shader = mat->GetShader();
            j["shader"] = shader ? shader->GetName() : "";
            j["baseColor"] = {mat->baseColor.r, mat->baseColor.g, mat->baseColor.b, mat->baseColor.a};

            auto serializeTextureSlot = [&](const char* key, const std::shared_ptr<Texture>& tex) {
                if (!tex) return;
                if (tex->GetFilePath().empty()) {
                    Log::Info("SceneSerializer: skipping embedded texture on material for node '" + node->name +
                              "' (no file path to persist)");
                    return;
                }
                json t;
                t["name"] = tex->GetName();
                t["path"] = tex->GetFilePath();
                j[key] = t;
            };
            serializeTextureSlot("albedo", mat->albedo);
            serializeTextureSlot("normal", mat->normal);
            serializeTextureSlot("metallicRoughness", mat->metallicRoughness);

            arr.push_back(j);
        }
    }

    if (auto* camera = node->GetComponent<CameraComponent>()) {
        json j;
        j["type"] = "camera";
        j["fov"] = camera->fov;
        j["nearPlane"] = camera->nearPlane;
        j["farPlane"] = camera->farPlane;
        j["moveSpeed"] = camera->moveSpeed;
        j["mouseSensitivity"] = camera->mouseSensitivity;
        j["yaw"] = camera->yaw;
        j["pitch"] = camera->pitch;

        arr.push_back(j);
    }

    if (auto* light = node->GetComponent<LightComponent>()) {
        json j;
        j["type"] = "light";
        j["lightType"] = static_cast<int>(light->type);
        j["color"] = {light->color.r, light->color.g, light->color.b};
        j["intensity"] = light->intensity;
        j["range"] = light->range;
        j["innerConeDegrees"] = light->innerConeDegrees;
        j["outerConeDegrees"] = light->outerConeDegrees;

        arr.push_back(j);
    }

    return arr;
}

void DeserializeComponents(TNode* node, Scene* scene, const json& j) {
    if (!j.is_array()) return;

    for (const auto& compJson : j) {
        if (!compJson.contains("type")) continue;
        std::string type = compJson["type"];

        if (type == "mesh" && compJson.contains("vertices") && compJson.contains("indices")) {
            std::vector<float> flat = compJson["vertices"].get<std::vector<float>>();
            std::vector<MeshVertex> vertices;
            vertices.reserve(flat.size() / 8);
            for (size_t i = 0; i + 7 < flat.size(); i += 8) {
                MeshVertex v;
                v.position = {flat[i], flat[i + 1], flat[i + 2]};
                v.normal = {flat[i + 3], flat[i + 4], flat[i + 5]};
                v.uv = {flat[i + 6], flat[i + 7]};
                vertices.push_back(v);
            }
            std::vector<uint32_t> indices = compJson["indices"].get<std::vector<uint32_t>>();

            node->AddComponent<MeshComponent>(vertices, indices);
        }
        else if (type == "material" && compJson.contains("shader")) {
            std::string shaderName = compJson["shader"];
            auto shader = ResourceManager::LoadShader(shaderName);
            auto material = std::make_shared<Material>(shader);

            if (compJson.contains("baseColor") && compJson["baseColor"].is_array()) {
                auto c = compJson["baseColor"];
                material->baseColor = {c[0], c[1], c[2], c[3]};
            }

            auto deserializeTextureSlot = [&](const char* key, std::shared_ptr<Texture>& slot) {
                if (!compJson.contains(key)) return;
                const auto& t = compJson[key];
                if (!t.contains("name") || !t.contains("path")) return;
                slot = ResourceManager::LoadTexture(t["name"], t["path"]);
            };
            deserializeTextureSlot("albedo", material->albedo);
            deserializeTextureSlot("normal", material->normal);
            deserializeTextureSlot("metallicRoughness", material->metallicRoughness);

            node->AddComponent<MaterialComponent>(material);
        }
        else if (type == "camera") {
            auto* camera = node->AddComponent<CameraComponent>(node);
            if (compJson.contains("fov")) camera->fov = compJson["fov"];
            if (compJson.contains("nearPlane")) camera->nearPlane = compJson["nearPlane"];
            if (compJson.contains("farPlane")) camera->farPlane = compJson["farPlane"];
            if (compJson.contains("moveSpeed")) camera->moveSpeed = compJson["moveSpeed"];
            if (compJson.contains("mouseSensitivity")) camera->mouseSensitivity = compJson["mouseSensitivity"];
            if (compJson.contains("yaw")) camera->yaw = compJson["yaw"];
            if (compJson.contains("pitch")) camera->pitch = compJson["pitch"];

            if (scene) {
                scene->RegisterCamera(node);
            }
        }
        else if (type == "light") {
            LightType lightType = LightType::Point;
            if (compJson.contains("lightType")) {
                lightType = static_cast<LightType>(compJson["lightType"].get<int>());
            }

            auto* light = node->AddComponent<LightComponent>(node, lightType);
            if (compJson.contains("color") && compJson["color"].is_array()) {
                auto c = compJson["color"];
                light->color = {c[0], c[1], c[2]};
            }
            if (compJson.contains("intensity")) light->intensity = compJson["intensity"];
            if (compJson.contains("range")) light->range = compJson["range"];
            if (compJson.contains("innerConeDegrees")) light->innerConeDegrees = compJson["innerConeDegrees"];
            if (compJson.contains("outerConeDegrees")) light->outerConeDegrees = compJson["outerConeDegrees"];

            if (scene) {
                scene->RegisterLight(node);
            }
        }
    }
}

json SerializeNode(const TNode* node) {
    json j;

    // Serialize name
    if (!node->name.empty()) {
        j["name"] = node->name;
    }

    // Serialize transform
    j["transform"] = SerializeTransform(node->transform);

    // Serialize bounding volume
    if (node->boundingBox) {
        j["boundingBox"] = SerializeBoundingVolume(node->boundingBox);
    }

    // Serialize components
    json components = SerializeComponents(node);
    if (!components.empty()) {
        j["components"] = components;
    }

    // Serialize children
    if (!node->children.empty()) {
        json children_array = json::array();
        for (const TNode* child : node->children) {
            children_array.push_back(SerializeNode(child));
        }
        j["children"] = children_array;
    }

    return j;
}

TNode* DeserializeNode(const json& j, Scene* scene) {
    BoundingVolume* boundingBox = nullptr;
    if (j.contains("boundingBox") && !j["boundingBox"].empty()) {
        boundingBox = DeserializeBoundingVolume(j["boundingBox"]);
    }

    std::string nodeName;
    if (j.contains("name")) {
        nodeName = j["name"];
    }

    TNode* node = new TNode(boundingBox, nodeName);

    // Deserialize transform
    if (j.contains("transform")) {
        node->transform = DeserializeTransform(j["transform"]);
    }

    // Deserialize components
    if (j.contains("components")) {
        DeserializeComponents(node, scene, j["components"]);
    }

    // Deserialize children
    if (j.contains("children") && j["children"].is_array()) {
        for (const auto& child_j : j["children"]) {
            TNode* child = DeserializeNode(child_j, scene);
            if (child) {
                node->addChild(child);
            }
        }
    }

    return node;
}

} // namespace

bool SceneSerializer::SaveScene(Scene* scene, const std::string& filePath) {
    if (!scene) {
        Log::Error("Cannot save null scene");
        return false;
    }

    try {
        json j;
        j["version"] = 1;

        TNode* root = scene->GetRoot();
        if (root) {
            j["root"] = SerializeNode(root);
        }

        std::ofstream file(filePath);
        if (!file.is_open()) {
            Log::Error("Failed to open file for writing: " + filePath);
            return false;
        }

        file << j.dump(2);
        file.close();

        Log::Info("Scene saved to: " + filePath);
        return true;
    } catch (const std::exception& e) {
        Log::Error(std::string("Error saving scene: ") + e.what());
        return false;
    }
}

Scene* SceneSerializer::LoadScene(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            Log::Error("Failed to open file for reading: " + filePath);
            return nullptr;
        }

        json j;
        file >> j;
        file.close();

        Scene* scene = new Scene();
        scene->Init();

        if (j.contains("root")) {
            TNode* root = DeserializeNode(j["root"], scene);
            if (root) {
                scene->GetRoot()->addChild(root);
            }
        }

        Log::Info("Scene loaded from: " + filePath);
        return scene;
    } catch (const std::exception& e) {
        Log::Error(std::string("Error loading scene: ") + e.what());
        return nullptr;
    }
}

TNode* SceneSerializer::DuplicateNode(const TNode* node, Scene* scene) {
    if (!node) return nullptr;

    try {
        json j = SerializeNode(node);
        return DeserializeNode(j, scene);
    } catch (const std::exception& e) {
        Log::Error(std::string("Error duplicating node: ") + e.what());
        return nullptr;
    }
}
