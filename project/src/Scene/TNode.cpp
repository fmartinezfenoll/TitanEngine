#include "Scene/TNode.h"
#include "Scene/MeshComponent.h"
#include "Scene/MaterialComponent.h"

void TNode::draw(const Frustum& frustum, const glm::mat4& parentMatrix) {
    glm::mat4 modelMatrix = parentMatrix * transform.getModelMatrix();

    if (!boundingBox || boundingBox->isOnFrustum(frustum, modelMatrix)) {
        if (auto* mesh = GetComponent<MeshComponent>()) {
            mesh->Draw(modelMatrix, GetComponent<MaterialComponent>());
        }

        for (TNode* child : children) {
            if (child) {
                child->draw(frustum, modelMatrix);
            }
        }
    }
}
