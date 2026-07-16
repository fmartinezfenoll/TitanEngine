#version 450 core
in vec3 FragNormal;
in vec2 FragTexCoord;
in vec3 WorldPos;
in mat3 TBN;
out vec4 FragColor;

uniform vec4 baseColor;
uniform bool hasAlbedoMap;
uniform sampler2D albedoMap;
uniform vec3 cameraWorldPos;

// 1-bit "Return of the Obra Dinn" look: shade to a single luminance value,
// then threshold it against an ordered Bayer matrix so the result is pure
// black or white with a stable screen-space dot pattern. All look constants
// (no extra uniforms), so MeshComponent::Draw already provides everything.
// Inverted 1-bit: the image is mostly dark INK, and only the brightly-lit
// parts light up as crisp white dots (like white text on black). PAPER stays
// bright so those lit fragments pop; the value curve below keeps most of the
// surface below threshold (ink).
const vec3 INK   = vec3(0.02, 0.02, 0.03); // dominant "black"
const vec3 PAPER = vec3(0.95, 0.94, 0.90); // bright "white" for the lit dots
// Pixels-per-dither-cell: >1 makes the dither pattern chunkier (retro), like a
// lower-resolution display. 1 = one cell per screen pixel.
const float PIXEL_SIZE = 2.0;

#define MAX_LIGHTS 32

struct Light {
    int type; // 0 = directional, 1 = point, 2 = spot
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float range;
    float innerCutoff;
    float outerCutoff;
    int shadowIndex;
};

uniform Light lights[MAX_LIGHTS];
uniform int lightCount;

// 4x4 Bayer ordered-dither threshold in [0,1) for the given screen cell.
float Bayer4x4(vec2 cell)
{
    const float m[16] = float[16](
         0.0/16.0,  8.0/16.0,  2.0/16.0, 10.0/16.0,
        12.0/16.0,  4.0/16.0, 14.0/16.0,  6.0/16.0,
         3.0/16.0, 11.0/16.0,  1.0/16.0,  9.0/16.0,
        15.0/16.0,  7.0/16.0, 13.0/16.0,  5.0/16.0
    );
    int x = int(mod(cell.x, 4.0));
    int y = int(mod(cell.y, 4.0));
    return m[y * 4 + x];
}

void main()
{
    vec4 color = baseColor;
    if (hasAlbedoMap)
        color *= texture(albedoMap, FragTexCoord);

    vec3 normal = normalize(FragNormal);

    // Simple diffuse accumulation (color is irrelevant -- only luminance drives
    // the 1-bit output, but albedo still modulates how bright a surface reads).
    float lit = 0.0;
    int count = min(lightCount, MAX_LIGHTS);
    for (int i = 0; i < count; ++i)
    {
        Light light = lights[i];
        vec3 L;
        float attenuation = 1.0;

        if (light.type == 0)
        {
            L = normalize(-light.direction);
        }
        else
        {
            vec3 toLight = light.position - WorldPos;
            float dist = length(toLight);
            L = dist > 0.0001 ? toLight / dist : vec3(0.0, 1.0, 0.0);
            float falloff = dist / max(light.range, 0.0001);
            attenuation = 1.0 / (1.0 + falloff * falloff);

            if (light.type == 2)
            {
                float cosAngle = dot(normalize(-L), normalize(light.direction));
                float spotRange = max(light.innerCutoff - light.outerCutoff, 0.0001);
                attenuation *= clamp((cosAngle - light.outerCutoff) / spotRange, 0.0, 1.0);
            }
        }

        float NdotL = max(dot(normal, L), 0.0);
        lit += NdotL * light.intensity * attenuation;
    }

    // Low ambient so shadowed sides read as dark ink rather than a mid dither.
    float ambient = 0.08;
    float shade = ambient + lit;

    // Weight by albedo luminance so darker-painted surfaces read as darker.
    float albedoLum = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    float value = clamp(shade * albedoLum, 0.0, 1.0);

    // Strongly bias toward dark: only well-lit fragments survive above the
    // dither threshold as white dots, everything else collapses to ink. This is
    // what gives the "dark image, only the lit parts show in white" look.
    value = pow(value, 2.4);

    // Ordered dithering: white where the shaded value beats the Bayer threshold.
    vec2 cell = floor(gl_FragCoord.xy / PIXEL_SIZE);
    float threshold = Bayer4x4(cell);
    vec3 result = value > threshold ? PAPER : INK;

    FragColor = vec4(result, color.a);
}
