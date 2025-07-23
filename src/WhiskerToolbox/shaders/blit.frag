#version 410 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D u_texture;

void main()
{
    FragColor = texture(u_texture, TexCoords);
} 