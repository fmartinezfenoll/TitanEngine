#pragma once

class Viewport
{
public:
    static void Set(int width, int height);
    static float GetAspectRatio();
    static int GetWidth() { return Width; }
    static int GetHeight() { return Height; }

private:
    static inline int Width = 1920;
    static inline int Height = 1080;
};
