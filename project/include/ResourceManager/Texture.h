#pragma once

#include "ResourceManager/Resource.h"
#include <string>

class Texture : public Resource
{
public:
    Texture(const std::string& name, const unsigned char* data, int size);
    Texture(const std::string& name, const std::string& filePath);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    void Bind(unsigned int slot = 0) const;

    int GetWidth() const { return Width; }
    int GetHeight() const { return Height; }
    bool IsValid() const { return ID != 0; }
    const std::string& GetFilePath() const { return FilePath; }
    // Raw GL texture id, for ImGui::Image thumbnails/previews: (ImTextureID)(intptr_t)GetID().
    unsigned int GetID() const { return ID; }

    // Reads the texture back from the GPU and writes it to disk as a PNG,
    // then updates GetFilePath() to point at it. Used to bake embedded glTF
    // textures (no FilePath) into real files so they can be persisted by
    // SceneSerializer. Returns false on failure (FilePath left unchanged).
    bool SaveToPNG(const std::string& path);

private:
    unsigned int ID = 0;
    int Width = 0;
    int Height = 0;
    int Channels = 0;
    std::string FilePath;

    void UploadFromMemory(const unsigned char* pixels);
};
