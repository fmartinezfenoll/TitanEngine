#version 450 core
layout (location = 0) in vec3 Pos;
layout (location = 1) in vec3 Normal;
layout (location = 2) in vec2 TexCoord;
layout (location = 3) in vec3 Tangent;
layout (location = 4) in ivec4 JointIndices;
layout (location = 5) in vec4 JointWeights;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

#define MAX_JOINTS 64
uniform mat4 jointMatrices[MAX_JOINTS];

out vec3 FragNormal;
out vec2 FragTexCoord;
out vec3 WorldPos;
out mat3 TBN;

void main()
{
    float totalWeight = JointWeights.x + JointWeights.y + JointWeights.z + JointWeights.w;

    mat4 skinMatrix;
    if (totalWeight < 0.0001)
    {
        skinMatrix = mat4(1.0);
    }
    else
    {
        skinMatrix =
            JointWeights.x * jointMatrices[JointIndices.x] +
            JointWeights.y * jointMatrices[JointIndices.y] +
            JointWeights.z * jointMatrices[JointIndices.z] +
            JointWeights.w * jointMatrices[JointIndices.w];
    }

    vec3 skinnedPos = vec3(skinMatrix * vec4(Pos, 1.0));
    vec3 skinnedNormal = mat3(skinMatrix) * Normal;
    vec3 skinnedTangent = mat3(skinMatrix) * Tangent;

    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vec3 normal = normalize(normalMatrix * skinnedNormal);
    FragNormal = normal;

    // Re-orthogonalize the tangent against the transformed normal (Gram-Schmidt)
    // since non-uniform scale can skew an already-normalized tangent.
    vec3 tangent = normalize(normalMatrix * skinnedTangent);
    tangent = normalize(tangent - normal * dot(normal, tangent));
    vec3 bitangent = cross(normal, tangent);
    TBN = mat3(tangent, bitangent, normal);

    FragTexCoord = TexCoord;
    WorldPos = vec3(model * vec4(skinnedPos, 1.0));
    gl_Position = projection * view * model * vec4(skinnedPos, 1.0);
}
