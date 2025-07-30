#version 410 core

layout (location = 0) in vec2 a_position; // Position (x, y) 
layout (location = 1) in float a_group_id; // Group ID

uniform mat4 u_mvp_matrix;
uniform float u_point_size;

// Pass group_id to fragment shader
flat out int v_group_id;

void main()
{
    gl_Position = u_mvp_matrix * vec4(a_position, 0.0, 1.0);
    gl_PointSize = u_point_size;
    v_group_id = int(a_group_id);
}