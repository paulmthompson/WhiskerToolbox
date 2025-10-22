#version 430 core

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

in vec2 v_position[];
flat in uint v_line_id[];

out vec2 g_position;
out vec2 g_tex_coord;
flat out uint g_line_id;
flat out uint g_is_selected;  // Changed from bool to uint (0 = not selected, 1 = selected)

uniform float u_line_width;
uniform vec2 u_viewport_size;

// Selection mask buffer: each uint corresponds to a line ID (1-based)
layout(std430, binding = 3) readonly buffer SelectionMaskBuffer {
    uint selection_mask[];
};

// Visibility mask buffer: each uint corresponds to a line ID (1-based)
layout(std430, binding = 4) readonly buffer VisibilityMaskBuffer {
    uint visibility_mask[];
};

void main() {
    uint line_id = v_line_id[0];  // Both vertices should have the same line ID
    
    // Check if this line is visible (line_id is 1-based, array is 0-based)
    uint is_visible = 1u; // Default to visible
    if (line_id > 0u && line_id <= visibility_mask.length()) {
        is_visible = visibility_mask[line_id - 1u];
    }
    
    // Skip rendering if line is hidden
    if (is_visible == 0u) {
        return;
    }
    
    // Get the two vertices of the line segment
    vec2 p0 = v_position[0];
    vec2 p1 = v_position[1];
    
    // Calculate line direction and perpendicular
    vec2 line_dir = normalize(p1 - p0);
    vec2 perp = vec2(-line_dir.y, line_dir.x);
    
    // Calculate half-width in normalized device coordinates
    // Convert line width from pixels to NDC space
    float half_width_ndc = (u_line_width / u_viewport_size.x) * 2.0;
    
    // Generate quad vertices
    vec2 v0 = p0 - perp * half_width_ndc;
    vec2 v1 = p0 + perp * half_width_ndc;
    vec2 v2 = p1 - perp * half_width_ndc;
    vec2 v3 = p1 + perp * half_width_ndc;
    
    // Check if this line is selected (line_id is 1-based, array is 0-based)
    uint is_selected = 0u;
    if (line_id > 0u && line_id <= selection_mask.length()) {
        is_selected = selection_mask[line_id - 1u];
    }
    
    g_position = v0;
    g_tex_coord = vec2(0.0, 0.0);
    g_line_id = line_id;
    g_is_selected = is_selected;
    gl_Position = vec4(v0, 0.0, 1.0);
    EmitVertex();
    
    g_position = v1;
    g_tex_coord = vec2(0.0, 1.0);
    g_line_id = line_id;
    g_is_selected = is_selected;
    gl_Position = vec4(v1, 0.0, 1.0);
    EmitVertex();
    
    g_position = v2;
    g_tex_coord = vec2(1.0, 0.0);
    g_line_id = line_id;
    g_is_selected = is_selected;
    gl_Position = vec4(v2, 0.0, 1.0);
    EmitVertex();
    
    g_position = v3;
    g_tex_coord = vec2(1.0, 1.0);
    g_line_id = line_id;
    g_is_selected = is_selected;
    gl_Position = vec4(v3, 0.0, 1.0);
    EmitVertex();
    
    EndPrimitive();
}
