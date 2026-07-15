#version 450 core
layout (location = 0) in vec2 Corner; // [-0.5, 0.5] quad corner
layout (location = 1) in vec2 UV;
layout (location = 2) in vec4 Instance; // xyz = local offset within the patch, w = yaw (radians)

uniform mat4 model;      // the grass node's world transform
uniform mat4 view;
uniform mat4 projection;
uniform vec2 bladeSize;  // width/height of one blade, world units
uniform float time;
uniform float windStrength;
uniform float windSpeed;

out vec2 FragUV;

void main()
{
    // Rotate the flat blade around Y by the per-instance yaw so blades face
    // different directions (cross-quad look comes from two instances per blade,
    // set up on the CPU with offset yaws).
    float c = cos(Instance.w);
    float s = sin(Instance.w);
    vec3 local = vec3(Corner.x * bladeSize.x, (Corner.y + 0.5) * bladeSize.y, 0.0);
    vec3 rotated = vec3(local.x * c - local.z * s, local.y, local.x * s + local.z * c);

    vec3 pos = Instance.xyz + rotated;

    // Wind sways the top of the blade (UV.y ~ 1) more than the base (UV.y ~ 0).
    float sway = sin(time * windSpeed + Instance.x * 0.7 + Instance.z * 0.9) * windStrength;
    pos.x += sway * UV.y;
    pos.z += sway * 0.5 * UV.y;

    FragUV = UV;
    gl_Position = projection * view * model * vec4(pos, 1.0);
}
