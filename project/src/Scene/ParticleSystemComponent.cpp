#include "Scene/ParticleSystemComponent.h"
#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/OpenGLShader.h"
#include "ResourceManager/Texture.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <random>
#include <vector>
#include <algorithm>

namespace {
// Same unit quad as BillboardGeometry; the particle VAO needs its own copy to
// attach two per-instance attributes alongside it.
const float kQuadVertices[] = {
    -0.5f, -0.5f,  0.0f, 0.0f,
     0.5f, -0.5f,  1.0f, 0.0f,
     0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f, -0.5f,  0.0f, 0.0f,
     0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f,  0.0f, 1.0f,
};
constexpr int kQuadVertexCount = 6;

// One thread-local RNG for spawn jitter -- deterministic enough for a visual
// effect, no need to seed per-system.
float RandRange(float lo, float hi) {
    static thread_local std::mt19937 rng(0xC0FFEE);
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng);
}

// Per-instance GPU layout: vec4 (xyz world pos, w size) + vec4 rgba color.
struct InstanceData {
    glm::vec4 posSize;
    glm::vec4 color;
};
}

ParticleSystemComponent::ParticleSystemComponent() = default;

ParticleSystemComponent::~ParticleSystemComponent() {
    if (vao != 0) glDeleteVertexArrays(1, &vao);
    if (quadVBO != 0) glDeleteBuffers(1, &quadVBO);
    if (instanceVBO != 0) glDeleteBuffers(1, &instanceVBO);
}

void ParticleSystemComponent::EnsureGLObjects() {
    if (vao != 0) return;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &quadVBO);
    glGenBuffers(1, &instanceVBO);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    // location 2 = posSize (vec4), location 3 = color (vec4), divisor 1.
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, posSize));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, color));
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);
}

void ParticleSystemComponent::Update(float deltaTime, const glm::vec3& emitterWorldPos) {
    // Integrate + age out existing particles.
    for (size_t i = 0; i < particles.size();) {
        Particle& p = particles[i];
        p.age += deltaTime;
        if (p.age >= p.maxAge) {
            particles[i] = particles.back();
            particles.pop_back();
            continue;
        }
        p.velocity += gravity * deltaTime;
        p.position += p.velocity * deltaTime;
        ++i;
    }

    if (!playing) return;

    // Spawn new particles at emitRate, capped at maxParticles.
    emitAccumulator += emitRate * deltaTime;
    int toSpawn = static_cast<int>(emitAccumulator);
    emitAccumulator -= static_cast<float>(toSpawn);

    for (int i = 0; i < toSpawn && static_cast<int>(particles.size()) < maxParticles; ++i) {
        Particle p;
        p.position = emitterWorldPos + glm::vec3(
            RandRange(-emitRadius, emitRadius),
            RandRange(-emitRadius, emitRadius),
            RandRange(-emitRadius, emitRadius));
        p.velocity = startVelocity + glm::vec3(
            RandRange(-velocitySpread.x, velocitySpread.x),
            RandRange(-velocitySpread.y, velocitySpread.y),
            RandRange(-velocitySpread.z, velocitySpread.z));
        p.age = 0.0f;
        p.maxAge = std::max(0.05f, lifetime + RandRange(-lifetimeSpread, lifetimeSpread));
        particles.push_back(p);
    }
}

void ParticleSystemComponent::Draw(const glm::mat4& view, const glm::mat4& projection) const {
    if (particles.empty()) return;

    const_cast<ParticleSystemComponent*>(this)->EnsureGLObjects();

    auto shader = ResourceManager::LoadShader("particle");
    if (!shader) return;

    // Build the per-instance data, fading color/size over each particle's life.
    std::vector<InstanceData> instances;
    instances.reserve(particles.size());
    for (const Particle& p : particles) {
        float t = p.maxAge > 0.0f ? p.age / p.maxAge : 1.0f;
        glm::vec4 color = glm::mix(startColor, endColor, t);
        float size = glm::mix(startSize, endSize, t);
        instances.push_back({ glm::vec4(p.position, size), color });
    }

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    if (static_cast<int>(instances.size()) > instanceCapacity) {
        glBufferData(GL_ARRAY_BUFFER, instances.size() * sizeof(InstanceData), instances.data(), GL_DYNAMIC_DRAW);
        instanceCapacity = static_cast<int>(instances.size());
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, instances.size() * sizeof(InstanceData), instances.data());
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    shader->Bind();
    shader->SetMat4("view", view);
    shader->SetMat4("projection", projection);

    shader->SetBool("hasTexture", texture != nullptr);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture ? texture->GetID() : 0);
    shader->SetInt("particleTexture", 0);

    glBindVertexArray(vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, kQuadVertexCount, static_cast<int>(instances.size()));
    glBindVertexArray(0);
}

void ParticleSystemComponent::ApplyPreset(Preset newPreset) {
    preset = newPreset;
    switch (newPreset) {
        case Preset::Fire:
            emitRate = 80.0f;
            lifetime = 1.2f; lifetimeSpread = 0.3f;
            startVelocity = glm::vec3(0.0f, 2.5f, 0.0f);
            velocitySpread = glm::vec3(0.5f, 0.5f, 0.5f);
            gravity = glm::vec3(0.0f, 1.0f, 0.0f); // fire rises
            emitRadius = 0.15f;
            startColor = glm::vec4(1.0f, 0.7f, 0.2f, 1.0f);
            endColor = glm::vec4(1.0f, 0.1f, 0.0f, 0.0f);
            startSize = 0.6f; endSize = 0.05f;
            blendMode = BlendMode::Additive;
            break;
        case Preset::Smoke:
            emitRate = 30.0f;
            lifetime = 3.0f; lifetimeSpread = 0.8f;
            startVelocity = glm::vec3(0.0f, 1.2f, 0.0f);
            velocitySpread = glm::vec3(0.3f, 0.2f, 0.3f);
            gravity = glm::vec3(0.0f, 0.3f, 0.0f);
            emitRadius = 0.2f;
            startColor = glm::vec4(0.3f, 0.3f, 0.3f, 0.6f);
            endColor = glm::vec4(0.1f, 0.1f, 0.1f, 0.0f);
            startSize = 0.4f; endSize = 1.5f;
            blendMode = BlendMode::Alpha;
            break;
        case Preset::Sparks:
            emitRate = 120.0f;
            lifetime = 0.8f; lifetimeSpread = 0.3f;
            startVelocity = glm::vec3(0.0f, 3.0f, 0.0f);
            velocitySpread = glm::vec3(2.0f, 1.5f, 2.0f);
            gravity = glm::vec3(0.0f, -6.0f, 0.0f); // arc back down
            emitRadius = 0.05f;
            startColor = glm::vec4(1.0f, 0.9f, 0.5f, 1.0f);
            endColor = glm::vec4(1.0f, 0.4f, 0.0f, 0.0f);
            startSize = 0.12f; endSize = 0.02f;
            blendMode = BlendMode::Additive;
            break;
        case Preset::Custom:
            break;
    }
}
