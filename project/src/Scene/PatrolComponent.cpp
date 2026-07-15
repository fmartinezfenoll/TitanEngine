#include "Scene/PatrolComponent.h"
#include "Scene/TNode.h"
#include <cmath>
#include <algorithm>

namespace {

// Shortest signed angular difference from `from` to `to`, in degrees, in (-180, 180].
float ShortestAngleDelta(float from, float to) {
    float delta = std::fmod(to - from, 360.0f);
    if (delta > 180.0f) delta -= 360.0f;
    if (delta < -180.0f) delta += 360.0f;
    return delta;
}

} // namespace

PatrolComponent::PatrolComponent(TNode* owner) : owner(owner) {}

void PatrolComponent::AddWaypoint(const glm::vec3& position, float pauseSeconds) {
    waypoints.push_back({position, pauseSeconds});
}

void PatrolComponent::RemoveWaypoint(size_t index) {
    if (index >= waypoints.size()) return;
    waypoints.erase(waypoints.begin() + index);
    if (currentIndex >= static_cast<int>(waypoints.size())) {
        currentIndex = 0;
    }
}

void PatrolComponent::Update(float deltaTime) {
    if (!active || waypoints.empty() || !owner) return;

    if (pauseTimeRemaining > 0.0f) {
        pauseTimeRemaining -= deltaTime;
        return;
    }

    const glm::vec3& targetPos = waypoints[currentIndex].position;
    glm::vec3 toTarget = targetPos - owner->transform.position;
    toTarget.y = 0.0f;

    float distance = glm::length(toTarget);
    float step = speed * deltaTime;

    if (distance <= step || distance < 1e-4f) {
        owner->transform.position = targetPos;
        pauseTimeRemaining = waypoints[currentIndex].pauseSeconds;
        currentIndex = (currentIndex + 1) % static_cast<int>(waypoints.size());
        return;
    }

    glm::vec3 direction = toTarget / distance;
    owner->transform.position += direction * step;

    float targetYaw = glm::degrees(std::atan2(-direction.x, -direction.z)) + forwardOffsetDegrees;
    float delta = ShortestAngleDelta(owner->transform.rotation.y, targetYaw);
    float maxTurn = turnSpeedDegrees * deltaTime;
    float turn = std::clamp(delta, -maxTurn, maxTurn);
    owner->transform.rotation.y += turn;
}
