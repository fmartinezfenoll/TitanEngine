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

    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    bool IsValid() const { return m_ID != 0; }
    const std::string& GetFilePath() const { return m_FilePath; }

private:
    unsigned int m_ID = 0;
    int m_Width = 0;
    int m_Height = 0;
    int m_Channels = 0;
    std::string m_FilePath;

    void UploadFromMemory(const unsigned char* pixels);
};
