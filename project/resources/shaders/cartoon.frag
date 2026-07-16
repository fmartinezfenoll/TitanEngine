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

// Cel-shading look constants (not per-material uniforms, so this shader needs
// no extra C++ upload -- MeshComponent::Draw already provides everything else).
const int   TOON_BANDS = 3;          // discrete lighting steps (lower = chunkier)
const float BAND_SOFTNESS = 1.0;     // width (in pixels) of the anti-aliased step edge
const float OUTLINE_THICKNESS = 0.30;// view-angle silhouette rim width [0,1]
const float OUTLINE_SHARPNESS = 2.5; // >1 makes the outline a crisper ink line
const vec3  OUTLINE_COLOR = vec3(0.02, 0.02, 0.03);
const float SPECULAR_SIZE = 0.25;    // 0..1, size of the hard specular blob (0 = none)
const vec3  SHADOW_TINT = vec3(0.35, 0.45, 0.7); // cool tint mixed into shadowed areas
const float SHADOW_TINT_AMOUNT = 0.35;           // how strongly shadows take the tint
const vec3  RIM_COLOR = vec3(1.0, 1.0, 0.95);    // bright edge highlight
const float RIM_WIDTH = 0.5;         // how far in the rim light reaches [0,1]

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

// Quantizes a 0..1 value into `bands` steps, but with a soft (anti-aliased)
// transition between steps sized by the screen-space derivative so the band
// edges don't look jagged. fwidth gives the per-pixel rate of change of `value`.
float ToonStepSoft(float value, int bands)
{
    float b = float(max(bands, 1));
    float scaled = clamp(value, 0.0, 1.0) * b;
    float lower = floor(scaled);
    float frac = scaled - lower;
    // Soften the jump around frac==0/1 using the local gradient.
    float aa = max(fwidth(scaled), 0.0001) * BAND_SOFTNESS;
    float stepped = lower + smoothstep(1.0 - aa, 1.0, frac);
    return stepped / b;
}

void main()
{
    vec4 color = baseColor;
    if (hasAlbedoMap)
        color *= texture(albedoMap, FragTexCoord);

    vec3 normal = normalize(FragNormal);
    vec3 V = normalize(cameraWorldPos - WorldPos);

    vec3 lighting = vec3(0.0);
    float specular = 0.0;
    float totalLight = 0.0; // unbanded light amount, used for the shadow tint

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

        // Toon diffuse: quantize N.L into soft-edged hard bands.
        float NdotL = max(dot(normal, L), 0.0);
        float toon = ToonStepSoft(NdotL, TOON_BANDS);
        lighting += light.color * light.intensity * attenuation * toon;
        totalLight += NdotL * light.intensity * attenuation;

        // Hard specular blob (Blinn-Phong thresholded to on/off).
        if (SPECULAR_SIZE > 0.0)
        {
            vec3 H = normalize(V + L);
            float NdotH = max(dot(normal, H), 0.0);
            float spec = pow(NdotH, mix(128.0, 8.0, SPECULAR_SIZE));
            specular += step(0.5, spec) * attenuation;
        }
    }

    // Fixed ambient so unlit sides don't go pure black.
    vec3 ambient = color.rgb * 0.25;
    vec3 result = ambient + color.rgb * lighting + vec3(specular);

    // Cool shadow tint: blend a cold hue into fragments that receive little
    // light, so shadows read as colored rather than just darker.
    float shadowFactor = 1.0 - smoothstep(0.0, 0.6, totalLight);
    result = mix(result, result * SHADOW_TINT, shadowFactor * SHADOW_TINT_AMOUNT);

    // Rim light: a bright edge where the surface faces away from the camera AND
    // is at least somewhat lit -- adds the stylized anime edge glow.
    float rimAmount = 1.0 - max(dot(normal, V), 0.0);
    float rim = smoothstep(1.0 - RIM_WIDTH, 1.0, rimAmount) * smoothstep(0.05, 0.4, totalLight);
    result += RIM_COLOR * rim;

    // Silhouette outline: darken the very edge of the object into a crisp ink
    // line. Sharpened with a power curve so it reads as a line, not a gradient.
    float edge = pow(smoothstep(1.0 - OUTLINE_THICKNESS, 1.0, rimAmount), OUTLINE_SHARPNESS);
    result = mix(result, OUTLINE_COLOR, edge);

    FragColor = vec4(result, color.a);
}
