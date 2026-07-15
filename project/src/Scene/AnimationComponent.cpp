#include "Scene/AnimationComponent.h"
#include "Scene/TNode.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

namespace {

// Locates the two vec3 keyframes surrounding `time` and returns their
// interpolated value per `interp`. CUBICSPLINE keyframes are stored as
// (inTangent, value, outTangent) triples packed consecutively in vec3Keys
// (matching glTF's CUBICSPLINE output layout), so the "real" keyframe count
// is vec3Keys.size() / 3 and each logical keyframe's value is the middle
// element of its triple.
glm::vec3 SampleVec3(const std::vector<AnimationKeyframeVec3>& keys, float time, AnimationInterpolation interp) {
    if (keys.empty()) return glm::vec3(0.0f);

    if (interp == AnimationInterpolation::CubicSpline) {
        size_t count = keys.size() / 3;
        if (count == 0) return glm::vec3(0.0f);
        if (count == 1 || time <= keys[1].time) return keys[1].value;

        size_t i = 0;
        while (i + 1 < count && keys[(i + 1) * 3 + 1].time <= time) ++i;
        if (i + 1 >= count) return keys[i * 3 + 1].value;

        float t0 = keys[i * 3 + 1].time;
        float t1 = keys[(i + 1) * 3 + 1].time;
        float dt = t1 - t0;
        float t = dt > 1e-6f ? (time - t0) / dt : 0.0f;

        glm::vec3 p0 = keys[i * 3 + 1].value;
        glm::vec3 m0 = keys[i * 3 + 2].value * dt;      // outTangent of keyframe i
        glm::vec3 p1 = keys[(i + 1) * 3 + 1].value;
        glm::vec3 m1 = keys[(i + 1) * 3].value * dt;    // inTangent of keyframe i+1

        float t2 = t * t, t3 = t2 * t;
        float h00 = 2 * t3 - 3 * t2 + 1;
        float h10 = t3 - 2 * t2 + t;
        float h01 = -2 * t3 + 3 * t2;
        float h11 = t3 - t2;
        return h00 * p0 + h10 * m0 + h01 * p1 + h11 * m1;
    }

    if (keys.size() == 1 || time <= keys.front().time) return keys.front().value;
    if (time >= keys.back().time) return keys.back().value;

    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        if (time >= keys[i].time && time <= keys[i + 1].time) {
            if (interp == AnimationInterpolation::Step) return keys[i].value;
            float dt = keys[i + 1].time - keys[i].time;
            float t = dt > 1e-6f ? (time - keys[i].time) / dt : 0.0f;
            return glm::mix(keys[i].value, keys[i + 1].value, t);
        }
    }
    return keys.back().value;
}

glm::quat SampleQuat(const std::vector<AnimationKeyframeQuat>& keys, float time, AnimationInterpolation interp) {
    if (keys.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    if (interp == AnimationInterpolation::CubicSpline) {
        size_t count = keys.size() / 3;
        if (count == 0) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        if (count == 1 || time <= keys[1].time) return keys[1].value;

        size_t i = 0;
        while (i + 1 < count && keys[(i + 1) * 3 + 1].time <= time) ++i;
        if (i + 1 >= count) return keys[i * 3 + 1].value;

        float t0 = keys[i * 3 + 1].time;
        float t1 = keys[(i + 1) * 3 + 1].time;
        float dt = t1 - t0;
        float t = dt > 1e-6f ? (time - t0) / dt : 0.0f;
        // Hermite on quaternion components, then renormalize (standard glTF approach).
        glm::quat p0 = keys[i * 3 + 1].value;
        glm::quat p1 = keys[(i + 1) * 3 + 1].value;
        return glm::normalize(glm::slerp(p0, p1, t));
    }

    if (keys.size() == 1 || time <= keys.front().time) return keys.front().value;
    if (time >= keys.back().time) return keys.back().value;

    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        if (time >= keys[i].time && time <= keys[i + 1].time) {
            if (interp == AnimationInterpolation::Step) return keys[i].value;
            float dt = keys[i + 1].time - keys[i].time;
            float t = dt > 1e-6f ? (time - keys[i].time) / dt : 0.0f;
            return glm::slerp(keys[i].value, keys[i + 1].value, t);
        }
    }
    return keys.back().value;
}

// Samples every channel of `clip` at `time` into a per-node pose map, without
// writing anything to the actual TNodes -- lets the caller blend two clips'
// poses before committing a final value to transform.
std::unordered_map<TNode*, SampledPose> SamplePose(const AnimationClip& clip, float time,
                                                    const std::unordered_map<int, TNode*>& nodeIndexMap) {
    std::unordered_map<TNode*, SampledPose> pose;

    for (const auto& channel : clip.channels) {
        auto it = nodeIndexMap.find(channel.targetNodeIndex);
        if (it == nodeIndexMap.end() || !it->second) continue;
        SampledPose& sampled = pose[it->second];

        switch (channel.path) {
            case AnimationTargetPath::Translation:
                sampled.position = SampleVec3(channel.vec3Keys, time, channel.interpolation);
                sampled.hasPosition = true;
                break;
            case AnimationTargetPath::Scale:
                sampled.scale = SampleVec3(channel.vec3Keys, time, channel.interpolation);
                sampled.hasScale = true;
                break;
            case AnimationTargetPath::Rotation:
                sampled.rotation = SampleQuat(channel.quatKeys, time, channel.interpolation);
                sampled.hasRotation = true;
                break;
        }
    }

    return pose;
}

