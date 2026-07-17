#version 450 core
// Lighting-component demo: AMBIENT term only. Part of the side-by-side
// comparison (ambient / diffuse / specular / full) used in the theory chapter.
in vec3 FragNormal;
in vec2 FragTexCoord;
in vec3 WorldPos;
in mat3 TBN;
out vec4 FragColor;

uniform vec4 baseColor;
uniform vec3 cameraWorldPos;

const float AMBIENT_STRENGTH = 0.25;

void main()
{
    // The ambient term is a flat fraction of the base color, independent of
    // light direction or view. It fills the shadowed areas so nothing is pure
    // black; on its own it looks like an unlit, flat silhouette.
    vec3 ambient = AMBIENT_STRENGTH * baseColor.rgb;
    FragColor = vec4(ambient, baseColor.a);
}
