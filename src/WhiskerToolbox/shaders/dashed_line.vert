#version 410 core
layout(location = 0) in vec3 inPos;
flat out vec3 startPos;
out vec3 vertPos;
uniform mat4 u_mvp;
void main() {
    vec4 pos = u_mvp * vec4(inPos, 1.0);
    gl_Position = pos;
    vertPos = pos.xyz / pos.w;
    startPos = vertPos;
}