// Writes a fully-sampled pose straight to each node's transform (weight=1
// case -- no blend in progress).
void ApplySampledPose(const std::unordered_map<TNode*, SampledPose>& pose) {
    for (const auto& [node, sampled] : pose) {
        if (sampled.hasPosition) node->transform.position = sampled.position;
        if (sampled.hasScale) node->transform.scale = sampled.scale;
        if (sampled.hasRotation) {
            node->transform.rotationQuat = sampled.rotation;
            node->transform.useQuatRotation = true;
        }
    }
}

// Blends `from` (the frozen snapshot of the outgoing clip) into `to` (the
// incoming clip, sampled fresh this frame) by `t` in [0,1], writing the
// result to each node in `to`. A node only in `to` (the old clip never
// animated it) uses `to`'s value from t=0 -- there's no "old pose" to mix.
void ApplyBlendedPose(const std::unordered_map<TNode*, SampledPose>& from,
                      const std::unordered_map<TNode*, SampledPose>& to, float t) {
    for (const auto& [node, target] : to) {
        auto fromIt = from.find(node);
        bool hasFrom = fromIt != from.end();

        if (target.hasPosition) {
            node->transform.position = (hasFrom && fromIt->second.hasPosition)
                ? glm::mix(fromIt->second.position, target.position, t)
                : target.position;
        }
        if (target.hasScale) {
            node->transform.scale = (hasFrom && fromIt->second.hasScale)
                ? glm::mix(fromIt->second.scale, target.scale, t)
                : target.scale;
        }
        if (target.hasRotation) {
            node->transform.rotationQuat = (hasFrom && fromIt->second.hasRotation)
                ? glm::normalize(glm::slerp(fromIt->second.rotation, target.rotation, t))
                : target.rotation;
            node->transform.useQuatRotation = true;
        }
    }
}

} // namespace

AnimationComponent::AnimationComponent(TNode* owner) : owner(owner) {}

void AnimationComponent::AddClip(AnimationClip clip) {
    clips.push_back(std::move(clip));
}

void AnimationComponent::SetNodeIndexMap(std::unordered_map<int, TNode*> map) {
    nodeIndexMap = std::move(map);
}

const AnimationClip* AnimationComponent::FindClip(const std::string& name) const {
    for (const auto& clip : clips) {
        if (clip.name == name) return &clip;
    }
    return nullptr;
}

AnimationClip* AnimationComponent::FindClipMutable(const std::string& name) {
    for (auto& clip : clips) {
        if (clip.name == name) return &clip;
    }
    return nullptr;
}

void AnimationComponent::Play(const std::string& clipName, bool loop, float blendSeconds) {
    const AnimationClip* newClip = FindClip(clipName);
    if (!newClip) return;

    if (playing && blendSeconds > 0.0f && currentClipName != clipName) {
        const AnimationClip* oldClip = FindClip(currentClipName);
        if (oldClip) {
            blendFromPose = SamplePose(*oldClip, currentTime, nodeIndexMap);
            blendDuration = blendSeconds;
            blendElapsed = 0.0f;
            blending = true;
        }
    } else {
        blending = false;
    }

    currentClipName = clipName;
    currentTime = 0.0f;
    previousTime = 0.0f;
    looping = loop;
    playing = true;
}

void AnimationComponent::Stop() {
    playing = false;
}

void AnimationComponent::Update(float deltaTime) {
    if (!hasAppliedPlayOnStart) {
        hasAppliedPlayOnStart = true;
        if (playOnStart && !stateMachine && !playOnStartClip.empty()) {
            Play(playOnStartClip, true);
        }
    }

    if (stateMachine) {
        std::string clipName;
        bool loop = true;
        float blendSeconds = 0.0f;
        if (stateMachine->Evaluate(clipName, loop, blendSeconds)) {
            Play(clipName, loop, blendSeconds);
        }
    }

    if (!playing) return;

    const AnimationClip* clip = FindClip(currentClipName);
    if (!clip) {
        playing = false;
        return;
    }

    previousTime = currentTime;
    currentTime += deltaTime * speed;

    bool wrappedThisFrame = false;
    if (clip->duration > 0.0f && currentTime > clip->duration) {
        if (looping) {
            currentTime = std::fmod(currentTime, clip->duration);
            wrappedThisFrame = true;
        } else {
            currentTime = clip->duration;
            playing = false;
        }
    }

    for (const auto& event : clip->events) {
        bool crossedNormally = previousTime < event.time && currentTime >= event.time;
        bool crossedViaWrap = wrappedThisFrame && (event.time <= currentTime || event.time > previousTime);
        if ((crossedNormally || crossedViaWrap) && eventCallback) {
            eventCallback(event.name);
        }
    }

    std::unordered_map<TNode*, SampledPose> targetPose = SamplePose(*clip, currentTime, nodeIndexMap);

    if (blending) {
        blendElapsed += deltaTime;
        float t = blendDuration > 0.0f ? std::clamp(blendElapsed / blendDuration, 0.0f, 1.0f) : 1.0f;
        ApplyBlendedPose(blendFromPose, targetPose, t);
        if (t >= 1.0f) blending = false;
    } else {
        ApplySampledPose(targetPose);
    }
}

