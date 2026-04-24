#pragma once
#include <string>
#include <json.hpp>

class Scene;
class TNode;
struct Transform;
struct BoundingVolume;
class TEntity;

using json = nlohmann::json;

class SceneSerializer {
public:
    static bool SaveScene(Scene* scene, const std::string& filePath);
    static Scene* LoadScene(const std::string& filePath);

private:
    static json SerializeTransform(const Transform& transform);
    static Transform DeserializeTransform(const json& j);

    static json SerializeBoundingVolume(const BoundingVolume* boundingBox);
    static BoundingVolume* DeserializeBoundingVolume(const json& j);

    static json SerializeEntity(const TEntity* entity);
    static TEntity* DeserializeEntity(const json& j);

    static json SerializeNode(const TNode* node);
    static TNode* DeserializeNode(const json& j);
};
