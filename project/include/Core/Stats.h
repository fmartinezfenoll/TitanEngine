#pragma once

class Stats {
public:
    static void BeginFrame() { drawCallsThisFrame = 0; }
    static void RecordDrawCall() { ++drawCallsThisFrame; }

    static void Tick(float deltaTime) {
        lastFrameDrawCalls = drawCallsThisFrame;

        fpsAccumTime += deltaTime;
        ++fpsFrameCount;
        if (fpsAccumTime >= 1.0f) {
            fps = static_cast<float>(fpsFrameCount) / fpsAccumTime;
            fpsAccumTime = 0.0f;
            fpsFrameCount = 0;
        }
    }

    static float GetFPS() { return fps; }
    static int GetDrawCalls() { return lastFrameDrawCalls; }

private:
    static inline int drawCallsThisFrame = 0;
    static inline int lastFrameDrawCalls = 0;
    static inline float fpsAccumTime = 0.0f;
    static inline int fpsFrameCount = 0;
    static inline float fps = 0.0f;
};
