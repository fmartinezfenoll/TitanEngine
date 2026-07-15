#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

struct AnimationKeyframeVec3 {
    float time = 0.0f;
    glm::vec3 value{0.0f};
};

struct AnimationKeyframeQuat {
    float time = 0.0f;
    glm::quat value{1.0f, 0.0f, 0.0f, 0.0f};
};

enum class AnimationInterpolation { Linear, Step, CubicSpline };
enum class AnimationTargetPath { Translation, Rotation, Scale };

// Targets a node by its pre-order position within the animated subtree (root = 0,
// then children depth-first in TNode::children order) rather than a raw TNode*,
// so the same clip data can be resolved against different instances of the same
// hierarchy (see AnimationComponent::SetNodeIndexMap).
struct AnimationChannelData {
    int targetNodeIndex = -1;
    AnimationTargetPath path = AnimationTargetPath::Translation;
    AnimationInterpolation interpolation = AnimationInterpolation::Linear;
    std::vector<AnimationKeyframeVec3> vec3Keys; // used when path != Rotation
    std::vector<AnimationKeyframeQuat> quatKeys; // used when path == Rotation
};

// Author-time marker fired once playback crosses `time` in either direction
// (including on loop wrap-around) -- see AnimationComponent::Update. Not
// populated by GLTFLoader (glTF has no native concept of an animation event);
// added manually via AnimationComponent::AddEvent (Inspector or code).
struct AnimationEvent {
    float time = 0.0f;
    std::string name;
};

struct AnimationClip {
    std::string name;
    float duration = 0.0f;
    std::vector<AnimationChannelData> channels;
    std::vector<AnimationEvent> events;
};
