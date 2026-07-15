#version 450 core
layout (location = 0) in vec2 Corner; // [-0.5, 0.5] quad corner
layout (location = 1) in vec2 UV;
layout (location = 2) in vec4 InstancePosSize; // xyz = world center, w = size
layout (location = 3) in vec4 InstanceColor;   // rgba, already faded on the CPU

uniform mat4 view;
uniform mat4 projection;

out vec2 FragUV;
out vec4 FragColor;

void main()
{
    vec3 camRight = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 camUp    = vec3(view[0][1], view[1][1], view[2][1]);

    float size = InstancePosSize.w;
    vec3 worldPos = InstancePosSize.xyz
        + camRight * (Corner.x * size)
        + camUp    * (Corner.y * size);

    FragUV = UV;
    FragColor = InstanceColor;
    gl_Position = projection * view * vec4(worldPos, 1.0);
}
