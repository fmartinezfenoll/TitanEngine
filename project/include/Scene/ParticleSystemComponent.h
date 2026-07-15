#pragma once

#include "Scene/Component.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

class Texture;

// A CPU-simulated particle system rendered as camera-facing billboards in one
// instanced draw call. Emits from the owning node's world position, integrates
// each particle (gravity + velocity), fades color/size over its lifetime, and
// recycles dead particles. Needs Update() every frame, so the owning node must
// be registered as an animator (see Scene::RegisterAnimator).
class ParticleSystemComponent : public Component {
public:
    enum class Preset { Custom, Fire, Smoke, Sparks };
    enum class BlendMode { Alpha, Additive };

    ParticleSystemComponent();
    ~ParticleSystemComponent();

    ParticleSystemComponent(const ParticleSystemComponent&) = delete;
    ParticleSystemComponent& operator=(const ParticleSystemComponent&) = delete;

    // Advances the simulation. emitterWorldPos is the owning node's world
    // position (particles spawn there when worldSpace is true).
    void Update(float deltaTime, const glm::vec3& emitterWorldPos);

    // Uploads the live particles and issues the instanced draw. Caller sets up
    // blend state per GetBlendMode(). Assumes BillboardGeometry::Init() ran.
    void Draw(const glm::mat4& view, const glm::mat4& projection) const;

    // Overwrites the tunable parameters with a canned effect (Fire/Smoke/
    // Sparks). Preset::Custom leaves everything as-is.
    void ApplyPreset(Preset preset);

    int GetLiveCount() const { return static_cast<int>(particles.size()); }

    // --- tunable parameters ---
    std::shared_ptr<Texture> texture;
    std::string texturePath;

    int maxParticles = 500;
    float emitRate = 60.0f;      // particles per second
    float lifetime = 2.0f;       // seconds
    float lifetimeSpread = 0.4f; // +/- randomization of lifetime

    glm::vec3 startVelocity{0.0f, 2.0f, 0.0f};
    glm::vec3 velocitySpread{0.5f, 0.5f, 0.5f};
    glm::vec3 gravity{0.0f, -1.0f, 0.0f};
    float emitRadius = 0.1f;     // spawn jitter around the emitter

    glm::vec4 startColor{1.0f, 0.6f, 0.1f, 1.0f};
    glm::vec4 endColor{1.0f, 0.0f, 0.0f, 0.0f};
    float startSize = 0.5f;
    float endSize = 0.1f;

    bool worldSpace = true; // spawned particles keep their world position
    bool playing = true;
    Preset preset = Preset::Fire;
    BlendMode blendMode = BlendMode::Additive;

private:
    struct Particle {
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f};
        float age = 0.0f;
        float maxAge = 1.0f;
    };

    void EnsureGLObjects();

    std::vector<Particle> particles;
    float emitAccumulator = 0.0f;

    // GL: shares BillboardGeometry's quad layout in its own VAO plus a dynamic
    // per-instance VBO (posSize vec4 + color vec4) re-uploaded each frame.
    unsigned int quadVBO = 0;
    unsigned int instanceVBO = 0;
    unsigned int vao = 0;
    mutable int instanceCapacity = 0; // current instanceVBO size in instances
};
