#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/OpenGLShader.h"
#include "ResourceManager/Texture.h"
#include "ResourceManager/Material.h"
#include "Core/Log.h"

#include <fstream>
#include <sstream>

std::unordered_map<
    std::string,
    std::shared_ptr<OpenGLShader>>
    ResourceManager::m_Shaders;

std::unordered_map<
    std::string,
    std::shared_ptr<Texture>>
    ResourceManager::m_Textures;

std::unordered_map<
    std::string,
    std::shared_ptr<Material>>
    ResourceManager::m_Materials;

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
const std::unordered_map<std::string, std::shared_ptr<OpenGLShader>>&
ResourceManager::GetAllShaders()
{
    return m_Shaders;
}

//=
// Texture API
//=
std::shared_ptr<Texture>
ResourceManager::LoadTexture(const std::string& name, const std::string& filePath)
{
    auto it = m_Textures.find(name);
    if (it != m_Textures.end())
        return it->second;

    auto texture = std::make_shared<Texture>(name, filePath);
    if (!texture->IsValid())
        return nullptr;

    m_Textures[name] = texture;
    return texture;
}

std::shared_ptr<Texture>
ResourceManager::LoadTextureFromMemory(const std::string& name, const unsigned char* data, int size)
{
    auto it = m_Textures.find(name);
    if (it != m_Textures.end())
        return it->second;

    auto texture = std::make_shared<Texture>(name, data, size);
    if (!texture->IsValid())
        return nullptr;

    m_Textures[name] = texture;
    return texture;
}

std::shared_ptr<Texture>
ResourceManager::GetTexture(const std::string& name)
{
    auto it = m_Textures.find(name);
    return it == m_Textures.end() ? nullptr : it->second;
}

//=
// Material API
//=
std::shared_ptr<Material>
ResourceManager::GetMaterial(const std::string& name)
{
    auto it = m_Materials.find(name);
    return it == m_Materials.end() ? nullptr : it->second;
}

void ResourceManager::AddMaterial(const std::string& name, const std::shared_ptr<Material>& material)
{
    m_Materials[name] = material;
}

//=
void ResourceManager::Clear()
{
    m_Shaders.clear();
    m_Textures.clear();
    m_Materials.clear();
    Log::Info("All resources cleared");
}