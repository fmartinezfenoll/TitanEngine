#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/OpenGLShader.h"
#include "Core/Log.h"

#include <fstream>
#include <sstream>

std::unordered_map<
    std::string,
    std::shared_ptr<OpenGLShader>>
    ResourceManager::m_Shaders;

//=
std::string ResourceManager::ReadFile(const std::string& path)
{
    std::ifstream file(path);

    if (!file)
    {
        Log::Error("Cannot open file: " + path);
        return "";
    }

    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

//=
std::shared_ptr<OpenGLShader>
ResourceManager::LoadShader(const std::string& name)
{
    auto it = m_Shaders.find(name);
    if (it != m_Shaders.end())
        return it->second;

    std::string vertPath = s_ShaderPath + name + ".vert";
    std::string fragPath = s_ShaderPath + name + ".frag";

    std::string vs = ReadFile(vertPath);
    std::string fs = ReadFile(fragPath);

    if (vs.empty() || fs.empty())
    {
        Log::Error("Shader load failed: " + name);
        return nullptr;
    }

    auto shader =
        std::make_shared<OpenGLShader>(name, vs, fs);

    m_Shaders[name] = shader;

    Log::Info("Shader loaded: " + name);

    return shader;
}

//=
std::shared_ptr<OpenGLShader>
ResourceManager::GetShader(const std::string& name)
{
    auto it = m_Shaders.find(name);

    if (it == m_Shaders.end())
    {
        Log::Error("Shader not found: " + name);
        return nullptr;
    }

    return it->second;
}

//=
void ResourceManager::Clear()
{
    m_Shaders.clear();
    Log::Info("All shaders cleared");
}