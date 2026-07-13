#include "Scene/LightComponent.h"
#include "Scene/TNode.h"
#include "Core/EngineSettings.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

LightComponent::LightComponent(TNode* ownerNode, LightType type)
    : type(type), owner(ownerNode)
{
}

glm::vec3 LightComponent::GetPosition() const {
    return owner ? owner->getGlobalPosition() : glm::vec3(0.0f);
}

glm::vec3 LightComponent::GetDirection() const {
    if (!owner) return glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec4 forward = owner->getModelMatrix() * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
    return glm::normalize(glm::vec3(forward));
}

glm::mat4 LightComponent::GetLightSpaceMatrix(const glm::vec3& focusPoint) const {
    if (type == LightType::Directional) {
        float boxSize = EngineSettings::GetDirectionalShadowBoxSize();

        // Snap the focus point to texel-sized increments in light space so the
        // shadow map only shifts by whole texels as focusPoint moves continuously
        // (e.g. following the camera) -- otherwise the shadow edges shimmer/swim.
        int resolution = EngineSettings::GetShadowResolution2D();
        float texelSize = (boxSize * 2.0f) / static_cast<float>(resolution);

        glm::vec3 dir = glm::normalize(GetDirection());
        glm::vec3 up = std::abs(dir.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        glm::mat4 lightViewForSnap = glm::lookAt(glm::vec3(0.0f), dir, up);

        glm::vec3 focusInLightSpace = glm::vec3(lightViewForSnap * glm::vec4(focusPoint, 1.0f));
        focusInLightSpace.x = std::floor(focusInLightSpace.x / texelSize) * texelSize;
        focusInLightSpace.y = std::floor(focusInLightSpace.y / texelSize) * texelSize;
        glm::vec3 snappedFocus = glm::vec3(glm::inverse(lightViewForSnap) * glm::vec4(focusInLightSpace, 1.0f));

        glm::vec3 eye = snappedFocus - dir * (boxSize + 50.0f);
        glm::mat4 lightView = glm::lookAt(eye, snappedFocus, up);
        glm::mat4 lightProj = glm::ortho(-boxSize, boxSize, -boxSize, boxSize, 1.0f, (boxSize + 50.0f) * 2.0f);
        return lightProj * lightView;
    }

    // Spot
    glm::vec3 pos = GetPosition();
    glm::vec3 dir = glm::normalize(GetDirection());
    glm::vec3 up = std::abs(dir.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    glm::mat4 lightView = glm::lookAt(pos, pos + dir, up);
    glm::mat4 lightProj = glm::perspective(glm::radians(outerConeDegrees * 2.0f), 1.0f, 0.1f, range);
    return lightProj * lightView;
}

std::array<glm::mat4, 6> LightComponent::GetCubemapViewProjections() const {
    glm::vec3 pos = GetPosition();
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, range);

    static const glm::vec3 dirs[6] = {
        glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0),
        glm::vec3(0, 1, 0), glm::vec3(0, -1, 0),
        glm::vec3(0, 0, 1), glm::vec3(0, 0, -1)
    };
    static const glm::vec3 ups[6] = {
        glm::vec3(0, -1, 0), glm::vec3(0, -1, 0),
        glm::vec3(0, 0, 1),  glm::vec3(0, 0, -1),
        glm::vec3(0, -1, 0), glm::vec3(0, -1, 0)
    };

    std::array<glm::mat4, 6> result;
    for (int i = 0; i < 6; ++i) {
        glm::mat4 view = glm::lookAt(pos, pos + dirs[i], ups[i]);
        result[i] = proj * view;
    }
    return result;
}
