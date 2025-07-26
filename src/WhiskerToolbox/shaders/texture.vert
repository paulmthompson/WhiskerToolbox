#version 410 core

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_texCoord;

uniform mat4 u_mvp_matrix;

out vec2 v_texCoord;

void main() {
    gl_Position = u_mvp_matrix * vec4(a_position, 0.0, 1.0);
    v_texCoord = a_texCoord;
} 