#pragma once

#include "Scene/Component.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>

class Texture;

// A single camera-facing textured quad (impostor/sprite). Positioned at the
// owning TNode's world position; orientation is computed in the vertex shader
// each frame from the view matrix, so no per-frame CPU work or Update() is
// needed. Drawn in the scene's transparent pass (alpha-blended, back-to-front
// with other transparent draws).
class BillboardComponent : public Component {
public:
    BillboardComponent() = default;

    // Draws the billboard centered at worldCenter (the owning node's world
    // position). Assumes BillboardGeometry::Init() has run and the caller has
    // set up the transparent-pass GL state (blend on, depth-write off).
    void Draw(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& worldCenter) const;

    std::shared_ptr<Texture> texture;
    std::string texturePath; // remembered for serialization; empty for none
    glm::vec2 size{1.0f, 1.0f};
    glm::vec4 tint{1.0f};
    float alphaCutoff = 0.01f;
};
