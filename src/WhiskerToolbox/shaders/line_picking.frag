#version 410 core

in uint g_line_id;

out vec4 FragColor;

void main() {
    // Output line ID as a color for picking
    // Convert uint to vec4 for framebuffer output
    uint id = g_line_id;
    float r = float((id >> 16) & 0xFF) / 255.0;
    float g = float((id >> 8) & 0xFF) / 255.0;
    float b = float(id & 0xFF) / 255.0;
    float a = 1.0;

    FragColor = vec4(r, g, b, a);
}