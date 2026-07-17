#version 450 core
layout (location = 0) in vec3 Pos;
layout (location = 1) in vec3 Normal;
layout (location = 2) in vec2 TexCoord;
layout (location = 3) in vec3 Tangent;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragNormal;
out vec2 FragTexCoord;
out vec3 WorldPos;
out mat3 TBN;

void main()
{
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vec3 normal = normalize(normalMatrix * Normal);
    FragNormal = normal;

    // Re-orthogonalize the tangent against the transformed normal (Gram-Schmidt)
    // since non-uniform scale can skew an already-normalized tangent.
    vec3 tangent = normalize(normalMatrix * Tangent);
    tangent = normalize(tangent - normal * dot(normal, tangent));
    vec3 bitangent = cross(normal, tangent);
    TBN = mat3(tangent, bitangent, normal);

    FragTexCoord = TexCoord;
    WorldPos = vec3(model * vec4(Pos, 1.0));
    gl_Position = projection * view * model * vec4(Pos, 1.0);
}
