#pragma once

class Viewport
{
public:
    static void Set(int width, int height);
    static float GetAspectRatio();
    static int GetWidth() { return s_Width; }
    static int GetHeight() { return s_Height; }

private:
    static inline int s_Width = 1920;
    static inline int s_Height = 1080;
};
