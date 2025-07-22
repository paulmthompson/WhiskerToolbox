#version 410 core

uniform vec4 u_color;

out vec4 FragColor;

void main() {
    // Create circular points
    vec2 coord = gl_PointCoord - vec2(0.5, 0.5);
    float distance = length(coord);

    // Discard fragments outside the circle
    if (distance > 0.5) {
        discard;
    }

    // Smooth anti-aliased edge
    float alpha = 1.0 - smoothstep(0.4, 0.5, distance);
    FragColor = vec4(u_color.rgb, u_color.a * alpha);
}