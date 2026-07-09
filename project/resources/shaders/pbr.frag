#version 450 core
in vec3 vNormal;
in vec2 vTexCoord;
in vec3 vWorldPos;
out vec4 FragColor;

uniform vec4 baseColor;
uniform bool hasAlbedoMap;
uniform sampler2D albedoMap;

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
};

uniform Light lights[MAX_LIGHTS];
uniform int lightCount;

void main()
{
    vec4 color = baseColor;
    if (hasAlbedoMap)
        color *= texture(albedoMap, vTexCoord);

    vec3 normal = normalize(vNormal);
    vec3 lighting = vec3(0.0);

    for (int i = 0; i < lightCount && i < MAX_LIGHTS; ++i)
    {
        Light light = lights[i];
        vec3 lightDir;
        float attenuation = 1.0;

        if (light.type == 0)
        {
            lightDir = normalize(-light.direction);
        }
        else
        {
            vec3 toLight = light.position - vWorldPos;
            float dist = length(toLight);
            lightDir = dist > 0.0001 ? toLight / dist : vec3(0.0, 1.0, 0.0);

            float falloff = dist / max(light.range, 0.0001);
            attenuation = 1.0 / (1.0 + falloff * falloff);

            if (light.type == 2)
            {
                float cosAngle = dot(normalize(-lightDir), normalize(light.direction));
                float spotRange = max(light.innerCutoff - light.outerCutoff, 0.0001);
                attenuation *= clamp((cosAngle - light.outerCutoff) / spotRange, 0.0, 1.0);
            }
        }

        float diffuseTerm = max(dot(normal, lightDir), 0.0);
        lighting += light.color * light.intensity * diffuseTerm * attenuation;
    }

    vec3 ambient = color.rgb * 0.3;
    vec3 result = (lightCount > 0)
        ? ambient + color.rgb * lighting
        : color.rgb * (max(dot(normal, normalize(vec3(0.4, 0.8, 0.6))), 0.0) * 0.7 + 0.3);

    FragColor = vec4(result, color.a);
}
