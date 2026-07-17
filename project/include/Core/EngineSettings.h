#pragma once
#include <string>

// Toggle for A/B performance comparisons of frustum culling.
class EngineSettings {
public:
    static bool IsFrustumCullingEnabled() { return frustumCullingEnabled; }
    static void SetFrustumCullingEnabled(bool enabled) { frustumCullingEnabled = enabled; }

    // Distance culling: nodes whose world origin is farther than the max draw
    // distance from the camera are skipped (per-node, not per-subtree).
    static bool IsDistanceCullEnabled() { return distanceCullEnabled; }
    static void SetDistanceCullEnabled(bool enabled) { distanceCullEnabled = enabled; }

    static float GetMaxDrawDistance() { return maxDrawDistance; }
    static void SetMaxDrawDistance(float distance) { maxDrawDistance = distance; }

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

    // When true, gizmo drags snap by default and holding Ctrl temporarily
    // disables snapping (inverted from the normal "snap only while holding
    // Ctrl" behavior) -- same toggle-then-invert pattern as Blender/Unity.
    static bool IsAlwaysSnapEnabled() { return alwaysSnapEnabled; }
    static void SetAlwaysSnapEnabled(bool enabled) { alwaysSnapEnabled = enabled; }

    // Gizmo snap increments applied while holding Ctrl during a drag.
    static float GetPositionSnap() { return positionSnap; }
    static void SetPositionSnap(float snap) { positionSnap = snap; }

    static float GetRotationSnapDegrees() { return rotationSnapDegrees; }
    static void SetRotationSnapDegrees(float snap) { rotationSnapDegrees = snap; }

    static float GetScaleSnap() { return scaleSnap; }
    static void SetScaleSnap(float snap) { scaleSnap = snap; }

    static bool IsAutoSaveEnabled() { return autoSaveEnabled; }
    static void SetAutoSaveEnabled(bool enabled) { autoSaveEnabled = enabled; }

    // Interval, in seconds, between automatic backup saves of the active scene.
    static float GetAutoSaveIntervalSeconds() { return autoSaveIntervalSeconds; }
    static void SetAutoSaveIntervalSeconds(float seconds) { autoSaveIntervalSeconds = seconds; }

    // Name of the last scene that was active; reloaded automatically on startup.
    static const std::string& GetLastActiveScene() { return lastActiveScene; }
    static void SetLastActiveScene(const std::string& name) { lastActiveScene = name; }

    // Maximum number of undo steps kept in the editor's history. Older steps are
    // discarded once the limit is reached (bounds memory use per snapshot).
    static int GetUndoHistoryLimit() { return undoHistoryLimit; }
    static void SetUndoHistoryLimit(int limit) { undoHistoryLimit = limit < 1 ? 1 : limit; }

    // --- Post-processing ---
    // Master switch: when false, the scene renders straight to the screen with
    // no HDR framebuffer or effect passes (zero overhead). When true, the scene
    // is rendered to an HDR target and the enabled effects below are applied.
    static bool IsPostProcessEnabled() { return postProcessEnabled; }
    static void SetPostProcessEnabled(bool enabled) { postProcessEnabled = enabled; }

    static bool IsBloomEnabled() { return bloomEnabled; }
    static void SetBloomEnabled(bool enabled) { bloomEnabled = enabled; }

    // Luminance above which a fragment contributes to the bloom halo.
    static float GetBloomThreshold() { return bloomThreshold; }
    static void SetBloomThreshold(float value) { bloomThreshold = value < 0.0f ? 0.0f : value; }

    // How strongly the blurred bright pass is added back over the image.
    static float GetBloomIntensity() { return bloomIntensity; }
    static void SetBloomIntensity(float value) { bloomIntensity = value < 0.0f ? 0.0f : value; }

    // Tone mapping (HDR -> LDR). Uses the exposure below (ACES filmic curve).
    static bool IsTonemapEnabled() { return tonemapEnabled; }
    static void SetTonemapEnabled(bool enabled) { tonemapEnabled = enabled; }

    static float GetExposure() { return exposure; }
    static void SetExposure(float value) { exposure = value < 0.0f ? 0.0f : value; }

    static bool IsFXAAEnabled() { return fxaaEnabled; }
    static void SetFXAAEnabled(bool enabled) { fxaaEnabled = enabled; }

    static bool IsSSAOEnabled() { return ssaoEnabled; }
    static void SetSSAOEnabled(bool enabled) { ssaoEnabled = enabled; }

    // World-space sampling radius of the SSAO hemisphere.
    static float GetSSAORadius() { return ssaoRadius; }
    static void SetSSAORadius(float value) { ssaoRadius = value < 0.001f ? 0.001f : value; }

    // How strongly the ambient occlusion darkens the ambient term.
    static float GetSSAOIntensity() { return ssaoIntensity; }
    static void SetSSAOIntensity(float value) { ssaoIntensity = value < 0.0f ? 0.0f : value; }

private:
    static inline bool frustumCullingEnabled = true;
    static inline bool distanceCullEnabled = false;
    static inline float maxDrawDistance = 200.0f;
    static inline bool shadowsEnabled = true;
    static inline bool vsyncEnabled = true;
    static inline bool iblEnabled = true;
    static inline bool wireframeEnabled = false;
    static inline int shadowResolution2D = 2048;
    static inline int shadowResolutionCube = 1024;
    static inline float directionalShadowBoxSize = 150.0f;
    static inline bool alwaysSnapEnabled = false;
    static inline float positionSnap = 0.5f;
    static inline float rotationSnapDegrees = 15.0f;
    static inline float scaleSnap = 0.1f;
    static inline bool autoSaveEnabled = true;
    static inline float autoSaveIntervalSeconds = 300.0f;
    static inline std::string lastActiveScene = "";
    static inline int undoHistoryLimit = 25;

    static inline bool postProcessEnabled = false;
    static inline bool bloomEnabled = true;
    static inline float bloomThreshold = 1.0f;
    static inline float bloomIntensity = 0.6f;
    static inline bool tonemapEnabled = true;
    static inline float exposure = 1.0f;
    static inline bool fxaaEnabled = true;
    static inline bool ssaoEnabled = false;
    static inline float ssaoRadius = 0.5f;
    static inline float ssaoIntensity = 1.0f;
};
