#include "Renderer/Viewport.h"

void Viewport::Set(int width, int height)
{
    s_Width = width;
    s_Height = height;
}

float Viewport::GetAspectRatio()
{
    return s_Height > 0 ? static_cast<float>(s_Width) / static_cast<float>(s_Height) : 16.0f / 9.0f;
}
