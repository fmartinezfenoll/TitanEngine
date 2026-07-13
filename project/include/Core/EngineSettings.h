#pragma once

// Toggle for A/B performance comparisons of frustum culling.
class EngineSettings {
public:
    static bool IsFrustumCullingEnabled() { return frustumCullingEnabled; }
    static void SetFrustumCullingEnabled(bool enabled) { frustumCullingEnabled = enabled; }

    static bool AreShadowsEnabled() { return shadowsEnabled; }
    static void SetShadowsEnabled(bool enabled) { shadowsEnabled = enabled; }

    static bool IsVSyncEnabled() { return vsyncEnabled; }
    static void SetVSyncEnabled(bool enabled) { vsyncEnabled = enabled; }

    // Resolution (per side) of Directional/Spot 2D shadow maps.
    static int GetShadowResolution2D() { return shadowResolution2D; }
    static void SetShadowResolution2D(int resolution) { shadowResolution2D = resolution; }

    // Resolution (per face) of Point light cubemap shadow maps.
    static int GetShadowResolutionCube() { return shadowResolutionCube; }
    static void SetShadowResolutionCube(int resolution) { shadowResolutionCube = resolution; }

    // Half-extent (world units) of the Directional light's fixed ortho shadow box,
    // centered at the origin. Smaller = sharper shadows but a smaller covered area.
    static float GetDirectionalShadowBoxSize() { return directionalShadowBoxSize; }
    static void SetDirectionalShadowBoxSize(float size) { directionalShadowBoxSize = size; }

private:
    static inline bool frustumCullingEnabled = true;
    static inline bool shadowsEnabled = true;
    static inline bool vsyncEnabled = true;
    static inline int shadowResolution2D = 2048;
    static inline int shadowResolutionCube = 1024;
    static inline float directionalShadowBoxSize = 150.0f;
};
