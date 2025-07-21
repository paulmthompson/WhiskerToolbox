#version 410 core
in vec3 fragColor;
in float fragAlpha;
out vec4 outColor;
void main() {
   outColor = vec4(fragColor, fragAlpha);
} 