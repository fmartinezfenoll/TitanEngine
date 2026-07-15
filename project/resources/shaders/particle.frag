#version 450 core
in vec2 FragUV;
in vec4 FragColor;
out vec4 OutColor;

uniform sampler2D particleTexture;
uniform bool hasTexture;

void main()
{
    vec4 color = FragColor;

    if (hasTexture) {
        color *= texture(particleTexture, FragUV);
    } else {
        // No texture: soft round falloff so bare particles look like a glow
        // instead of a hard square.
        float d = length(FragUV - vec2(0.5));
        float soft = smoothstep(0.5, 0.0, d);
        color.a *= soft;
    }

    if (color.a <= 0.001)
        discard;

    OutColor = color;
}
