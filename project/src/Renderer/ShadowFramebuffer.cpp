#include "Renderer/ShadowFramebuffer.h"
#include "Core/Log.h"

#include <glad/glad.h>

ShadowFramebuffer::ShadowFramebuffer(int resolution, bool isCubemap)
    : Resolution(resolution), isCubemap(isCubemap)
{
    shadowMap = std::make_unique<ShadowMap>(resolution, isCubemap);

    glGenFramebuffers(1, &FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);

    if (isCubemap)
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowMap->GetID(), 0);
    else
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                                shadowMap->GetID(), 0);

    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        Log::Error("ShadowFramebuffer incomplete");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

ShadowFramebuffer::~ShadowFramebuffer()
{
    if (FBO != 0)
        glDeleteFramebuffers(1, &FBO);
}

void ShadowFramebuffer::BindForWriting() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glViewport(0, 0, Resolution, Resolution);
}

void ShadowFramebuffer::UnbindToScreen()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
