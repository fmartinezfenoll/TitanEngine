#pragma once
#include "Scene/Component.h"
#include <memory>

class Material;

class MaterialComponent : public Component {
public:
    explicit MaterialComponent(const std::shared_ptr<Material>& material)
        : material(material) {}

    void Bind() const;

    std::shared_ptr<Material> material;
};
