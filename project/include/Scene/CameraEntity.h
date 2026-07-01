#pragma once
#include "Scene/TEntity.h"
#include <glm/glm.hpp>

class TNode;

class CameraEntity : public TEntity {
public:
    explicit CameraEntity(TNode* owner);

    void draw(const glm::mat4& modelMatrix) override {}
    void update(float deltaTime) override {}

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

    void ProcessKeyboard(const glm::vec3& moveDir, float deltaTime);
    void ProcessMouseLook(float xOffset, float yOffset);

    float fov = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 500.0f;
    float moveSpeed = 3.0f;
    float mouseSensitivity = 0.1f;
    float yaw = -90.0f;
    float pitch = 0.0f;

    glm::vec3 GetForward() const;
    glm::vec3 GetRight() const;

private:
    TNode* m_owner;
};
