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

    static bool IsIBLEnabled() { return iblEnabled; }
    static void SetIBLEnabled(bool enabled) { iblEnabled = enabled; }

    // Not persisted to engine.ini -- purely a live editor visualization toggle.
    static bool IsWireframeEnabled() { return wireframeEnabled; }
    static void SetWireframeEnabled(bool enabled) { wireframeEnabled = enabled; }

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

    // Gizmo snap increments applied while holding Ctrl during a drag.
    static float GetPositionSnap() { return positionSnap; }
    static void SetPositionSnap(float snap) { positionSnap = snap; }

    static float GetRotationSnapDegrees() { return rotationSnapDegrees; }
    static void SetRotationSnapDegrees(float snap) { rotationSnapDegrees = snap; }

    static float GetScaleSnap() { return scaleSnap; }
    static void SetScaleSnap(float snap) { scaleSnap = snap; }

private:
    static inline bool frustumCullingEnabled = true;
    static inline bool shadowsEnabled = true;
    static inline bool vsyncEnabled = true;
    static inline bool iblEnabled = true;
    static inline bool wireframeEnabled = false;
    static inline int shadowResolution2D = 2048;
    static inline int shadowResolutionCube = 1024;
    static inline float directionalShadowBoxSize = 150.0f;
    static inline float positionSnap = 0.5f;
    static inline float rotationSnapDegrees = 15.0f;
    static inline float scaleSnap = 0.1f;
};
