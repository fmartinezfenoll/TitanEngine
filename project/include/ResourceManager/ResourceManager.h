#pragma once

#include <unordered_map>
#include <memory>
#include <string>

class OpenGLShader;

class ResourceManager
{
public:

    //=
    // Shader API
    //=
    static std::shared_ptr<OpenGLShader>
    LoadShader(const std::string& name);

    static std::shared_ptr<OpenGLShader>
    GetShader(const std::string& name);

    static const std::unordered_map<std::string, std::shared_ptr<OpenGLShader>>&
    GetAllShaders();

    static void Clear();

private:

    static std::unordered_map<
        std::string,
        std::shared_ptr<OpenGLShader>> m_Shaders;

    static std::string ReadFile(const std::string& path);

    static inline const std::string s_ShaderPath =
        "resources/shaders/";
};