#version 450 core
layout (location = 0) in vec2 Corner; // [-0.5, 0.5] quad corner
layout (location = 1) in vec2 UV;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 worldCenter; // billboard center in world space
uniform vec2 size;        // width/height in world units

out vec2 FragUV;

void main()
{
    // The camera's right and up axes in world space are the first two rows of
    // the view matrix (== columns of its inverse-rotation). Using them to
    // expand the quad makes it always face the camera.
    vec3 camRight = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 camUp    = vec3(view[0][1], view[1][1], view[2][1]);

    vec3 worldPos = worldCenter
        + camRight * (Corner.x * size.x)
        + camUp    * (Corner.y * size.y);

    FragUV = UV;
    gl_Position = projection * view * vec4(worldPos, 1.0);
}
