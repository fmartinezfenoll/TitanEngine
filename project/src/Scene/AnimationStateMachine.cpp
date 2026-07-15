#include "Scene/AnimationStateMachine.h"
#include <algorithm>

void AnimationStateMachine::AddState(const std::string& name, const std::string& clipName, bool loop) {
    states.push_back({name, clipName, loop});
}

void AnimationStateMachine::AddTransition(const std::string& fromState, const std::string& toState,
                                           const std::string& parameter, ConditionOp op, float threshold,
                                           float blendSeconds) {
    transitions.push_back({fromState, toState, parameter, op, threshold, blendSeconds});
}

void AnimationStateMachine::RemoveState(const std::string& name) {
    states.erase(std::remove_if(states.begin(), states.end(),
        [&](const State& s) { return s.name == name; }), states.end());
    transitions.erase(std::remove_if(transitions.begin(), transitions.end(),
        [&](const Transition& t) { return t.fromState == name || t.toState == name; }), transitions.end());
    if (initialState == name) initialState.clear();
}

void AnimationStateMachine::RemoveTransition(size_t index) {
    if (index >= transitions.size()) return;
    transitions.erase(transitions.begin() + static_cast<long>(index));
}

void AnimationStateMachine::SetBool(const std::string& parameter, bool value) {
    parameters[parameter] = value ? 1.0f : 0.0f;
}

void AnimationStateMachine::SetFloat(const std::string& parameter, float value) {
    parameters[parameter] = value;
}

void AnimationStateMachine::SetInitialState(const std::string& name) {
    initialState = name;
}

const AnimationStateMachine::State* AnimationStateMachine::FindState(const std::string& name) const {
    for (const auto& state : states) {
        if (state.name == name) return &state;
    }
    return nullptr;
}

bool AnimationStateMachine::ConditionMet(const Transition& transition) const {
    auto it = parameters.find(transition.parameter);
    float value = (it != parameters.end()) ? it->second : 0.0f;

    switch (transition.op) {
        case ConditionOp::GreaterThan: return value > transition.threshold;
        case ConditionOp::LessThan:    return value < transition.threshold;
        case ConditionOp::Equals:      return value == transition.threshold;
        case ConditionOp::NotEquals:   return value != transition.threshold;
    }
    return false;
}

bool AnimationStateMachine::Evaluate(std::string& outClipName, bool& outLoop, float& outBlendSeconds) {
    if (!hasEnteredInitialState) {
        hasEnteredInitialState = true;
        const State* state = FindState(initialState);
        if (!state) return false;
        currentState = state->name;
        outClipName = state->clipName;
        outLoop = state->loop;
        outBlendSeconds = 0.0f;
        return true;
    }

    for (const auto& transition : transitions) {
        bool appliesToCurrentState = transition.fromState.empty() || transition.fromState == currentState;
        if (!appliesToCurrentState || transition.toState == currentState) continue;

        if (ConditionMet(transition)) {
            const State* target = FindState(transition.toState);
            if (!target) continue;

            currentState = target->name;
            outClipName = target->clipName;
            outLoop = target->loop;
            outBlendSeconds = transition.blendSeconds;
            return true;
        }
    }

    return false;
}
