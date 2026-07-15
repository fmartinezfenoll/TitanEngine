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
uniform bool hasMetallicRoughnessMap;
uniform sampler2D metallicRoughnessMap;
uniform float metallicFactor;
uniform float roughnessFactor;
uniform vec3 cameraWorldPos;

uniform bool hasIBL;
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;

const float PI = 3.14159265359;

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

// Trowbridge-Reitz / GGX normal distribution function.
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 0.0000001);
}

// Schlick-GGX geometry (single direction), combined via Smith's method below.
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    float denom = NdotV * (1.0 - k) + k;
    return NdotV / max(denom, 0.0000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Roughness-aware Fresnel: avoids over-bright grazing-angle highlights on
// rough surfaces when used for ambient (as opposed to direct-light) specular.
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
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

    // glTF convention: metallicRoughnessMap.g = roughness, .b = metallic.
    float metallic = metallicFactor;
    float roughness = roughnessFactor;
    if (hasMetallicRoughnessMap)
    {
        vec2 mr = texture(metallicRoughnessMap, FragTexCoord).bg;
        metallic *= mr.x;
        roughness *= mr.y;
    }
    roughness = clamp(roughness, 0.045, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);

    vec3 albedo = color.rgb;
    vec3 V = normalize(cameraWorldPos - WorldPos);

    // Dielectrics get a flat 4% reflectance at normal incidence; metals tint
    // their specular by their own albedo and contribute no diffuse term.
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 lighting = vec3(0.0);

    for (int i = 0; i < lightCount && i < MAX_LIGHTS; ++i)
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

        vec3 H = normalize(V + L);
        float NdotL = max(dot(normal, L), 0.0);
        float NdotV = max(dot(normal, V), 0.0);

        float shadowFactor = 1.0;
        if (light.shadowIndex >= 0)
        {
            if (light.type == 0 && hasDirectionalShadow)
            {
                shadowFactor = SampleShadow2D(directionalShadowMap, directionalLightSpaceMatrix, WorldPos, normal, L);
            }
            else if (light.type == 2 && light.shadowIndex < spotShadowCount)
            {
                shadowFactor = SampleShadow2D(spotShadowMaps[light.shadowIndex], spotLightSpaceMatrices[light.shadowIndex], WorldPos, normal, L);
            }
            else if (light.type == 1 && light.shadowIndex < pointShadowCount)
            {
                shadowFactor = SampleShadowCube(pointShadowMaps[light.shadowIndex], pointShadowLightPos[light.shadowIndex], pointShadowFarPlane[light.shadowIndex], WorldPos, normal, L);
            }
        }

        vec3 radiance = light.color * light.intensity * attenuation * shadowFactor;

        float NDF = DistributionGGX(normal, H, roughness);
        float G = GeometrySmith(normal, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 specular = (NDF * G * F) / max(4.0 * NdotV * NdotL, 0.0000001);

        // kS is the Fresnel reflectance itself; kD is what's left for diffuse,
        // zeroed out for metals since they have no subsurface scattering.
        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        lighting += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient;
    if (hasIBL)
    {
        vec3 F = FresnelSchlickRoughness(max(dot(normal, V), 0.0), F0, roughness);
        vec3 kS = F;
        vec3 kD = (1.0 - kS) * (1.0 - metallic);

        vec3 irradiance = texture(irradianceMap, normal).rgb;
        vec3 diffuse = irradiance * albedo;

        vec3 R = reflect(-V, normal);
        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
        vec2 envBRDF = texture(brdfLUT, vec2(max(dot(normal, V), 0.0), roughness)).rg;
        vec3 specularAmbient = prefilteredColor * (F * envBRDF.x + envBRDF.y);

        ambient = kD * diffuse + specularAmbient;
    }
    else
    {
        ambient = albedo * 0.03 * (1.0 - metallic);
    }

    vec3 result = (lightCount > 0)
        ? ambient + lighting
        : albedo * (max(dot(normal, normalize(vec3(0.4, 0.8, 0.6))), 0.0) * 0.7 + 0.3);

    FragColor = vec4(result, color.a);
}
