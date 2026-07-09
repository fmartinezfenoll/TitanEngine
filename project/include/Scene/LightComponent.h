#pragma once
#include "Scene/Component.h"
#include <glm/glm.hpp>

class TNode;

enum class LightType { Directional, Point, Spot };

class LightComponent : public Component {
public:
    explicit LightComponent(TNode* owner, LightType type = LightType::Point);

    LightType type;
    glm::vec3 color{1.0f};
    float intensity = 1.0f;

    // Point/Spot attenuation
    float range = 20.0f;

    // Spot only
    float innerConeDegrees = 20.0f;
    float outerConeDegrees = 30.0f;

    glm::vec3 GetPosition() const;
    glm::vec3 GetDirection() const;

private:
    TNode* m_owner;
};
