#version 450 core
in vec2 FragUV;
out vec4 FragColor;

uniform sampler2D billboardTexture;
uniform bool hasTexture;
uniform vec4 tint;        // multiplied over the sampled color
uniform float alphaCutoff; // discard fragments below this alpha

void main()
{
    vec4 color = hasTexture ? texture(billboardTexture, FragUV) : vec4(1.0);
    color *= tint;

    if (color.a < alphaCutoff)
        discard;

    FragColor = color;
}
