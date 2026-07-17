#version 450 core
// Bloom step 1: keep only the fragments brighter than a threshold; everything
// else becomes black. The result is later blurred and added back.
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D sceneTexture; // HDR scene
uniform float threshold;

void main()
{
    vec3 color = texture(sceneTexture, TexCoord).rgb;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    // Soft knee: fade in over a small range above the threshold so the bloom
    // doesn't pop on/off harshly at the cutoff.
    float contribution = clamp((luminance - threshold) / max(threshold, 0.0001), 0.0, 1.0);
    FragColor = vec4(color * contribution, 1.0);
}