void AnimationComponent::PreviewPose(const std::string& clipName, float time) {
    const AnimationClip* clip = FindClip(clipName);
    if (!clip) return;
    ApplySampledPose(SamplePose(*clip, time, nodeIndexMap));
}

namespace {
constexpr float kKeyframeTimeEpsilon = 1e-4f;

template <typename KeyVec, typename Value>
void UpsertKey(KeyVec& keys, float time, const Value& value) {
    for (auto& k : keys) {
        if (std::abs(k.time - time) < kKeyframeTimeEpsilon) {
            k.value = value;
            return;
        }
    }
    keys.push_back({time, value});
    std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
}

template <typename KeyVec>
void RemoveKey(KeyVec& keys, float time) {
    keys.erase(std::remove_if(keys.begin(), keys.end(),
        [time](const auto& k) { return std::abs(k.time - time) < kKeyframeTimeEpsilon; }),
        keys.end());
}
} // namespace

void AnimationComponent::SetKeyframe(const std::string& clipName, float time, const Transform& transform) {
    AnimationClip* clip = FindClipMutable(clipName);
    if (!clip) return;

    auto findOrCreateChannel = [&](AnimationTargetPath path) -> AnimationChannelData& {
        for (auto& channel : clip->channels) {
            if (channel.path == path && channel.targetNodeIndex == 0) return channel;
        }
        AnimationChannelData channel;
        channel.targetNodeIndex = 0;
        channel.path = path;
        channel.interpolation = AnimationInterpolation::Linear;
        clip->channels.push_back(std::move(channel));
        return clip->channels.back();
    };

    UpsertKey(findOrCreateChannel(AnimationTargetPath::Translation).vec3Keys, time, transform.position);
    UpsertKey(findOrCreateChannel(AnimationTargetPath::Scale).vec3Keys, time, transform.scale);

    glm::quat rotationAsQuat = transform.useQuatRotation ? transform.rotationQuat : glm::quat(glm::radians(transform.rotation));
    UpsertKey(findOrCreateChannel(AnimationTargetPath::Rotation).quatKeys, time, rotationAsQuat);

    clip->duration = std::max(clip->duration, time);

    nodeIndexMap[0] = owner;
}

void AnimationComponent::RemoveKeyframe(const std::string& clipName, float time) {
    AnimationClip* clip = FindClipMutable(clipName);
    if (!clip) return;

    for (auto& channel : clip->channels) {
        RemoveKey(channel.vec3Keys, time);
        RemoveKey(channel.quatKeys, time);
    }

    float maxTime = 0.0f;
    for (const auto& channel : clip->channels) {
        for (const auto& k : channel.vec3Keys) maxTime = std::max(maxTime, k.time);
        for (const auto& k : channel.quatKeys) maxTime = std::max(maxTime, k.time);
    }
    clip->duration = maxTime;
}

std::vector<float> AnimationComponent::GetKeyframeTimes(const std::string& clipName) const {
    const AnimationClip* clip = FindClip(clipName);
    if (!clip) return {};

    std::vector<float> times;
    auto addTimes = [&](const auto& keys) {
        for (const auto& k : keys) {
            bool found = false;
            for (float t : times) {
                if (std::abs(t - k.time) < kKeyframeTimeEpsilon) { found = true; break; }
            }
            if (!found) times.push_back(k.time);
        }
    };
    for (const auto& channel : clip->channels) {
        addTimes(channel.vec3Keys);
        addTimes(channel.quatKeys);
    }

    std::sort(times.begin(), times.end());
    return times;
}

void AnimationComponent::AddEvent(const std::string& clipName, float time, const std::string& eventName) {
    AnimationClip* clip = FindClipMutable(clipName);
    if (!clip) return;
    clip->events.push_back({time, eventName});
    std::sort(clip->events.begin(), clip->events.end(),
        [](const AnimationEvent& a, const AnimationEvent& b) { return a.time < b.time; });
}

void AnimationComponent::SetEventCallback(AnimationEventCallback callback) {
    eventCallback = std::move(callback);
}

void AnimationComponent::SetStateMachine(std::unique_ptr<AnimationStateMachine> machine) {
    stateMachine = std::move(machine);
}

void AnimationComponent::Restart() {
    if (currentClipName.empty()) return;
    currentTime = 0.0f;
    previousTime = 0.0f;
    playing = true;
    blending = false;
}

AnimationStateMachine* AnimationComponent::GetOrCreateStateMachine() {
    if (!stateMachine) {
        stateMachine = std::make_unique<AnimationStateMachine>();
    }
    return stateMachine.get();
}
