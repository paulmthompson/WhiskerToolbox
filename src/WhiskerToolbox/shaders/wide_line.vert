#version 410 core

layout(location = 0) in int a_time_index;
layout(location = 1) in float a_value;

uniform int u_view_start_sample;
uniform mat4 u_mvp_matrix;

void main() {
    int rel_sample = a_time_index - u_view_start_sample;
    gl_Position = u_mvp_matrix * vec4(float(rel_sample), a_value, 0.0, 1.0);
}
