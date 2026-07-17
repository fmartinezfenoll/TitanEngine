#version 450 core
// Fullscreen-triangle/quad vertex shader shared by every post-process pass.
// Expects a quad with positions in [-1,1] and UVs in [0,1].
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

out vec2 TexCoord;

void main()
{
    TexCoord = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
