#version 450 core
in vec3 FragNormal;
in vec2 FragTexCoord;
in vec3 WorldPos;
in mat3 TBN;
out vec4 FragColor;

uniform vec4 baseColor;      // base body color the iridescence is layered over
uniform vec3 cameraWorldPos;

// Look constants.
const float IRIDESCENCE_FREQ = 5.0; // how many rainbow cycles across the angle range
const float IRIDESCENCE_MIX = 0.85; // how strongly the rainbow overrides the base color

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

// Iñigo Quílez's cosine-based palette: cheap, smooth rainbow from a scalar t.
// a=bias, b=amp, c=freq, d=phase per channel.
vec3 Palette(float t)
{
    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.00, 0.33, 0.67); // R/G/B phase offsets -> full spectrum
    return a + b * cos(6.28318 * (c * t + d));
}

void main()
{
    vec3 N = normalize(FragNormal);
    vec3 V = normalize(cameraWorldPos - WorldPos);

    // Drive the palette by the view angle (thin-film interference shifts hue
    // with viewing angle -- the soap-bubble / opal effect).
    float NdotV = max(dot(N, V), 0.0);
    float fresnel = pow(1.0 - NdotV, 3.0);
    float hueParam = NdotV * IRIDESCENCE_FREQ + fresnel;
    vec3 iridescence = Palette(hueParam);

    // Blend the rainbow over the base body color, stronger at grazing angles.
    vec3 bodyColor = mix(baseColor.rgb, iridescence, IRIDESCENCE_MIX);

    // Standard-ish lighting so it still sits in the scene: diffuse + sharp
    // specular highlights (gems are glossy).
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
        float NdotL = max(dot(N, L), 0.0);
        lighting += light.color * light.intensity * attenuation * NdotL;

        vec3 H = normalize(V + L);
        specular += pow(max(dot(N, H), 0.0), 120.0) * attenuation;
    }

    // Brighten the iridescence at the rim (Fresnel) so edges shimmer.
    vec3 result = bodyColor * (0.3 + lighting) + iridescence * fresnel * 0.6 + vec3(specular);

    FragColor = vec4(result, baseColor.a);
}
