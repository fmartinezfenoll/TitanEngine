#pragma once

#include "Scene/Component.h"
#include "Scene/AnimationClip.h"
#include "Scene/AnimationStateMachine.h"
#include "Scene/TNode.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <memory>

// A single node's sampled pose at some point in time -- used internally to
// snapshot the outgoing clip's pose when a blended Play() starts, and to
// mix it with the incoming clip's pose each frame until the blend finishes.
struct SampledPose {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    bool hasPosition = false;
    bool hasRotation = false;
    bool hasScale = false;
};

// Plays back AnimationClips (parsed from glTF, or restored from a saved
// scene) against a specific instance's TNode subtree. Lives on the model's
// root TNode. See SetNodeIndexMap for how clip channels (which reference
// nodes by a tree-position index, not a raw TNode*) get resolved to this
// instance's actual nodes.
class AnimationComponent : public Component {
public:
    using AnimationEventCallback = std::function<void(const std::string& eventName)>;

    explicit AnimationComponent(TNode* owner);

    void AddClip(AnimationClip clip);

    // Maps a channel's targetNodeIndex (pre-order position within the
    // animated subtree) to this instance's TNode*. Built by GLTFLoader at
    // load time, or by SceneSerializer's post-process pass after loading a
    // saved scene.
    void SetNodeIndexMap(std::unordered_map<int, TNode*> map);

    // blendSeconds > 0 and a clip already playing: crossfades from the
    // current pose into the new clip over blendSeconds instead of cutting
    // instantly. blendSeconds = 0 (default) preserves the old hard-cut
    // behavior -- existing callers are unaffected.
    void Play(const std::string& clipName, bool loop = true, float blendSeconds = 0.0f);
    void Stop();
    void Update(float deltaTime);

    // Applies the given clip's pose at `time` immediately, without affecting
    // playback state (playing/currentClipName/currentTime) -- used by the
    // Inspector's keyframe editor to preview a pose while scrubbing.
    void PreviewPose(const std::string& clipName, float time);

    // Manual keyframe editing (Inspector's keyframe editor). Writes to all
    // three channels (translation/rotation/scale) together at `time`,
    // creating the clip's channels on first use. Coexists with clips
    // authored by GLTFLoader, whose per-channel key times may differ from
    // each other -- SamplePose already samples each channel independently.
    void SetKeyframe(const std::string& clipName, float time, const Transform& transform);
    void RemoveKeyframe(const std::string& clipName, float time);
    std::vector<float> GetKeyframeTimes(const std::string& clipName) const;
    AnimationClip* FindClipMutable(const std::string& name);

    // Author-time event at `time` in `clipName` -- fired via the event
    // callback (see SetEventCallback) whenever playback crosses that time,
    // in either normal playback or on loop wrap-around.
    void AddEvent(const std::string& clipName, float time, const std::string& eventName);
    // Replaces any previously-set callback -- one listener per component.
    void SetEventCallback(AnimationEventCallback callback);

    // Optional: if set, this state machine decides which clip to Play() each
    // frame based on parameters set via GetStateMachine()->SetBool/SetFloat.
    // See AnimationStateMachine.h. Ownership transfers to this component.
    void SetStateMachine(std::unique_ptr<AnimationStateMachine> machine);
    AnimationStateMachine* GetStateMachine() const { return stateMachine.get(); }
    // Lazily creates an empty state machine if none exists yet -- lets the
    // Inspector's state-machine editor start from scratch without requiring
    // code to call SetStateMachine first.
    AnimationStateMachine* GetOrCreateStateMachine();
    void RemoveStateMachine() { stateMachine.reset(); }

    const std::vector<AnimationClip>& GetClips() const { return clips; }
    bool IsPlaying() const { return playing; }
    const std::string& GetCurrentClip() const { return currentClipName; }
    float GetCurrentTime() const { return currentTime; }
    bool IsLooping() const { return looping; }
    void SetLooping(bool loop) { looping = loop; }
    float GetSpeed() const { return speed; }
    void SetSpeed(float s) { speed = s; }

    // If true and no state machine is set, Update() auto-Plays playOnStartClip
    // (looping) the first time it runs -- lets a standalone clip start without
    // any code in Application.cpp. Ignored when a state machine is present
    // (its own initial-state transition already plays something on the first
    // Evaluate()).
    void SetPlayOnStart(bool enabled, const std::string& clipName = "") {
        playOnStart = enabled;
        playOnStartClip = clipName;
    }
    bool GetPlayOnStart() const { return playOnStart; }
    const std::string& GetPlayOnStartClip() const { return playOnStartClip; }

    // Resumes advancing currentTime from wherever it was left (unlike Play(),
    // which always resets to time 0). No-op if a clip isn't already loaded.
    void Pause() { playing = false; }
    void Resume() { if (!currentClipName.empty()) playing = true; }
    bool IsPaused() const { return !playing && !currentClipName.empty(); }
    // Replays the current clip from time 0, keeping the same clip/loop/speed.
    void Restart();

private:
    const AnimationClip* FindClip(const std::string& name) const;

    TNode* owner;
    std::vector<AnimationClip> clips;
    std::unordered_map<int, TNode*> nodeIndexMap;
    std::string currentClipName;
    float currentTime = 0.0f;
    float previousTime = 0.0f; // used to detect event-time crossings each frame
    float speed = 1.0f;
    bool playing = false;
    bool looping = true;

    bool playOnStart = false;
    std::string playOnStartClip;
    bool hasAppliedPlayOnStart = false;

    // Blending state -- see SamplePose()/ApplyBlendedPose() in the .cpp.
    std::unordered_map<TNode*, SampledPose> blendFromPose;
    float blendDuration = 0.0f;
    float blendElapsed = 0.0f;
    bool blending = false;

    AnimationEventCallback eventCallback;
    std::unique_ptr<AnimationStateMachine> stateMachine;
};
