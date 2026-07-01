#version 450 core
in vec3 vNormal;
in vec2 vTexCoord;
out vec4 FragColor;

uniform vec4 baseColor;
uniform bool hasAlbedoMap;
uniform sampler2D albedoMap;

void main()
{
    vec4 color = baseColor;
    if (hasAlbedoMap)
        color *= texture(albedoMap, vTexCoord);

    vec3 lightDir = normalize(vec3(0.4, 0.8, 0.6));
    float diffuse = max(dot(normalize(vNormal), lightDir), 0.0) * 0.7 + 0.3;

    FragColor = vec4(color.rgb * diffuse, color.a);
}
