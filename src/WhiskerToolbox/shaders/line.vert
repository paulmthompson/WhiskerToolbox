#version 410 core

layout(location = 0) in vec2 a_position;

uniform mat4 u_mvp_matrix;

void main() {
    gl_Position = u_mvp_matrix * vec4(a_position, 0.0, 1.0);
}