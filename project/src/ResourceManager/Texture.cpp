#include "ResourceManager/Texture.h"
#include "Core/Log.h"

#include <glad/glad.h>
#include <stb_image.h>

Texture::Texture(const std::string& name, const std::string& filePath)
    : Resource(name), FilePath(filePath)
{
    // glTF UVs have V=0 at the top, matching stb_image's default row order
    // (row 0 = top of the source image) against OpenGL's texture storage
    // (row 0 = V=0), so textures are loaded unflipped.
    stbi_set_flip_vertically_on_load(false);
    unsigned char* pixels = stbi_load(filePath.c_str(), &Width, &Height, &Channels, 4);
    if (!pixels)
    {
        Log::Error("Failed to load texture: " + filePath);
        return;
    }

    UploadFromMemory(pixels);
    stbi_image_free(pixels);
}

Texture::Texture(const std::string& name, const unsigned char* data, int size)
    : Resource(name)
{
    stbi_set_flip_vertically_on_load(false);
    unsigned char* pixels = stbi_load_from_memory(data, size, &Width, &Height, &Channels, 4);
    if (!pixels)
    {
        Log::Error("Failed to decode embedded texture: " + name);
        return;
    }

    UploadFromMemory(pixels);
    stbi_image_free(pixels);
}

void Texture::UploadFromMemory(const unsigned char* pixels)
{
    glGenTextures(1, &ID);
    glBindTexture(GL_TEXTURE_2D, ID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
}

Texture::~Texture()
{
    if (ID != 0)
        glDeleteTextures(1, &ID);
}

void Texture::Bind(unsigned int slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, ID);
}
