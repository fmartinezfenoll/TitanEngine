#include "Scene/CameraComponent.h"
#include "Scene/TNode.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

CameraComponent::CameraComponent(TNode* ownerNode)
    : owner(ownerNode)
{
}

glm::vec3 CameraComponent::GetForward() const
{
    glm::vec3 forward;
    forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward.y = sin(glm::radians(pitch));
    forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    return glm::normalize(forward);
}

glm::vec3 CameraComponent::GetRight() const
{
    return glm::normalize(glm::cross(GetForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::mat4 CameraComponent::GetViewMatrix() const
{
    glm::vec3 position = owner ? owner->transform.position : glm::vec3(0.0f);
    return glm::lookAt(position, position + GetForward(), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 CameraComponent::GetProjectionMatrix(float aspectRatio) const
{
    return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
}

void CameraComponent::ProcessKeyboard(const glm::vec3& moveDir, float deltaTime)
{
    if (!owner) return;

    glm::vec3 forward = GetForward();
    glm::vec3 right = GetRight();
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    glm::vec3 delta = (forward * moveDir.z) + (right * moveDir.x) + (up * moveDir.y);
    if (glm::length(delta) > 0.0f)
        delta = glm::normalize(delta);

    owner->transform.position += delta * moveSpeed * deltaTime;
}

void CameraComponent::ProcessMouseLook(float xOffset, float yOffset)
{
    yaw += xOffset * mouseSensitivity;
    pitch += yOffset * mouseSensitivity;
    pitch = std::clamp(pitch, -89.0f, 89.0f);
}
