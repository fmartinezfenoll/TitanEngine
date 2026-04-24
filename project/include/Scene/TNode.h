#pragma once
#ifndef SCENE_TNODE_H
#define SCENE_TNODE_H

#include <vector>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include "Scene/TEntity.h"

// Forward declarations
struct BoundingVolume;
struct Frustum;

// ─────────────────────────────────────────────────────────────────────────────
// 📌 STRUCT PLANE
struct Plane {
    glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
    float distance = 0.0f;

    Plane() = default;
    Plane(const glm::vec3& point, const glm::vec3& norm) {
        normal = glm::normalize(norm);
        distance = glm::dot(normal, point);
    }

    float getSignedDistanceToPlane(const glm::vec3& point) const {
        return glm::dot(normal, point) - distance;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// 📌 STRUCT FRUSTUM
struct Frustum {
    Plane topFace, bottomFace, rightFace, leftFace, farFace, nearFace;
    Frustum() = default;

    void updateFromCamera(const class TNode* cameraNode, float aspect, float zNear, float zFar);
};

// ─────────────────────────────────────────────────────────────────────────────
// 📌 STRUCT BOUNDING VOLUME
struct BoundingVolume {
    virtual ~BoundingVolume() = default;
    virtual bool isOnFrustum(const Frustum& camFrustum, const glm::mat4& modelMatrix) const = 0;
    virtual bool isOnOrForwardPlane(const Plane& plane) const = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// 📌 STRUCT SPHERE
struct Sphere : public BoundingVolume {
    glm::vec3 center{0.f, 0.f, 0.f};
    float radius{0.f};

    Sphere(const glm::vec3& inCenter, float inRadius) : center{inCenter}, radius{inRadius} {}

    bool isOnOrForwardPlane(const Plane& plane) const override {
        return plane.getSignedDistanceToPlane(center) > -radius;
    }

    bool isOnFrustum(const Frustum& camFrustum, const glm::mat4& modelMatrix) const override {
        glm::vec3 globalCenter = glm::vec3(modelMatrix * glm::vec4(center, 1.0f));
        float scaledRadius = radius;
        return (camFrustum.leftFace.getSignedDistanceToPlane(globalCenter) > -scaledRadius &&
                camFrustum.rightFace.getSignedDistanceToPlane(globalCenter) > -scaledRadius &&
                camFrustum.topFace.getSignedDistanceToPlane(globalCenter) > -scaledRadius &&
                camFrustum.bottomFace.getSignedDistanceToPlane(globalCenter) > -scaledRadius &&
                camFrustum.nearFace.getSignedDistanceToPlane(globalCenter) > -scaledRadius &&
                camFrustum.farFace.getSignedDistanceToPlane(globalCenter) > -scaledRadius);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// 📌 STRUCT AABB
struct AABB : public BoundingVolume {
    glm::vec3 center{0.f, 0.f, 0.f};
    glm::vec3 extents{0.f, 0.f, 0.f};

    AABB(const glm::vec3& min, const glm::vec3& max)
        : center{(max + min) * 0.5f}, extents{max.x - center.x, max.y - center.y, max.z - center.z} {}

    bool isOnOrForwardPlane(const Plane& plane) const override {
        float r = extents.x * std::abs(plane.normal.x) +
                  extents.y * std::abs(plane.normal.y) +
                  extents.z * std::abs(plane.normal.z);
        return -r <= plane.getSignedDistanceToPlane(center);
    }

    bool isOnFrustum(const Frustum& camFrustum, const glm::mat4& modelMatrix) const override {
        glm::vec3 globalCenter = glm::vec3(modelMatrix * glm::vec4(center, 1.f));

        glm::vec3 scale;
        scale.x = glm::length(glm::vec3(modelMatrix[0]));
        scale.y = glm::length(glm::vec3(modelMatrix[1]));
        scale.z = glm::length(glm::vec3(modelMatrix[2]));

        glm::vec3 scaledExtents = extents * scale;

        for (const Plane& plane : {camFrustum.leftFace, camFrustum.rightFace, camFrustum.topFace,
                                   camFrustum.bottomFace, camFrustum.nearFace, camFrustum.farFace}) {
            float r = scaledExtents.x * std::abs(plane.normal.x) +
                      scaledExtents.y * std::abs(plane.normal.y) +
                      scaledExtents.z * std::abs(plane.normal.z);

            float distance = plane.getSignedDistanceToPlane(globalCenter);
            if (distance < -r) return false;
        }

        return true;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// 📌 STRUCT TRANSFORM
struct Transform {
    glm::vec3 position{0.f, 0.f, 0.f};
    glm::vec3 rotation{0.f, 0.f, 0.f};
    glm::vec3 scale{1.f, 1.f, 1.f};

    glm::mat4 getModelMatrix() const {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, scale);
        return model;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// 📌 CLASS TNODE
class TNode {
public:
    TEntity* entity;
    std::vector<TNode*> children;
    TNode* parent = nullptr;
    Transform transform;
    BoundingVolume* boundingBox = nullptr;
    std::string name;

    TNode(TEntity* entity = nullptr, BoundingVolume* boundingBox = nullptr, const std::string& nodeName = "")
        : entity(entity), boundingBox(boundingBox), name(nodeName) {}

    ~TNode() {
        for (auto child : children) delete child;
        if (entity) delete entity;
        if (boundingBox) delete boundingBox;
    }

    TNode(const TNode&) = delete;
    TNode& operator=(const TNode&) = delete;

    void addChild(TNode* node) {
        if (node && node != this) {
            node->removeFromParent();
            children.push_back(node);
            node->parent = this;
        }
    }

    void removeChild(TNode* node) {
        auto it = std::find(children.begin(), children.end(), node);
        if (it != children.end()) {
            children.erase(it);
            node->parent = nullptr;
        }
    }

    void removeFromParent() {
        if (parent) {
            parent->removeChild(this);
        }
    }

    void draw(const Frustum& frustum, const glm::mat4& parentMatrix = glm::mat4(1.0f)) {
        glm::mat4 modelMatrix = parentMatrix * transform.getModelMatrix();

        if (!boundingBox || boundingBox->isOnFrustum(frustum, modelMatrix)) {
            if (entity) {
                entity->draw(modelMatrix);
            }

            for (TNode* child : children) {
                if (child) {
                    child->draw(frustum, modelMatrix);
                }
            }
        }
    }

    glm::mat4 getModelMatrix() const {
        if (parent) {
            return parent->getModelMatrix() * transform.getModelMatrix();
        }
        return transform.getModelMatrix();
    }

    glm::vec3 getGlobalPosition() const {
        return glm::vec3(getModelMatrix()[3]);
    }
};

#endif // SCENE_TNODE_H
