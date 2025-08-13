#version 410 core

uniform vec4 u_color;
uniform vec4 u_group_colors[256]; // Array of group colors (max 256 groups)
uniform int u_num_groups;

// Input from vertex shader
flat in int v_group_id;

out vec4 FragColor;

void main() {
    // Create circular points
    vec2 coord = gl_PointCoord - vec2(0.5, 0.5);
    float distance = length(coord);

    // Discard fragments outside the circle
    if (distance > 0.5) {
        discard;
    }

    // Determine color based on group_id
    vec4 point_color;
    if (v_group_id < 0 || v_group_id >= u_num_groups) {
        // Use default color for ungrouped points or invalid group IDs
        point_color = u_color;
    } else {
        // Use group-specific color
        point_color = u_group_colors[v_group_id];
    }

    // Smooth anti-aliased edge
    float alpha = 1.0 - smoothstep(0.4, 0.5, distance);
    FragColor = vec4(point_color.rgb, point_color.a * alpha);
}