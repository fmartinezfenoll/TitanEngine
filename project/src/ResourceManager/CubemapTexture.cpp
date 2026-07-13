#include "ResourceManager/CubemapTexture.h"
#include "Core/Log.h"

#include <glad/glad.h>
#include <stb_image.h>

CubemapTexture::CubemapTexture(const std::string& name, const std::array<std::string, 6>& facePaths)
    : Resource(name)
{
    glGenTextures(1, &ID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, ID);

    // Cubemap face UVs expect row 0 = top, matching stb_image's default order.
    stbi_set_flip_vertically_on_load(false);

    for (int i = 0; i < 6; ++i)
    {
        int width, height, channels;
        unsigned char* pixels = stbi_load(facePaths[i].c_str(), &width, &height, &channels, 4);
        if (!pixels)
        {
            Log::Error("Failed to load skybox face: " + facePaths[i]);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            glDeleteTextures(1, &ID);
            ID = 0;
            return;
        }

        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8, width, height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        stbi_image_free(pixels);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

CubemapTexture::~CubemapTexture()
{
    if (ID != 0)
        glDeleteTextures(1, &ID);
}

void CubemapTexture::Bind(unsigned int slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, ID);
}
