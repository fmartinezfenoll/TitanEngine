#version 450 core
in vec3 FragNormal;
in vec2 FragTexCoord;
in vec3 WorldPos;
in mat3 TBN;
out vec4 FragColor;

uniform vec4 baseColor;
uniform bool hasAlbedoMap;
uniform sampler2D albedoMap;
uniform bool hasNormalMap;
uniform sampler2D normalMap;

#define MAX_LIGHTS 32
#define MAX_SHADOW_SPOT 2
#define MAX_SHADOW_POINT 2

struct Light {
    int type; // 0 = directional, 1 = point, 2 = spot
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float range;
    float innerCutoff;
    float outerCutoff;
    int shadowIndex; // -1 = no shadow, else index within the type-specific shadow array
};

uniform Light lights[MAX_LIGHTS];
uniform int lightCount;

uniform bool hasDirectionalShadow;
uniform sampler2D directionalShadowMap;
uniform mat4 directionalLightSpaceMatrix;

uniform int spotShadowCount;
uniform sampler2D spotShadowMaps[MAX_SHADOW_SPOT];
uniform mat4 spotLightSpaceMatrices[MAX_SHADOW_SPOT];

uniform int pointShadowCount;
uniform samplerCube pointShadowMaps[MAX_SHADOW_POINT];
uniform vec3 pointShadowLightPos[MAX_SHADOW_POINT];
uniform float pointShadowFarPlane[MAX_SHADOW_POINT];

float ComputeBias(vec3 normal, vec3 lightDir)
{
    return max(0.0025 * (1.0 - dot(normal, lightDir)), 0.0005);
}

float SampleShadow2D(sampler2D shadowMap, mat4 lightSpaceMatrix, vec3 worldPos, vec3 normal, vec3 lightDir)
{
    vec4 fragPosLightSpace = lightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 1.0;

    float bias = ComputeBias(normal, lightDir);
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));

    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (projCoords.z - bias) > pcfDepth ? 0.0 : 1.0;
        }
    }
    return shadow / 9.0;
}

float SampleShadowCube(samplerCube shadowMap, vec3 lightPos, float farPlane, vec3 worldPos, vec3 normal, vec3 lightDir)
{
    vec3 toFrag = worldPos - lightPos;
    float currentDist = length(toFrag);

    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.02);
    float closestDist = texture(shadowMap, toFrag).r * farPlane;

    return (currentDist - bias) > closestDist ? 0.0 : 1.0;
}

void main()
{
    vec4 color = baseColor;
    if (hasAlbedoMap)
        color *= texture(albedoMap, FragTexCoord);

    vec3 normal = normalize(FragNormal);
    if (hasNormalMap)
    {
        vec3 tangentNormal = texture(normalMap, FragTexCoord).rgb * 2.0 - 1.0;
        normal = normalize(TBN * tangentNormal);
    }
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
            vec3 toLight = light.position - WorldPos;
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

        float shadowFactor = 1.0;
        if (light.shadowIndex >= 0)
        {
            if (light.type == 0 && hasDirectionalShadow)
            {
                shadowFactor = SampleShadow2D(directionalShadowMap, directionalLightSpaceMatrix, WorldPos, normal, lightDir);
            }
            else if (light.type == 2 && light.shadowIndex < spotShadowCount)
            {
                shadowFactor = SampleShadow2D(spotShadowMaps[light.shadowIndex], spotLightSpaceMatrices[light.shadowIndex], WorldPos, normal, lightDir);
            }
            else if (light.type == 1 && light.shadowIndex < pointShadowCount)
            {
                shadowFactor = SampleShadowCube(pointShadowMaps[light.shadowIndex], pointShadowLightPos[light.shadowIndex], pointShadowFarPlane[light.shadowIndex], WorldPos, normal, lightDir);
            }
        }

        lighting += light.color * light.intensity * diffuseTerm * attenuation * shadowFactor;
    }

    vec3 ambient = color.rgb * 0.3;
    vec3 result = (lightCount > 0)
        ? ambient + color.rgb * lighting
        : color.rgb * (max(dot(normal, normalize(vec3(0.4, 0.8, 0.6))), 0.0) * 0.7 + 0.3);

    FragColor = vec4(result, color.a);
}
