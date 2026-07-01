#include "Scene/CameraEntity.h"
#include "Scene/TNode.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

CameraEntity::CameraEntity(TNode* owner)
    : m_owner(owner)
{
}

glm::vec3 CameraEntity::GetForward() const
{
    glm::vec3 forward;
    forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward.y = sin(glm::radians(pitch));
    forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    return glm::normalize(forward);
}

glm::vec3 CameraEntity::GetRight() const
{
    return glm::normalize(glm::cross(GetForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::mat4 CameraEntity::GetViewMatrix() const
{
    glm::vec3 position = m_owner ? m_owner->transform.position : glm::vec3(0.0f);
    return glm::lookAt(position, position + GetForward(), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 CameraEntity::GetProjectionMatrix(float aspectRatio) const
{
    return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
}

void CameraEntity::ProcessKeyboard(const glm::vec3& moveDir, float deltaTime)
{
    if (!m_owner) return;

    glm::vec3 forward = GetForward();
    glm::vec3 right = GetRight();
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    glm::vec3 delta = (forward * moveDir.z) + (right * moveDir.x) + (up * moveDir.y);
    if (glm::length(delta) > 0.0f)
        delta = glm::normalize(delta);

    m_owner->transform.position += delta * moveSpeed * deltaTime;
}

void CameraEntity::ProcessMouseLook(float xOffset, float yOffset)
{
    yaw += xOffset * mouseSensitivity;
    pitch += yOffset * mouseSensitivity;
    pitch = std::clamp(pitch, -89.0f, 89.0f);
}
