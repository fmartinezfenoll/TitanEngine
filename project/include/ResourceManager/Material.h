#pragma once

#include <memory>
#include <glm/glm.hpp>

class OpenGLShader;
class Texture;

//=
// Encapsulates a shader, PBR textures, and base color for render-state grouping.
//=

class Material
{
public:
    Material(const std::shared_ptr<OpenGLShader>& shader);

    void Bind() const;

    std::shared_ptr<OpenGLShader> GetShader() const { return shader; }

    std::shared_ptr<Texture> albedo;
    std::shared_ptr<Texture> normal;
    std::shared_ptr<Texture> metallicRoughness;
    glm::vec4 baseColor{1.0f};

    // Scalar factors, glTF convention: multiplied with metallicRoughnessMap's
    // B (metallic) / G (roughness) channels when present, used alone otherwise.
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;

    // Added unconditionally to the final lit color (before fog), independent of
    // any light in the scene -- lets a surface glow (signs, lava, eyes) even
    // with lightCount == 0. intensity multiplies color; 0 = no glow (default).
    glm::vec3 emissiveColor{1.0f, 1.0f, 1.0f};
    float emissiveIntensity = 0.0f;

    // When true, drawn in the transparent pass (back-to-front sorted, GL_BLEND
    // enabled, depth writes disabled). Simple on/off blend flag, not a full
    // glTF alphaMode enum -- MASK/alpha-cutoff is out of scope.
    bool transparent = false;

private:
    std::shared_ptr<OpenGLShader> shader;
};
