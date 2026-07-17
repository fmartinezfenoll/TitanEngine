#version 450 core
// Lighting-component demo: SPECULAR (Blinn-Phong) term only.
in vec3 FragNormal;
in vec2 FragTexCoord;
in vec3 WorldPos;
in mat3 TBN;
out vec4 FragColor;

uniform vec4 baseColor;
uniform vec3 cameraWorldPos;

const float SHININESS = 48.0;

#define MAX_LIGHTS 32
struct Light {
    int type; vec3 position; vec3 direction; vec3 color;
    float intensity; float range; float innerCutoff; float outerCutoff; int shadowIndex;
};
uniform Light lights[MAX_LIGHTS];
uniform int lightCount;

vec3 lightDir(Light light, out float attenuation)
{
    attenuation = 1.0;
    if (light.type == 0) return normalize(-light.direction);
    vec3 toLight = light.position - WorldPos;
    float dist = length(toLight);
    float falloff = dist / max(light.range, 0.0001);
    attenuation = 1.0 / (1.0 + falloff * falloff);
    return dist > 0.0001 ? toLight / dist : vec3(0.0, 1.0, 0.0);
}

void main()
{
    vec3 N = normalize(FragNormal);
    vec3 V = normalize(cameraWorldPos - WorldPos);
    vec3 specular = vec3(0.0);
    int count = min(lightCount, MAX_LIGHTS);
    for (int i = 0; i < count; ++i)
    {
        float att;
        vec3 L = lightDir(lights[i], att);
        // Blinn-Phong: highlight based on the angle between the normal and the
        // halfway vector between light and view. Shown alone, it is just the
        // bright specular dot with no body underneath.
        vec3 H = normalize(V + L);
        float NdotH = max(dot(N, H), 0.0);
        float spec = pow(NdotH, SHININESS) * (dot(N, L) > 0.0 ? 1.0 : 0.0);
        specular += lights[i].color * lights[i].intensity * att * spec;
    }
    FragColor = vec4(specular, baseColor.a);
}
