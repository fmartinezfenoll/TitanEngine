#pragma once

#include <string>

class TNode;
class Scene;

// Persists a TNode (and its full subtree) as a reusable .prefab asset on disk
// (resources/prefabs/*.prefab), independent of any scene. Reuses the exact
// same node JSON schema as SceneSerializer's node serialization.
class PrefabSerializer
{
public:
    static bool Save(const TNode* node, const std::string& filePath);

    // Deserializes a fresh, independent TNode subtree from the prefab file.
    // Every call produces its own deep copy (no sharing between instances) --
    // caller is responsible for addChild-ing the result into a scene.
    // Returns nullptr on failure (missing/corrupt file).
    static TNode* Instantiate(const std::string& filePath, Scene* scene);
};
