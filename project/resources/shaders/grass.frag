#version 450 core
in vec2 FragUV;
out vec4 FragColor;

uniform sampler2D grassTexture;
uniform bool hasTexture;
uniform vec3 tint;
uniform float alphaCutoff;

void main()
{
    vec4 color = hasTexture ? texture(grassTexture, FragUV) : vec4(1.0);

    // Alpha-cutout: hard discard instead of blending, so grass renders in the
    // opaque pass with depth writes on and needs no back-to-front sorting.
    if (color.a < alphaCutoff)
        discard;

    // Cheap vertical gradient so the base reads slightly darker even without a
    // texture, and tint applied on top.
    float shade = mix(0.6, 1.0, FragUV.y);
    FragColor = vec4(color.rgb * tint * shade, 1.0);
}
