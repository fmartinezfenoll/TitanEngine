#pragma once

#include "Scene/Component.h"
#include <glm/glm.hpp>
#include <vector>

class TNode;

struct CameraPathPoint {
    glm::vec3 position{0.0f};
    glm::vec3 lookAt{0.0f};
    float travelSeconds = 2.0f; // time to travel FROM the previous point TO this one (ignored for point 0 unless looping)
    float holdSeconds = 0.0f;   // pause here, still looking at lookAt, before continuing
};

// Plays back a scripted camera move: a sequence of positions/look-targets,
// interpolated with a Catmull-Rom spline for position (smooth curved travel,
// not straight-line segments like PatrolComponent) and a linear blend of the
// look-at target across each segment. Drives its owner TNode's position and,
// if the owner also has a CameraComponent, that component's yaw/pitch --
// exactly what CameraComponent::GetViewMatrix() reads.
//
// Deliberately separate from PatrolComponent: patrol drives a walking
// character at constant speed with discrete turns, this drives a cinematic
// camera move authored as timed keyframes with smooth curvature.
class CameraPathComponent : public Component {
public:
    explicit CameraPathComponent(TNode* owner);

    void AddPoint(const glm::vec3& position, const glm::vec3& lookAt, float travelSeconds = 2.0f, float holdSeconds = 0.0f);
    void RemovePoint(size_t index);
    const std::vector<CameraPathPoint>& GetPoints() const { return points; }
    std::vector<CameraPathPoint>& GetPointsMutable() { return points; }

    void Play();
    void Stop();
    bool IsPlaying() const { return playing; }

    void SetLooping(bool loop) { looping = loop; }
    bool IsLooping() const { return looping; }

    void Update(float deltaTime);

    int GetCurrentSegment() const { return currentIndex; }

private:
    glm::vec3 EvaluatePosition(float t) const; // t in [0,1] across segment [currentIndex-1 -> currentIndex]

    TNode* owner;
    std::vector<CameraPathPoint> points;
    bool looping = true;
    bool playing = false;

    int currentIndex = 0;       // index of the point we're travelling TOWARD
    float segmentElapsed = 0.0f;
    bool holding = false;
    float holdElapsed = 0.0f;
};
