#pragma once

#include "Scene/Component.h"
#include <glm/glm.hpp>
#include <vector>

class TNode;

// Attached to the same TNode as the MeshComponent it skins. Holds the joint
// TNodes (in glTF skin.joints order) and their inverse bind matrices, and
// computes the per-frame skinning matrices MeshComponent::Draw uploads to
// the pbr_skinned shader's "jointMatrices" uniform array.
class SkinComponent : public Component {
public:
    // joints and inverseBindMatrices must be the same length and in the same
    // order as glTF's skin.joints -- that order IS the joint index used by
    // MeshVertex::jointIndices.
    SkinComponent(std::vector<TNode*> joints, std::vector<glm::mat4> inverseBindMatrices);

    // For each joint: meshWorldInverse * jointNode->getModelMatrix() * inverseBindMatrix,
    // i.e. the skinning matrix expressed in the mesh's own local space so it
    // composes correctly with the mesh node's own model matrix in the shader.
    std::vector<glm::mat4> ComputeJointMatrices(const glm::mat4& meshWorldInverse) const;

    size_t GetJointCount() const { return joints.size(); }
    const std::vector<TNode*>& GetJoints() const { return joints; }
    const std::vector<glm::mat4>& GetInverseBindMatrices() const { return inverseBindMatrices; }

private:
    std::vector<TNode*> joints;
    std::vector<glm::mat4> inverseBindMatrices;
};
