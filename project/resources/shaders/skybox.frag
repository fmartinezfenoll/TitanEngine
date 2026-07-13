#version 450 core
in vec3 TexCoord;
out vec4 FragColor;

uniform samplerCube skyboxMap;

void main()
{
    FragColor = texture(skyboxMap, TexCoord);
}
