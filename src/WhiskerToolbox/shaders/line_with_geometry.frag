#version 410 core

in vec2 g_position;
in vec2 g_tex_coord;
flat in uint g_line_id;

uniform vec4 u_color;
uniform vec4 u_hover_color;
uniform vec4 u_selected_color;
uniform uint u_hover_line_id;
uniform bool u_is_hovered;
uniform bool u_is_selected;

out vec4 FragColor;

void main() {
    // Use normal color for regular rendering
    vec4 final_color = u_color;
    
    // Check if this specific line should be highlighted
    if (u_hover_line_id > 0u && g_line_id == u_hover_line_id) {
        final_color = vec4(1.0, 0.0, 1.0, 1.0); // Bright magenta for maximum visibility
    }

    FragColor = final_color;
}