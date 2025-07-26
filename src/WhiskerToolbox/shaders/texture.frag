#version 410 core

in vec2 v_texCoord;
uniform sampler2D u_texture;
uniform vec4 u_color;

out vec4 FragColor;

void main() {
    float intensity = texture(u_texture, v_texCoord).r;

    // Only render pixels where masks exist
    if (intensity <= 0.0) {
        discard;
    }

    // Proper density visualization with premultiplied alpha output
    vec3 final_color = u_color.rgb;  // Keep color pure (red for masks)
    float final_alpha = u_color.a * intensity;  // Density affects transparency

    // Output premultiplied alpha to work correctly with GL_ONE, GL_ONE_MINUS_SRC_ALPHA
    FragColor = vec4(final_color * final_alpha, final_alpha);
} 