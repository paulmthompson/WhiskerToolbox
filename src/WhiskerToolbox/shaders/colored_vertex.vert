#version 410 core
in vec4 vertex;
uniform vec3 u_color;
uniform float u_alpha;
out vec3 fragColor;
out float fragAlpha;
uniform mat4 projMatrix;
uniform mat4 viewMatrix;
uniform mat4 modelMatrix;
void main() {
   gl_Position = projMatrix * viewMatrix * modelMatrix * vertex;
   fragColor = u_color;
   fragAlpha = u_alpha;
} 