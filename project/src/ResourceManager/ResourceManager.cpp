#include "ResourceManager/ResourceManager.h"
#include "ResourceManager/OpenGLShader.h"
#include "ResourceManager/Texture.h"
#include "ResourceManager/CubemapTexture.h"
#include "ResourceManager/Material.h"
#include "Core/Log.h"

#include <fstream>
#include <sstream>

std::unordered_map<
    std::string,
    std::shared_ptr<OpenGLShader>>
    ResourceManager::Shaders;

std::unordered_map<
    std::string,
    std::shared_ptr<Texture>>
    ResourceManager::Textures;

std::unordered_map<
    std::string,
    std::shared_ptr<CubemapTexture>>
    ResourceManager::Cubemaps;

std::unordered_map<
    std::string,
    std::shared_ptr<Material>>
    ResourceManager::Materials;

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
    auto it = Shaders.find(name);
    if (it != Shaders.end())
        return it->second;

    std::string vertPath = ShaderPath + name + ".vert";
    std::string fragPath = ShaderPath + name + ".frag";

    std::string vs = ReadFile(vertPath);
    std::string fs = ReadFile(fragPath);

    if (vs.empty() || fs.empty())
    {
        Log::Error("Shader load failed: " + name);
        return nullptr;
    }

    auto shader =
        std::make_shared<OpenGLShader>(name, vs, fs);

    Shaders[name] = shader;

    Log::Info("Shader loaded: " + name);

    return shader;
}

//=
std::shared_ptr<OpenGLShader>
ResourceManager::LoadShader(const std::string& name, bool hasGeometryShader)
{
    if (!hasGeometryShader) {
        return LoadShader(name);
    }

    auto it = Shaders.find(name);
    if (it != Shaders.end())
        return it->second;

    std::string vertPath = ShaderPath + name + ".vert";
    std::string fragPath = ShaderPath + name + ".frag";
    std::string geomPath = ShaderPath + name + ".geom";

    std::string vs = ReadFile(vertPath);
    std::string fs = ReadFile(fragPath);
    std::string gs = ReadFile(geomPath);

    if (vs.empty() || fs.empty() || gs.empty())
    {
        Log::Error("Shader load failed: " + name);
        return nullptr;
    }

    auto shader =
        std::make_shared<OpenGLShader>(name, vs, fs, gs);

    Shaders[name] = shader;

    Log::Info("Shader loaded: " + name);

    return shader;
}

//=
std::shared_ptr<OpenGLShader>
ResourceManager::GetShader(const std::string& name)
{
    auto it = Shaders.find(name);

    if (it == Shaders.end())
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
    return Shaders;
}

//=
// Texture API
//=
std::shared_ptr<Texture>
ResourceManager::LoadTexture(const std::string& name, const std::string& filePath)
{
    auto it = Textures.find(name);
    if (it != Textures.end())
        return it->second;

    auto texture = std::make_shared<Texture>(name, filePath);
    if (!texture->IsValid())
        return nullptr;

    Textures[name] = texture;
    return texture;
}

std::shared_ptr<Texture>
ResourceManager::LoadTextureFromMemory(const std::string& name, const unsigned char* data, int size)
{
    auto it = Textures.find(name);
    if (it != Textures.end())
        return it->second;

    auto texture = std::make_shared<Texture>(name, data, size);
    if (!texture->IsValid())
        return nullptr;

    Textures[name] = texture;
    return texture;
}

std::shared_ptr<Texture>
ResourceManager::GetTexture(const std::string& name)
{
    auto it = Textures.find(name);
    return it == Textures.end() ? nullptr : it->second;
}

//=
// Cubemap API
//=
std::shared_ptr<CubemapTexture>
ResourceManager::LoadCubemap(const std::string& name, const std::array<std::string, 6>& facePaths)
{
    auto it = Cubemaps.find(name);
    if (it != Cubemaps.end())
        return it->second;

    auto cubemap = std::make_shared<CubemapTexture>(name, facePaths);
    if (!cubemap->IsValid())
        return nullptr;

    Cubemaps[name] = cubemap;
    return cubemap;
}

std::shared_ptr<CubemapTexture>
ResourceManager::GetCubemap(const std::string& name)
{
    auto it = Cubemaps.find(name);
    return it == Cubemaps.end() ? nullptr : it->second;
}

bool ResourceManager::FileExists(const std::string& path)
{
    std::ifstream file(path);
    return file.good();
}

std::shared_ptr<CubemapTexture>
ResourceManager::LoadSkyboxFromFolder(const std::string& folderName)
{
    std::string cacheName = "skybox:" + folderName;
    auto it = Cubemaps.find(cacheName);
    if (it != Cubemaps.end())
        return it->second;

    static const char* kFaceNames[6] = { "right", "left", "top", "bottom", "front", "back" };
    std::array<std::string, 6> facePaths;

    for (int i = 0; i < 6; ++i)
    {
        std::string base = SkyboxPath + folderName + "/" + kFaceNames[i];
        std::string jpgPath = base + ".jpg";
        std::string pngPath = base + ".png";

        if (FileExists(jpgPath))
            facePaths[i] = jpgPath;
        else if (FileExists(pngPath))
            facePaths[i] = pngPath;
        else
        {
            Log::Error("Skybox face not found (.jpg/.png): " + base);
            return nullptr;
        }
    }

    auto cubemap = std::make_shared<CubemapTexture>(cacheName, facePaths);
    if (!cubemap->IsValid())
        return nullptr;

    Cubemaps[cacheName] = cubemap;
    return cubemap;
}

//=
// Material API
//=
std::shared_ptr<Material>
ResourceManager::GetMaterial(const std::string& name)
{
    auto it = Materials.find(name);
    return it == Materials.end() ? nullptr : it->second;
}

void ResourceManager::AddMaterial(const std::string& name, const std::shared_ptr<Material>& material)
{
    Materials[name] = material;
}

//=
void ResourceManager::Clear()
{
    Shaders.clear();
    Textures.clear();
    Cubemaps.clear();
    Materials.clear();
    Log::Info("All resources cleared");
}