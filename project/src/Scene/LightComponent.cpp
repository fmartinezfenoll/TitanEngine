#include "Scene/LightComponent.h"
#include "Scene/TNode.h"

LightComponent::LightComponent(TNode* owner, LightType type)
    : type(type), m_owner(owner)
{
}

glm::vec3 LightComponent::GetPosition() const {
    return m_owner ? m_owner->getGlobalPosition() : glm::vec3(0.0f);
}

glm::vec3 LightComponent::GetDirection() const {
    if (!m_owner) return glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec4 forward = m_owner->getModelMatrix() * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
    return glm::normalize(glm::vec3(forward));
}
