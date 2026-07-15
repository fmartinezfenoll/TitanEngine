#pragma once

#include "Scene/Component.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>

class Texture;

// A patch of instanced grass: many cross-quad blades scattered over an area,
// drawn in ONE instanced draw call, with a shader-side wind sway. Rendered in
// the opaque pass using alpha-cutout (discard), so it needs no sorting and
// occludes / is occluded correctly.
//
// Owns its own GL objects (a quad VBO + a per-instance VBO + a VAO). The
// instance buffer is (re)built from the parameters below whenever they change
// via Rebuild(); it's deterministic given `seed`, so save/load reproduces the
// exact same layout without storing every blade.
class GrassComponent : public Component {
public:
    GrassComponent();
    ~GrassComponent();

    GrassComponent(const GrassComponent&) = delete;
    GrassComponent& operator=(const GrassComponent&) = delete;

    // Regenerates the per-instance buffer from the current parameters. Call
    // after changing area/density/seed/bladeSize. Cheap enough to call from the
    // Inspector on edit.
    void Rebuild();

    void Draw(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection, float time) const;

    std::shared_ptr<Texture> texture;
    std::string texturePath;
    glm::vec2 areaSize{10.0f, 10.0f}; // patch extent on X/Z, world units
    int density = 500;                // number of blades (each = 2 cross quads)
    glm::vec2 bladeSize{0.3f, 0.6f};  // width/height of one blade
    glm::vec3 tint{0.35f, 0.65f, 0.25f};
    float alphaCutoff = 0.3f;
    float windStrength = 0.08f;
    float windSpeed = 1.5f;
    unsigned int seed = 1337;

private:
    void EnsureGLObjects();

    unsigned int quadVBO = 0;
    unsigned int instanceVBO = 0;
    unsigned int vao = 0;
    int instanceCount = 0; // = density * 2 (two crossed quads per blade)
    bool dirty = true;
};
