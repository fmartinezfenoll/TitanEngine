#pragma once

#include <memory>

class OpenGLShader;

//=
// Encapsulates a shader and its GPU state.
// Future: textures, uniforms, render states
//=

class Material
{
public:
    Material(const std::shared_ptr<OpenGLShader>& shader);

    void Bind() const;

    std::shared_ptr<OpenGLShader> GetShader() const { return m_shader; }

private:
    std::shared_ptr<OpenGLShader> m_shader;
};