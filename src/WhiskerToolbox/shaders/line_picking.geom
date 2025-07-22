#version 410 core

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

flat in uint v_line_id[];

flat out uint g_line_id;

uniform float u_line_width;
uniform vec2 u_viewport_size;

void main() {
    // Get the two vertices of the line segment
    vec2 p0 = gl_in[0].gl_Position.xy;
    vec2 p1 = gl_in[1].gl_Position.xy;

    // Calculate line direction and perpendicular
    vec2 line_dir = normalize(p1 - p0);
    vec2 perp = vec2(-line_dir.y, line_dir.x);

    // Calculate half-width in normalized device coordinates
    float half_width_ndc = (u_line_width / u_viewport_size.x) * 2.0;

    // Generate quad vertices
    vec2 v0 = p0 - perp * half_width_ndc;
    vec2 v1 = p0 + perp * half_width_ndc;
    vec2 v2 = p1 - perp * half_width_ndc;
    vec2 v3 = p1 + perp * half_width_ndc;

    // Emit vertices
    g_line_id = v_line_id[0];
    gl_Position = vec4(v0, 0.0, 1.0);
    EmitVertex();

    g_line_id = v_line_id[0];
    gl_Position = vec4(v1, 0.0, 1.0);
    EmitVertex();

    g_line_id = v_line_id[1];
    gl_Position = vec4(v2, 0.0, 1.0);
    EmitVertex();

    g_line_id = v_line_id[1];
    gl_Position = vec4(v3, 0.0, 1.0);
    EmitVertex();

    EndPrimitive();
}