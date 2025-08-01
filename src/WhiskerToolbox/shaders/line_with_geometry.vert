#version 410 core

layout(location = 0) in vec2 a_position;
layout(location = 1) in uint a_line_id;

uniform mat4 u_mvp_matrix;
uniform float u_line_width;
uniform vec2 u_canvas_size;

out vec2 v_position;
flat out uint v_line_id;

void main() {
    // Apply MVP transformation to the input position
    // Input coordinates are expected to be in world space (pixel coordinates)
    vec4 world_pos = vec4(a_position, 0.0, 1.0);
    vec4 ndc_pos = u_mvp_matrix * world_pos;
    
    v_position = ndc_pos.xy;
    v_line_id = a_line_id;
    gl_Position = ndc_pos;
}