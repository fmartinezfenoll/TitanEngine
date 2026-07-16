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
uniform float time;

// Scroll velocity (UV units per second). Change these for conveyor belts,
// waterfalls, flowing lava, drifting clouds, etc.
const vec2 SCROLL_SPEED = vec2(0.15, 0.0);
const float TILING = 4.0; // how many times the texture repeats across the surface

#define MAX_LIGHTS 32

struct Light {
    int type;
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

// Procedural checkerboard used when no albedo texture is assigned, so the
// scroll is visible without needing an image asset.
vec3 Checker(vec2 uv)
{
    vec2 c = floor(uv * 8.0);
    float checker = mod(c.x + c.y, 2.0);
    return mix(vec3(0.15), vec3(0.85), checker);
}

void main()
{
    // The whole effect: offset the UVs by time * speed before sampling.
    vec2 uv = FragTexCoord * TILING + SCROLL_SPEED * time;

    vec3 texel;
    if (hasAlbedoMap)
        texel = texture(albedoMap, uv).rgb;
    else
        texel = Checker(uv);

    vec3 albedo = texel * baseColor.rgb;

    // Basic diffuse lighting so it sits in the scene.
    vec3 normal = normalize(FragNormal);
    vec3 lighting = vec3(0.0);
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
        lighting += light.color * light.intensity * attenuation * NdotL;
    }

    vec3 result = albedo * (0.25 + lighting);
    FragColor = vec4(result, baseColor.a);
}
