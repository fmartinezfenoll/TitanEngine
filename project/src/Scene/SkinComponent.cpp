#include "Scene/SkinComponent.h"
#include "Scene/TNode.h"
#include "Core/Log.h"

namespace {
constexpr size_t kMaxJoints = 64;
}

SkinComponent::SkinComponent(std::vector<TNode*> joints, std::vector<glm::mat4> inverseBindMatrices)
    : joints(std::move(joints)), inverseBindMatrices(std::move(inverseBindMatrices))
{
    if (this->joints.size() > kMaxJoints) {
        Log::Error("SkinComponent: skin has " + std::to_string(this->joints.size()) +
                   " joints, truncating to " + std::to_string(kMaxJoints));
        this->joints.resize(kMaxJoints);
        this->inverseBindMatrices.resize(kMaxJoints);
    }
}

std::vector<glm::mat4> SkinComponent::ComputeJointMatrices(const glm::mat4& meshWorldInverse) const {
    std::vector<glm::mat4> result;
    result.reserve(joints.size());

    for (size_t i = 0; i < joints.size(); ++i) {
        if (!joints[i]) {
            result.push_back(glm::mat4(1.0f));
            continue;
        }
        result.push_back(meshWorldInverse * joints[i]->getModelMatrix() * inverseBindMatrices[i]);
    }

    return result;
}
