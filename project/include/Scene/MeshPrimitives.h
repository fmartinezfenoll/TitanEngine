#pragma once

#include "Scene/MeshComponent.h"
#include <vector>
#include <cstdint>

// Procedural primitive mesh generators (position + normal + uv; tangents are
// derived automatically by MeshComponent). Kept out of DebugUI.cpp so the
// geometry math is reusable (Create menu, demo scenes, tests) and that file
// stays UI-focused. All primitives are centered on the origin.
namespace MeshPrimitives {

// UV sphere. `segments` = longitudinal divisions, `rings` = latitudinal.
void Sphere(std::vector<MeshVertex>& outVertices, std::vector<uint32_t>& outIndices,
            float radius = 0.5f, int segments = 24, int rings = 16);

// Flat plane on the XZ plane (normal = +Y), `size` x `size`, one quad
// subdivided `subdivisions` times per side (subdivisions = 1 -> a single quad).
void Plane(std::vector<MeshVertex>& outVertices, std::vector<uint32_t>& outIndices,
           float size = 1.0f, int subdivisions = 1);

// Cylinder along the Y axis, capped at both ends.
void Cylinder(std::vector<MeshVertex>& outVertices, std::vector<uint32_t>& outIndices,
              float radius = 0.5f, float height = 1.0f, int segments = 24);

// Cone along the Y axis (apex up), with a bottom cap.
void Cone(std::vector<MeshVertex>& outVertices, std::vector<uint32_t>& outIndices,
          float radius = 0.5f, float height = 1.0f, int segments = 24);

} // namespace MeshPrimitives
