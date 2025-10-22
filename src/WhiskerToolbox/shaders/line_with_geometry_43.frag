#version 430 core

in vec2 g_position;
in vec2 g_tex_coord;
flat in uint g_line_id;
flat in uint g_is_selected;  // Changed from bool to uint

uniform vec4 u_color;
uniform vec4 u_hover_color;
uniform vec4 u_selected_color;
uniform uint u_hover_line_id;
uniform bool u_is_hovered;
uniform bool u_is_selected;  // Legacy - now using g_is_selected from geometry shader

out vec4 FragColor;

void main() {
    // Use normal color for regular rendering
    vec4 final_color = u_color;
    
    // Check if this specific line is selected (using per-vertex data from geometry shader)
    if (g_is_selected != 0u) {
        final_color = u_selected_color;
    }

    // Check if this specific line should be highlighted (hover overrides selection)
    if (u_hover_line_id > 0u && g_line_id == u_hover_line_id) {
        final_color = u_hover_color;
    }

    FragColor = final_color;
}
