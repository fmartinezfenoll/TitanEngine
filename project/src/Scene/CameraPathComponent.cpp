#include "Scene/CameraPathComponent.h"
#include "Scene/TNode.h"
#include "Scene/CameraComponent.h"
#include <cmath>
#include <algorithm>

namespace {

glm::vec3 CatmullRom(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) +
                   (-p0 + p2) * t +
                   (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

} // namespace

CameraPathComponent::CameraPathComponent(TNode* owner) : owner(owner) {}

void CameraPathComponent::AddPoint(const glm::vec3& position, const glm::vec3& lookAt, float travelSeconds, float holdSeconds) {
    points.push_back({position, lookAt, travelSeconds, holdSeconds});
}

void CameraPathComponent::RemovePoint(size_t index) {
    if (index >= points.size()) return;
    points.erase(points.begin() + index);

    // Editing the path while it plays would leave currentIndex pointing at a
    // shifted/stale point (and segmentElapsed/holding mid-segment against the
    // old layout). Rather than try to remap, just restart the traversal from a
    // clean state -- editing a live path is an authoring action, not something
    // that needs to preserve exact playback position.
    if (playing) {
        currentIndex = 0;
        segmentElapsed = 0.0f;
        holding = false;
        holdElapsed = 0.0f;
        if (points.empty()) playing = false;
    } else if (currentIndex >= static_cast<int>(points.size())) {
        currentIndex = 0;
    }
}

void CameraPathComponent::Play() {
    if (points.empty() || !owner) return;
    playing = true;
    currentIndex = 0;
    segmentElapsed = 0.0f;
    holding = false;
    holdElapsed = 0.0f;
    owner->transform.position = points[0].position;
}

void CameraPathComponent::Stop() {
    playing = false;
}

glm::vec3 CameraPathComponent::EvaluatePosition(float t) const {
    int count = static_cast<int>(points.size());
    if (count == 1) return points[0].position;

    int i1 = currentIndex == 0 ? count - 1 : currentIndex - 1; // "from" point, wraps for the closing segment
    int i2 = currentIndex;

    // Neighbors for spline tangents -- clamp at the ends when not looping so
    // the curve doesn't reach past the authored path; wrap around when looping
    // so the loop-closing segment curves smoothly too.
    auto neighbor = [&](int idx) -> int {
        if (looping) return ((idx % count) + count) % count;
        return std::clamp(idx, 0, count - 1);
    };

    const glm::vec3& p0 = points[static_cast<size_t>(neighbor(i1 - 1))].position;
    const glm::vec3& p1 = points[static_cast<size_t>(i1)].position;
    const glm::vec3& p2 = points[static_cast<size_t>(i2)].position;
    const glm::vec3& p3 = points[static_cast<size_t>(neighbor(i2 + 1))].position;

    return CatmullRom(p0, p1, p2, p3, t);
}

void CameraPathComponent::Update(float deltaTime) {
    if (!playing || points.empty() || !owner) return;

    if (points.size() == 1) {
        owner->transform.position = points[0].position;
        if (auto* camera = owner->GetComponent<CameraComponent>()) {
            glm::vec3 dir = points[0].lookAt - points[0].position;
            if (glm::length(dir) > 1e-4f) {
                dir = glm::normalize(dir);
                camera->yaw = glm::degrees(std::atan2(dir.z, dir.x));
                camera->pitch = glm::degrees(std::asin(std::clamp(dir.y, -1.0f, 1.0f)));
            }
        }
        return;
    }

    int count = static_cast<int>(points.size());

    if (holding) {
        holdElapsed += deltaTime;
        if (holdElapsed >= points[static_cast<size_t>(currentIndex)].holdSeconds) {
            holding = false;
            holdElapsed = 0.0f;
            int next = currentIndex + 1;
            if (next >= count) {
                if (!looping) { playing = false; return; }
                next = 0;
            }
            currentIndex = next;
            segmentElapsed = 0.0f;
        }
        return;
    }

    float travelSeconds = std::max(points[static_cast<size_t>(currentIndex)].travelSeconds, 1e-3f);
    segmentElapsed += deltaTime;
    float t = std::clamp(segmentElapsed / travelSeconds, 0.0f, 1.0f);

    owner->transform.position = EvaluatePosition(t);

    int fromIndex = currentIndex == 0 ? count - 1 : currentIndex - 1;
    glm::vec3 lookAt = glm::mix(points[static_cast<size_t>(fromIndex)].lookAt, points[static_cast<size_t>(currentIndex)].lookAt, t);

    if (auto* camera = owner->GetComponent<CameraComponent>()) {
        glm::vec3 dir = lookAt - owner->transform.position;
        if (glm::length(dir) > 1e-4f) {
            dir = glm::normalize(dir);
            camera->yaw = glm::degrees(std::atan2(dir.z, dir.x));
            camera->pitch = glm::degrees(std::asin(std::clamp(dir.y, -1.0f, 1.0f)));
        }
    }

    if (t >= 1.0f) {
        holding = true;
        holdElapsed = 0.0f;
        if (points[static_cast<size_t>(currentIndex)].holdSeconds <= 0.0f) {
            holding = false;
            int next = currentIndex + 1;
            if (next >= count) {
                if (!looping) { playing = false; return; }
                next = 0;
            }
            currentIndex = next;
            segmentElapsed = 0.0f;
        }
    }
}
