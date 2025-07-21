#version 410 core
flat in vec3 startPos;
in vec3 vertPos;
out vec4 fragColor;
uniform vec2 u_resolution;
uniform float u_dashSize;
uniform float u_gapSize;
void main()
{
    vec2 dir = (vertPos.xy - startPos.xy) * u_resolution / 2.0;
    float dist = length(dir);
    if (fract(dist / (u_dashSize + u_gapSize)) > u_dashSize / (u_dashSize + u_gapSize))
        discard;
    fragColor = vec4(1.0);
} 