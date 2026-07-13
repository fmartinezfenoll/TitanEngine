#pragma once

#include "Renderer/ShadowMap.h"
#include <memory>

class ShadowFramebuffer
{
public:
    ShadowFramebuffer(int resolution, bool isCubemap);
    ~ShadowFramebuffer();

    ShadowFramebuffer(const ShadowFramebuffer&) = delete;
    ShadowFramebuffer& operator=(const ShadowFramebuffer&) = delete;

    void BindForWriting() const;
    static void UnbindToScreen();

    const ShadowMap& GetShadowMap() const { return *shadowMap; }
    int GetResolution() const { return Resolution; }
    bool IsCubemap() const { return isCubemap; }

private:
    unsigned int FBO = 0;
    std::unique_ptr<ShadowMap> shadowMap;
    int Resolution = 0;
    bool isCubemap = false;
};
