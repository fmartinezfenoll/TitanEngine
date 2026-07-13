#pragma once

class ShadowMap
{
public:
    ShadowMap(int resolution, bool isCubemap);
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    void Bind(unsigned int slot) const;

    unsigned int GetID() const { return ID; }
    bool IsCubemap() const { return isCubemap; }

private:
    unsigned int ID = 0;
    int Resolution = 0;
    bool isCubemap = false;

    void Create2D();
    void CreateCubemap();
};
