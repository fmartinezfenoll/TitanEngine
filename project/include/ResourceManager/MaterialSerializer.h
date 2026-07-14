#pragma once

#include <memory>
#include <string>

class Material;

// Persists a standalone Material as a reusable .material asset on disk
// (resources/materials/*.material), independent of any scene. Uses the same
// JSON field layout as the inline "material" component block written by
// SceneSerializer, minus the "type"/"components" wrapper.
class MaterialSerializer
{
public:
    static bool Save(const std::shared_ptr<Material>& material, const std::string& filePath);

    // Returns a cached instance if this filePath was already loaded
    // (ResourceManager::GetMaterial), so multiple objects referencing the
    // same .material asset share one Material instance.
    static std::shared_ptr<Material> Load(const std::string& filePath);
};
