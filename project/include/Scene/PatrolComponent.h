#pragma once

#include "Scene/Component.h"
#include <glm/glm.hpp>
#include <vector>

class TNode;

struct PatrolWaypoint {
    glm::vec3 position{0.0f};
    float pauseSeconds = 0.0f; // how long to stand still (facing the waypoint) after arriving
};

// Moves and rotates its owner TNode through a looping sequence of waypoints:
// walk toward waypoint[i], face its direction of travel while moving, pause
// pauseSeconds once arrived, rotate to face the next waypoint, repeat.
// Purely a transform driver -- has no opinion about any animation clip
// playing on this node or its children (e.g. a walk-cycle AnimationComponent
// on a child mesh keeps looping independently, unaffected by this component).
class PatrolComponent : public Component {
public:
    explicit PatrolComponent(TNode* owner);

    void AddWaypoint(const glm::vec3& position, float pauseSeconds = 0.0f);
    void RemoveWaypoint(size_t index);
    const std::vector<PatrolWaypoint>& GetWaypoints() const { return waypoints; }
    std::vector<PatrolWaypoint>& GetWaypointsMutable() { return waypoints; }

    void SetSpeed(float unitsPerSecond) { speed = unitsPerSecond; }
    float GetSpeed() const { return speed; }

    void SetTurnSpeed(float degreesPerSecond) { turnSpeedDegrees = degreesPerSecond; }
    float GetTurnSpeed() const { return turnSpeedDegrees; }

    // Added to the computed facing yaw before applying it. Different glTF
    // exports rig their model's "forward" to different axes (+Z, -Z, ...);
    // if the character walks facing backward/sideways relative to its
    // travel direction, adjust this (e.g. 180 to flip front/back).
    void SetForwardOffset(float degrees) { forwardOffsetDegrees = degrees; }
    float GetForwardOffset() const { return forwardOffsetDegrees; }

    void SetActive(bool isActive) { active = isActive; }
    bool IsActive() const { return active; }

    void Update(float deltaTime);

    int GetCurrentWaypointIndex() const { return currentIndex; }
    bool IsPaused() const { return pauseTimeRemaining > 0.0f; }

private:
    TNode* owner;
    std::vector<PatrolWaypoint> waypoints;
    float speed = 2.0f;
    float turnSpeedDegrees = 180.0f;
    float forwardOffsetDegrees = 0.0f;
    bool active = true;

    int currentIndex = 0;
    float pauseTimeRemaining = 0.0f;
};
