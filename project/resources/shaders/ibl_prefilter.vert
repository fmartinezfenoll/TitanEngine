#version 450 core
layout (location = 0) in vec3 Pos;

uniform mat4 view;
uniform mat4 projection;

out vec3 LocalPos;

void main() {
    LocalPos = Pos;
    gl_Position = projection * view * vec4(Pos, 1.0);
}
