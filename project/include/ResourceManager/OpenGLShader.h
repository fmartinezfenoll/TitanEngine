#pragma once

#include "ResourceManager/Resource.h"

#include <string>
#include <glm/glm.hpp>
#include <glad/glad.h>

class OpenGLShader : public Resource
{
public:
    OpenGLShader(const std::string& name,
                 const std::string& vertexSource,
                 const std::string& fragmentSource,
                 const std::string& geometrySource = "");

    ~OpenGLShader();

    void Bind() const;
    void Unbind() const;

    //=
    // Uniform setters
    //=
    void SetBool(const std::string& name, bool value) const;
    void SetInt(const std::string& name, int value) const;
    void SetFloat(const std::string& name, float value) const;
    void SetVec2(const std::string& name, const glm::vec2& value) const;
    void SetVec3(const std::string& name, const glm::vec3& value) const;
    void SetVec4(const std::string& name, const glm::vec4& value) const;
    void SetMat4(const std::string& name, const glm::mat4& value) const;

private:
    unsigned int m_ID;

    unsigned int Compile(const std::string& source, GLenum type);
    void CheckErrors(unsigned int object, const std::string& type);
};