#include "Renderer/Viewport.h"

void Viewport::Set(int width, int height)
{
    Width = width;
    Height = height;
}

float Viewport::GetAspectRatio()
{
    return Height > 0 ? static_cast<float>(Width) / static_cast<float>(Height) : 16.0f / 9.0f;
}
