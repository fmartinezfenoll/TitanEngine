#version 450 core
// Final HDR composition: add the blurred bloom back over the scene, apply
// exposure + ACES filmic tone mapping (HDR -> LDR), then gamma-correct. Output
// is low-dynamic-range, ready for the screen (or the FXAA pass).
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D sceneTexture; // HDR scene
uniform sampler2D bloomTexture; // blurred bright pass
uniform sampler2D ssaoTexture;  // ambient occlusion factor (1 = unoccluded)
uniform bool bloomEnabled;
uniform float bloomIntensity;
uniform bool tonemapEnabled;
uniform float exposure;
uniform bool ssaoEnabled;

// ACES filmic tone-mapping curve (Narkowicz approximation): compresses the HDR
// range into [0,1] while keeping saturated highlights and shadow detail.
vec3 ACESFilmic(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 hdr = texture(sceneTexture, TexCoord).rgb;

    // Apply ambient occlusion by darkening the scene where geometry is occluded.
    // Done before bloom so occluded creases don't also glow.
    if (ssaoEnabled)
        hdr *= texture(ssaoTexture, TexCoord).r;

    if (bloomEnabled)
        hdr += texture(bloomTexture, TexCoord).rgb * bloomIntensity;

    vec3 color;
    if (tonemapEnabled)
    {
        color = ACESFilmic(hdr * exposure);
    }
    else
    {
        color = clamp(hdr, 0.0, 1.0);
    }

    // Gamma correction (linear -> sRGB).
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, 1.0);
}
