#pragma once
#include <string>

class Scene;
class TNode;

class SceneSerializer {
public:
    static bool SaveScene(Scene* scene, const std::string& filePath);
    static Scene* LoadScene(const std::string& filePath);
    static TNode* DuplicateNode(const TNode* node, Scene* scene);

    // Serializes a node (and its subtree) to a JSON string -- e.g. for a Copy/Paste clipboard.
    static std::string SerializeNodeToString(const TNode* node);

    // Reconstructs a node (and its subtree) from a string previously produced by
    // SerializeNodeToString(). Returns nullptr on parse/deserialize failure.
    static TNode* DeserializeNodeFromString(const std::string& json, Scene* scene);
};
