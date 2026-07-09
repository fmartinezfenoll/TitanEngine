#pragma once
#include <glm/glm.hpp>

class GizmoRenderer
{
public:
    static void Init();
    static void Shutdown();

    static void DrawLightGizmo(const glm::vec3& worldPos, const glm::vec3& color,
                               const glm::mat4& view, const glm::mat4& projection);
    static void DrawCameraGizmo(const glm::mat4& cameraModelMatrix,
                                const glm::mat4& view, const glm::mat4& projection);
    static void DrawSelectionBox(const glm::vec3& worldCenter, const glm::vec3& worldExtents,
                                 const glm::mat4& view, const glm::mat4& projection);

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

private:
    static inline unsigned int s_SphereVAO = 0;
    static inline unsigned int s_SphereVBO = 0;
    static inline int s_SphereVertexCount = 0;

    static inline unsigned int s_FrustumVAO = 0;
    static inline unsigned int s_FrustumVBO = 0;
    static inline int s_FrustumVertexCount = 0;

    static inline unsigned int s_CubeVAO = 0;
    static inline unsigned int s_CubeVBO = 0;
    static inline int s_CubeVertexCount = 0;

    static inline unsigned int s_ArrowVAO = 0;
    static inline unsigned int s_ArrowVBO = 0;
    static inline int s_ArrowVertexCount = 0;

    static inline unsigned int s_RingVAO = 0;
    static inline unsigned int s_RingVBO = 0;
    static inline int s_RingVertexCount = 0;

    static inline unsigned int s_ShaftVAO = 0;
    static inline unsigned int s_ShaftVBO = 0;
    static inline int s_ShaftVertexCount = 0;
};
