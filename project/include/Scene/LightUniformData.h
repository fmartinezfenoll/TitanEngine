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

// Image-based lighting textures ready to be bound/sampled during the main
// render pass, already resolved to texture units by the renderer.
struct IBLRenderData {
    bool hasIBL = false;
    unsigned int irradianceSlot = 8;
    unsigned int prefilterSlot = 9;
    unsigned int brdfLUTSlot = 10;
};

// Distance-based fog, owned per-Scene (see Scene::GetFog) and passed down to
// the PBR fragment shaders each draw. The mode int must match the shader's
// switch (0 = Linear, 1 = Exp, 2 = Exp2).
enum class FogMode { Linear = 0, Exp = 1, Exp2 = 2 };

struct FogSettings {
    bool enabled = false;
    FogMode mode = FogMode::Exp2;
    glm::vec3 color{0.6f, 0.65f, 0.7f};
    float density = 0.03f;  // used by Exp / Exp2
    float start = 10.0f;    // used by Linear: fully clear before this
    float end = 60.0f;      // used by Linear: fully fogged past this
};
