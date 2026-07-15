#pragma once

#include <string>
#include <vector>
#include <unordered_map>

// A minimal Idle/Walk/Run-style animator: named states each mapping to a
// clip, and transitions between them gated by a named float/bool parameter
// condition. Evaluated once per frame by AnimationComponent::Update, which
// calls Play() on itself when a transition fires. Not a Component -- it has
// no meaning detached from the AnimationComponent that owns it (see
// AnimationComponent::SetStateMachine).
//
// Deliberately minimal: no blend trees, no exit-time-gated transitions, no
// sub-state machines -- "Any State" transitions (fromState == "") are the
// only prioritization mechanism, evaluated before the current state's own
// transitions.
class AnimationStateMachine {
public:
    struct State {
        std::string name;
        std::string clipName;
        bool loop = true;
    };

    enum class ConditionOp { GreaterThan, LessThan, Equals, NotEquals };

    struct Transition {
        std::string fromState; // "" = "Any State"
        std::string toState;
        std::string parameter;
        ConditionOp op = ConditionOp::Equals;
        float threshold = 0.0f;
        float blendSeconds = 0.2f;
    };

    void AddState(const std::string& name, const std::string& clipName, bool loop = true);
    void AddTransition(const std::string& fromState, const std::string& toState,
                        const std::string& parameter, ConditionOp op, float threshold,
                        float blendSeconds = 0.2f);
    void RemoveState(const std::string& name);
    void RemoveTransition(size_t index);

    void SetBool(const std::string& parameter, bool value);
    void SetFloat(const std::string& parameter, float value);

    void SetInitialState(const std::string& name);
    const std::string& GetInitialState() const { return initialState; }

    // Inspector editing access -- direct mutation of names/clips/conditions.
    // Renaming a state here does NOT rewrite existing Transition::fromState/
    // toState/initialState references to it; the Inspector editor is
    // responsible for that (see DebugUI's state-machine section).
    std::vector<State>& GetStatesMutable() { return states; }
    std::vector<Transition>& GetTransitionsMutable() { return transitions; }
    const std::vector<State>& GetStates() const { return states; }
    const std::vector<Transition>& GetTransitions() const { return transitions; }

    // Checks the current state's outgoing transitions (plus any "Any State"
    // ones) against current parameter values. If one matches, updates the
    // internal current state and returns true with the target state's clip
    // info written to the out-params. Returns false (out-params untouched)
    // if nothing fired -- caller should keep playing whatever's already
    // playing. The very first call (before any Evaluate has run) always
    // "transitions" into the initial state with no blend.
    bool Evaluate(std::string& outClipName, bool& outLoop, float& outBlendSeconds);

    const std::string& GetCurrentState() const { return currentState; }

private:
    const State* FindState(const std::string& name) const;
    bool ConditionMet(const Transition& transition) const;

    std::vector<State> states;
    std::vector<Transition> transitions;
    std::unordered_map<std::string, float> parameters; // bools stored as 0.0f/1.0f
    std::string currentState;
    std::string initialState;
    bool hasEnteredInitialState = false;
};
