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

private:
    std::shared_ptr<OpenGLShader> shader;
};
