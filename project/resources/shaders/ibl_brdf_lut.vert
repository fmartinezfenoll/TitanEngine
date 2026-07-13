#version 450 core
out vec2 TexCoord;

void main() {
    // Fullscreen triangle from gl_VertexID, no vertex buffer needed.
    vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    TexCoord = pos;
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
