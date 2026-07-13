#pragma once

#include "ResourceManager/Resource.h"
#include <array>
#include <string>

class CubemapTexture : public Resource
{
public:
    // facePaths order: +X (right), -X (left), +Y (top), -Y (bottom), +Z (front), -Z (back)
    CubemapTexture(const std::string& name, const std::array<std::string, 6>& facePaths);
    ~CubemapTexture();

    CubemapTexture(const CubemapTexture&) = delete;
    CubemapTexture& operator=(const CubemapTexture&) = delete;

    void Bind(unsigned int slot = 0) const;

    bool IsValid() const { return ID != 0; }

private:
    unsigned int ID = 0;
};
