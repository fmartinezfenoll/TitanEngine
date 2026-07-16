#pragma once

#include "Scene/Component.h"
#include <glm/glm.hpp>
#include <string>

class TNode;

// Generates a heightmap-based terrain mesh onto its owning node. It does NOT
// draw anything itself: on Generate() it builds a grid mesh (positions from the
// heightmap's luminance, computed normals, UVs) and installs it as a
// MeshComponent on the same node, so the existing PBR pipeline (lighting, fog,
// culling, material/texture drag-drop) applies unchanged. A default PBR
// MaterialComponent is added if the node doesn't already have one.
//
// The heightmap path + parameters are all that's serialized; the mesh is
// regenerated from them on load (like GrassComponent's instance buffer).
class TerrainComponent : public Component {
public:
    explicit TerrainComponent(TNode* owner);

    // (Re)builds the mesh from the current heightmap + parameters and installs
    // it on the owning node. No-op with a log if the heightmap can't be loaded.
    void Generate();

    void SetHeightmap(const std::string& path) { heightmapPath = path; }
    const std::string& GetHeightmapPath() const { return heightmapPath; }

    float size = 50.0f;       // world-space extent on X and Z
    int resolution = 128;     // vertices per side (grid is resolution x resolution)
    float heightScale = 8.0f; // max height (luminance 1.0 -> this many world units)
    // With no heightmap assigned, Generate() falls back to procedural rolling
    // hills (a sum of sines) so the component works out of the box without an
    // external asset. Drag a grayscale image onto Heightmap to use a real one.
    float noiseFrequency = 0.15f; // spatial frequency of the procedural hills

private:
    TNode* owner;
    std::string heightmapPath;
};
