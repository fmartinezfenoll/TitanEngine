#version 450 core
// Simple 4x4 box blur of the raw SSAO texture, to smooth out the per-pixel
// kernel-rotation noise. Single channel in/out.
in vec2 TexCoord;
out float FragColor;

uniform sampler2D ssaoInput;

void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));
    float result = 0.0;
    for (int x = -2; x < 2; ++x)
    {
        for (int y = -2; y < 2; ++y)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(ssaoInput, TexCoord + offset).r;
        }
    }
    FragColor = result / 16.0;
}
