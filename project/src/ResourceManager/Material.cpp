#include "ResourceManager/Material.h"
#include "ResourceManager/OpenGLShader.h"
#include "ResourceManager/Texture.h"
#include <glad/glad.h>
#include <iostream>

Material::Material(const std::shared_ptr<OpenGLShader>& shaderIn)
    : shader(shaderIn)
{
    if (!shader)
    {
        std::cout << "[ERROR] Material created with null shader\n";
    }
}

void Material::Bind() const
{
    if (!shader)
    {
        std::cout << "[ERROR] Trying to bind material with null shader\n";
        return;
    }

    shader->Bind();
    shader->SetVec4("baseColor", baseColor);

    // Each slot always (re)binds a GL_TEXTURE_2D, even to 0, when there's no
    // texture -- otherwise whatever was last bound to that texture unit (e.g.
    // the skybox's GL_TEXTURE_CUBE_MAP on unit 0) stays bound, and sampling
    // albedoMap (a sampler2D) against a cubemap-typed binding is a type
    // mismatch that triggers GL_INVALID_OPERATION on the draw call, silently
    // aborting it -- nothing renders even though the shader itself is fine.
    shader->SetBool("hasAlbedoMap", albedo != nullptr);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, albedo ? albedo->GetID() : 0);
    shader->SetInt("albedoMap", 0);

    shader->SetBool("hasNormalMap", normal != nullptr);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, normal ? normal->GetID() : 0);
    shader->SetInt("normalMap", 1);

    shader->SetBool("hasMetallicRoughnessMap", metallicRoughness != nullptr);
    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, metallicRoughness ? metallicRoughness->GetID() : 0);
    shader->SetInt("metallicRoughnessMap", 2);

    shader->SetFloat("metallicFactor", metallicFactor);
    shader->SetFloat("roughnessFactor", roughnessFactor);

    shader->SetVec3("emissiveColor", emissiveColor);
    shader->SetFloat("emissiveIntensity", emissiveIntensity);
}