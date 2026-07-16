#version 450 core
in vec3 FragNormal;
in vec2 FragTexCoord;
in vec3 WorldPos;
in mat3 TBN;
out vec4 FragColor;

uniform vec4 baseColor;      // tint: blue = water, green = toxic, orange = lava
uniform vec3 cameraWorldPos;
uniform float time;          // seconds; drives the wave animation

// Look constants (no extra C++ upload needed).
const float WAVE_SCALE = 0.9;    // spatial frequency of the ripples
const float WAVE_SPEED = 1.4;    // how fast the ripples move
const float WAVE_HEIGHT = 0.18;  // how much the surface normal is perturbed
const vec3  DEEP_COLOR = vec3(0.02, 0.08, 0.12);  // color at steep view angles
const float FRESNEL_POWER = 3.0; // how tight the rim reflection is

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

// Cheap procedural height field: sum of a few moving sine waves sampled from
// the fragment's world XZ position. Returns a height used only to derive a
// perturbed normal (the mesh itself isn't displaced here).
float WaveHeight(vec2 p)
{
    float t = time * WAVE_SPEED;
    float h = 0.0;
    h += sin(p.x * WAVE_SCALE + t);
    h += sin(p.y * WAVE_SCALE * 1.3 - t * 0.8);
    h += 0.5 * sin((p.x + p.y) * WAVE_SCALE * 1.7 + t * 1.3);
    return h;
}

void main()
{
    // Perturb the surface normal by the gradient of the wave field (finite
    // differences), so lighting and Fresnel ripple even on a flat plane.
    vec2 p = WorldPos.xz;
    float eps = 0.15;
    float hL = WaveHeight(p - vec2(eps, 0.0));
    float hR = WaveHeight(p + vec2(eps, 0.0));
    float hD = WaveHeight(p - vec2(0.0, eps));
    float hU = WaveHeight(p + vec2(0.0, eps));
    vec3 baseN = normalize(FragNormal);
    vec3 perturbed = normalize(baseN + vec3((hL - hR) * WAVE_HEIGHT, 0.0, (hD - hU) * WAVE_HEIGHT));

    vec3 V = normalize(cameraWorldPos - WorldPos);

    // Lighting (diffuse + a tight specular for wet glints).
    vec3 lighting = vec3(0.0);
    float specular = 0.0;
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
        float NdotL = max(dot(perturbed, L), 0.0);
        lighting += light.color * light.intensity * attenuation * NdotL;

        vec3 H = normalize(V + L);
        float NdotH = max(dot(perturbed, H), 0.0);
        specular += pow(NdotH, 80.0) * attenuation;
    }

    // Fresnel: shallow (grazing) angles reflect more -> brighter tint edge,
    // steep (top-down) angles show the deep color.
    float fresnel = pow(1.0 - max(dot(perturbed, V), 0.0), FRESNEL_POWER);
    vec3 surface = mix(DEEP_COLOR, baseColor.rgb, fresnel);

    vec3 result = surface * (0.25 + lighting) + vec3(specular);

    // Slight transparency so it reads as liquid (needs the material's
    // "transparent" flag on to actually blend; opaque still looks fine).
    float alpha = mix(0.75, 1.0, fresnel) * baseColor.a;
    FragColor = vec4(result, alpha);
}
