#include "Scene/MaterialComponent.h"
#include "ResourceManager/Material.h"

void MaterialComponent::Bind() const {
    if (material) {
        material->Bind();
    }
}
