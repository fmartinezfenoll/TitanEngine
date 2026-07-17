#version 450 core
// Separable Gaussian blur. Run once horizontally and once vertically (ping-pong
// between two framebuffers) for a full 2D blur at low cost. Used for the bloom
// halo and to smooth the raw SSAO term.
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D image;
uniform bool horizontal;

// 5-tap Gaussian weights (center + 4 neighbours).
const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(image, 0));
    vec3 result = texture(image, TexCoord).rgb * weights[0];

    if (horizontal)
    {
        for (int i = 1; i < 5; ++i)
        {
            vec2 off = vec2(texelSize.x * float(i), 0.0);
            result += texture(image, TexCoord + off).rgb * weights[i];
            result += texture(image, TexCoord - off).rgb * weights[i];
        }
    }
    else
    {
        for (int i = 1; i < 5; ++i)
        {
            vec2 off = vec2(0.0, texelSize.y * float(i));
            result += texture(image, TexCoord + off).rgb * weights[i];
            result += texture(image, TexCoord - off).rgb * weights[i];
        }
    }

    FragColor = vec4(result, 1.0);
}
