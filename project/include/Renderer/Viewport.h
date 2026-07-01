#pragma once

class Viewport
{
public:
    static void Set(int width, int height);
    static float GetAspectRatio();

private:
    static inline int s_Width = 1920;
    static inline int s_Height = 1080;
};
