#include "ResourceManager/Material.h"
#include "ResourceManager/OpenGLShader.h"
#include "ResourceManager/Texture.h"
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

    shader->SetBool("hasAlbedoMap", albedo != nullptr);
    if (albedo)
    {
        albedo->Bind(0);
        shader->SetInt("albedoMap", 0);
    }

    shader->SetBool("hasNormalMap", normal != nullptr);
    if (normal)
    {
        normal->Bind(1);
        shader->SetInt("normalMap", 1);
    }

    shader->SetBool("hasMetallicRoughnessMap", metallicRoughness != nullptr);
    if (metallicRoughness)
    {
        metallicRoughness->Bind(2);
        shader->SetInt("metallicRoughnessMap", 2);
    }
}