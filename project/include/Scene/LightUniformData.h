#pragma once
#include <glm/glm.hpp>
#include <vector>

struct LightUniformData {
    int type;
    glm::vec3 position;
    glm::vec3 direction;
    glm::vec3 color;
    float intensity;
    float range;
    float innerCutoff;
    float outerCutoff;
    int shadowIndex = -1;
};

// Shadow maps ready to be bound/sampled during the main render pass,
// already resolved to texture units by the renderer.
struct ShadowRenderData {
    bool hasDirectional = false;
    unsigned int directionalSlot = 0;
    glm::mat4 directionalLightSpaceMatrix{1.0f};

    int spotCount = 0;
    unsigned int spotSlots[2] = {0, 0};
    glm::mat4 spotLightSpaceMatrices[2];

    int pointCount = 0;
    unsigned int pointSlots[2] = {0, 0};
    glm::vec3 pointLightPos[2];
    float pointFarPlane[2] = {0.0f, 0.0f};
};
