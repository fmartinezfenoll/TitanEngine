#version 450 core
layout(location = 0) in vec3 Pos;

out vec3 TexCoord;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    TexCoord = Pos;
    vec4 pos = projection * mat4(mat3(view)) * vec4(Pos, 1.0);
    gl_Position = pos.xyww;
}
