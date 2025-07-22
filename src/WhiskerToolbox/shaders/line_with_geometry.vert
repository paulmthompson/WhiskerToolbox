#version 410 core

layout(location = 0) in vec2 a_position;
layout(location = 1) in uint a_line_id;

uniform float u_line_width;
uniform vec2 u_canvas_size;

out vec2 v_position;
flat out uint v_line_id;

void main() {
    // Normalize pixel coordinates to NDC (-1 to +1)
    // Input: pixel coordinates (0 to canvas_size)
    // Output: normalized device coordinates (-1 to +1)
    vec2 normalized_pos = vec2(
        (a_position.x / u_canvas_size.x) * 2.0 - 1.0,  // X: [0, width] -> [-1, 1]
        -((a_position.y / u_canvas_size.y) * 2.0 - 1.0)  // Y: [0, height] -> [1, -1] (flip Y)
    );
    
    v_position = normalized_pos;
    v_line_id = a_line_id;
    gl_Position = vec4(normalized_pos, 0.0, 1.0);
}