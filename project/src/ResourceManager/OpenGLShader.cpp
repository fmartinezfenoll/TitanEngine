#include "ResourceManager/OpenGLShader.h"
#include "Core/Log.h"

#include <glm/glm.hpp>

OpenGLShader::OpenGLShader(const std::string& name,
                           const std::string& vertexSource,
                           const std::string& fragmentSource,
                           const std::string& geometrySource)
    : Resource(name)
{
    unsigned int vertex   = Compile(vertexSource, GL_VERTEX_SHADER);
    unsigned int fragment = Compile(fragmentSource, GL_FRAGMENT_SHADER);
    unsigned int geometry = 0;

    if (!geometrySource.empty())
        geometry = Compile(geometrySource, GL_GEOMETRY_SHADER);

    m_ID = glCreateProgram();

    glAttachShader(m_ID, vertex);
    glAttachShader(m_ID, fragment);
    if (geometry) glAttachShader(m_ID, geometry);

    glLinkProgram(m_ID);
    CheckErrors(m_ID, "PROGRAM");

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    if (geometry) glDeleteShader(geometry);
}

//=
OpenGLShader::~OpenGLShader()
{
    glDeleteProgram(m_ID);
}

//=
void OpenGLShader::Bind() const
{
    glUseProgram(m_ID);
}

void OpenGLShader::Unbind() const
{
    glUseProgram(0);
}

//=
unsigned int OpenGLShader::Compile(const std::string& source,
                                   GLenum type)
{
    if (source.empty())
    {
        Log::Error("Shader source empty");
        return 0;
    }

    unsigned int shader = glCreateShader(type);
    const char* src = source.c_str();

    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    CheckErrors(shader,
        type == GL_VERTEX_SHADER   ? "VERTEX" :
        type == GL_FRAGMENT_SHADER ? "FRAGMENT" :
                                     "GEOMETRY");

    return shader;
}

//=
void OpenGLShader::CheckErrors(unsigned int object,
                               const std::string& type)
{
    int success;
    char infoLog[1024];

    if (type != "PROGRAM")
    {
        glGetShaderiv(object, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(object, 1024, nullptr, infoLog);

            Log::Error("Shader " + type +
                       " compilation failed:\n" +
                       std::string(infoLog));
        }
    }
    else
    {
        glGetProgramiv(object, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(object, 1024, nullptr, infoLog);

            Log::Error("Program linking failed:\n" +
                       std::string(infoLog));
        }
    }
}