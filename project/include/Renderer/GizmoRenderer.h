#pragma once
#include <glm/glm.hpp>

class GizmoRenderer
{
public:
    static void Init();
    static void Shutdown();

    static void DrawLightGizmo(const glm::vec3& worldPos, const glm::vec3& color,
                               const glm::mat4& view, const glm::mat4& projection);
    // Wireframe visualization of a Point light's range (sphere) or a Spot
    // light's range+outer cone angle -- drawn only for the selected light so
    // it doesn't clutter the viewport with every light's full range.
    static void DrawPointRangeGizmo(const glm::vec3& worldPos, float range,
                                    const glm::mat4& view, const glm::mat4& projection);
    static void DrawSpotRangeGizmo(const glm::vec3& worldPos, const glm::vec3& direction,
                                   float range, float outerConeDegrees,
                                   const glm::mat4& view, const glm::mat4& projection);
    static void DrawCameraGizmo(const glm::mat4& cameraModelMatrix,
                                const glm::mat4& view, const glm::mat4& projection);
    static void DrawSelectionBox(const glm::vec3& worldCenter, const glm::vec3& worldExtents,
                                 const glm::mat4& view, const glm::mat4& projection);
    static void DrawGrid(const glm::mat4& view, const glm::mat4& projection);

    static void DrawMoveGizmo(const glm::vec3& worldPos, const glm::mat4& baseRotation, float scale,
                              const glm::mat4& view, const glm::mat4& projection);
    static void DrawRotateGizmo(const glm::vec3& worldPos, const glm::mat4& baseRotation, float scale,
                                const glm::mat4& view, const glm::mat4& projection);
    static void DrawScaleGizmo(const glm::vec3& worldPos, const glm::mat4& baseRotation, float scale,
                               const glm::mat4& view, const glm::mat4& projection);

    static float ComputeGizmoScale(const glm::vec3& worldPos, const glm::vec3& cameraWorldPos);

    static constexpr float kLightGizmoRadius = 0.3f;
    static constexpr float kCameraGizmoRadius = 0.5f;

    static constexpr float kGizmoArmLength = 1.0f;
    static constexpr float kGizmoRingRadius = 1.0f;
    static constexpr float kGizmoPickTolerance = 0.12f;
    static constexpr float kGizmoScreenScaleFactor = 0.15f;
    static constexpr float kGizmoCenterCubeSize = 0.18f;

    static constexpr int kGridHalfSize = 20;
    static constexpr float kGridSpacing = 1.0f;

private:
    static inline unsigned int SphereVAO = 0;
    static inline unsigned int SphereVBO = 0;
    static inline int SphereVertexCount = 0;

    static inline unsigned int FrustumVAO = 0;
    static inline unsigned int FrustumVBO = 0;
    static inline int FrustumVertexCount = 0;

    static inline unsigned int CubeVAO = 0;
    static inline unsigned int CubeVBO = 0;
    static inline int CubeVertexCount = 0;

    static inline unsigned int ArrowVAO = 0;
    static inline unsigned int ArrowVBO = 0;
    static inline int ArrowVertexCount = 0;

    static inline unsigned int RingVAO = 0;
    static inline unsigned int RingVBO = 0;
    static inline int RingVertexCount = 0;

    static inline unsigned int ShaftVAO = 0;
    static inline unsigned int ShaftVBO = 0;
    static inline int ShaftVertexCount = 0;

    static inline unsigned int GridVAO = 0;
    static inline unsigned int GridVBO = 0;
    static inline int GridVertexCount = 0;

    static inline unsigned int UnitSphereVAO = 0;
    static inline unsigned int UnitSphereVBO = 0;
    static inline int UnitSphereVertexCount = 0;

    static inline unsigned int UnitConeVAO = 0;
    static inline unsigned int UnitConeVBO = 0;
    static inline int UnitConeVertexCount = 0;
};
