#include "ResourceManager/Material.h"
#include "ResourceManager/OpenGLShader.h"
#include "ResourceManager/Texture.h"
#include <iostream>

Material::Material(const std::shared_ptr<OpenGLShader>& shader)
    : m_shader(shader)
{
    if (!m_shader)
    {
        std::cout << "[ERROR] Material created with null shader\n";
    }
}

void Material::Bind() const
{
    if (!m_shader)
    {
        std::cout << "[ERROR] Trying to bind material with null shader\n";
        return;
    }

    m_shader->Bind();
    m_shader->SetVec4("baseColor", baseColor);

    m_shader->SetBool("hasAlbedoMap", albedo != nullptr);
    if (albedo)
    {
        albedo->Bind(0);
        m_shader->SetInt("albedoMap", 0);
    }

    m_shader->SetBool("hasNormalMap", normal != nullptr);
    if (normal)
    {
        normal->Bind(1);
        m_shader->SetInt("normalMap", 1);
    }

    m_shader->SetBool("hasMetallicRoughnessMap", metallicRoughness != nullptr);
    if (metallicRoughness)
    {
        metallicRoughness->Bind(2);
        m_shader->SetInt("metallicRoughnessMap", 2);
    }
}