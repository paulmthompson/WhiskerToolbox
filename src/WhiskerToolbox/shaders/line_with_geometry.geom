#version 410 core

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

in vec2 v_position[];
in uint v_line_id[];

out vec2 g_position;
out vec2 g_tex_coord;
out uint g_line_id;

uniform mat4 u_mvp_matrix;
uniform float u_line_width;
uniform vec2 u_viewport_size;

void main() {
    // Get the two vertices of the line segment
    vec2 p0 = v_position[0];
    vec2 p1 = v_position[1];

    // Calculate line direction and perpendicular
    vec2 line_dir = normalize(p1 - p0);
    vec2 perp = vec2(-line_dir.y, line_dir.x);

    // Calculate half-width in world coordinates
    float half_width = u_line_width / u_viewport_size.x;

    // Generate quad vertices
    vec2 v0 = p0 - perp * half_width;
    vec2 v1 = p0 + perp * half_width;
    vec2 v2 = p1 - perp * half_width;
    vec2 v3 = p1 + perp * half_width;

    // Emit vertices
    g_position = v0;
    g_tex_coord = vec2(0.0, 0.0);
    g_line_id = v_line_id[0];
    gl_Position = u_mvp_matrix * vec4(v0, 0.0, 1.0);
    EmitVertex();

    g_position = v1;
    g_tex_coord = vec2(0.0, 1.0);
    g_line_id = v_line_id[0];
    gl_Position = u_mvp_matrix * vec4(v1, 0.0, 1.0);
    EmitVertex();

    g_position = v2;
    g_tex_coord = vec2(1.0, 0.0);
    g_line_id = v_line_id[1];
    gl_Position = u_mvp_matrix * vec4(v2, 0.0, 1.0);
    EmitVertex();

    g_position = v3;
    g_tex_coord = vec2(1.0, 1.0);
    g_line_id = v_line_id[1];
    gl_Position = u_mvp_matrix * vec4(v3, 0.0, 1.0);
    EmitVertex();

    EndPrimitive();
}