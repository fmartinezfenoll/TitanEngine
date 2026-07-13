#version 450 core
in vec4 FragPos;

uniform vec3 lightPos;
uniform float farPlane;

void main() {
    float dist = length(FragPos.xyz - lightPos);
    gl_FragDepth = dist / farPlane;
}
