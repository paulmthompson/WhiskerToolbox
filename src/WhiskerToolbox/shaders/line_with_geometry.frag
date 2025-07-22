#version 410 core

in vec2 g_position;
in vec2 g_tex_coord;
in uint g_line_id;

uniform vec4 u_color;
uniform vec4 u_hover_color;
uniform vec4 u_selected_color;
uniform uint u_hover_line_id;
uniform bool u_is_hovered;
uniform bool u_is_selected;

out vec4 FragColor;

void main() {
    // Determine the color based on state
    vec4 final_color = u_color;

    if (u_is_hovered && g_line_id == u_hover_line_id) {
        final_color = u_hover_color;
    } else if (u_is_selected) {
        final_color = u_selected_color;
    }

    FragColor = final_color;
}