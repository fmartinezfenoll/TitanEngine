#pragma once

#include <unordered_map>
#include <memory>
#include <string>
#include <array>

class OpenGLShader;
class Texture;
class Material;
class CubemapTexture;

class ResourceManager
{
public:

    //=
    // Shader API
    //=
    static std::shared_ptr<OpenGLShader>
    LoadShader(const std::string& name);

    static std::shared_ptr<OpenGLShader>
    LoadShader(const std::string& name, bool hasGeometryShader);

    static std::shared_ptr<OpenGLShader>
    GetShader(const std::string& name);

    static const std::unordered_map<std::string, std::shared_ptr<OpenGLShader>>&
    GetAllShaders();

    //=
    // Texture API
    //=
    static std::shared_ptr<Texture>
    LoadTexture(const std::string& name, const std::string& filePath);

    static std::shared_ptr<Texture>
    LoadTextureFromMemory(const std::string& name, const unsigned char* data, int size);

    static std::shared_ptr<Texture>
    GetTexture(const std::string& name);

    //=
    // Cubemap API
    //=
    static std::shared_ptr<CubemapTexture>
    LoadCubemap(const std::string& name, const std::array<std::string, 6>& facePaths);

    static std::shared_ptr<CubemapTexture>
    GetCubemap(const std::string& name);

    // Loads resources/textures/skybox/{folderName}/{right,left,top,bottom,front,back}.{jpg,png}
    // Tries .jpg then .png per face. Returns nullptr (with a log) if any face is missing.
    static std::shared_ptr<CubemapTexture>
    LoadSkyboxFromFolder(const std::string& folderName);

    //=
    // Material API
    //=
    static std::shared_ptr<Material>
    GetMaterial(const std::string& name);

    static void
    AddMaterial(const std::string& name, const std::shared_ptr<Material>& material);

    static void Clear();

private:

    static std::unordered_map<
        std::string,
        std::shared_ptr<OpenGLShader>> Shaders;

    static std::unordered_map<
        std::string,
        std::shared_ptr<Texture>> Textures;

    static std::unordered_map<
        std::string,
        std::shared_ptr<CubemapTexture>> Cubemaps;

    static std::unordered_map<
        std::string,
        std::shared_ptr<Material>> Materials;

    static std::string ReadFile(const std::string& path);

    static inline const std::string ShaderPath =
        "resources/shaders/";

    static inline const std::string SkyboxPath =
        "resources/textures/skybox/";

    static bool FileExists(const std::string& path);
};