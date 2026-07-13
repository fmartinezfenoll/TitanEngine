#pragma once
#include "Scene/Component.h"
#include <glm/glm.hpp>
#include <array>

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

    bool castsShadow = false;

    glm::vec3 GetPosition() const;
    glm::vec3 GetDirection() const;

    // Directional and Spot. For Directional, the ortho box is centered at
    // focusPoint (snapped to texel-sized increments to avoid shadow shimmer
    // as focusPoint moves continuously, e.g. following the camera).
    glm::mat4 GetLightSpaceMatrix(const glm::vec3& focusPoint = glm::vec3(0.0f)) const;

    // Point: 6 view-projection matrices, order +X,-X,+Y,-Y,+Z,-Z
    std::array<glm::mat4, 6> GetCubemapViewProjections() const;

private:
    TNode* owner;
};
