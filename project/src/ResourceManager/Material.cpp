#include "ResourceManager/Material.h"
#include "ResourceManager/OpenGLShader.h"
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
}