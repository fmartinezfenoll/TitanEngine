#pragma once

#include <unordered_map>
#include <memory>
#include <string>

class OpenGLShader;
class Texture;
class Material;

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
        std::shared_ptr<OpenGLShader>> m_Shaders;

    static std::unordered_map<
        std::string,
        std::shared_ptr<Texture>> m_Textures;

    static std::unordered_map<
        std::string,
        std::shared_ptr<Material>> m_Materials;

    static std::string ReadFile(const std::string& path);

    static inline const std::string s_ShaderPath =
        "resources/shaders/";
};