#version 410 core

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

uniform float u_line_width;
uniform vec2 u_viewport_size;

void main() {
    vec2 p0 = gl_in[0].gl_Position.xy;
    vec2 p1 = gl_in[1].gl_Position.xy;

    // Convert NDC to screen space (pixels)
    vec2 sp0 = p0 * u_viewport_size * 0.5;
    vec2 sp1 = p1 * u_viewport_size * 0.5;

    // Direction and perpendicular in screen space
    vec2 dir = sp1 - sp0;
    float len = length(dir);
    if (len < 0.001) return;
    dir /= len;
    vec2 perp = vec2(-dir.y, dir.x);

    // Offset in screen pixels, then convert back to NDC
    vec2 offset_ndc = perp * u_line_width / u_viewport_size;

    // Emit quad as triangle strip
    gl_Position = vec4(p0 + offset_ndc, 0.0, 1.0);
    EmitVertex();

    gl_Position = vec4(p0 - offset_ndc, 0.0, 1.0);
    EmitVertex();

    gl_Position = vec4(p1 + offset_ndc, 0.0, 1.0);
    EmitVertex();

    gl_Position = vec4(p1 - offset_ndc, 0.0, 1.0);
    EmitVertex();

    EndPrimitive();
}
