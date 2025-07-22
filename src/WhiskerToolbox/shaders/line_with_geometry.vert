#version 410 core

layout(location = 0) in vec2 a_position;
layout(location = 1) in uint a_line_id;

uniform mat4 u_mvp_matrix;
uniform float u_line_width;

out vec2 v_position;
out uint v_line_id;

void main() {
    v_position = a_position;
    v_line_id = a_line_id;
    gl_Position = u_mvp_matrix * vec4(a_position, 0.0, 1.0);
}