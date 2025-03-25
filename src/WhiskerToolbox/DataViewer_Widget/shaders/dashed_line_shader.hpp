#ifndef WHISKERTOOLBOX_DASHED_LINE_SHADER_HPP
#define WHISKERTOOLBOX_DASHED_LINE_SHADER_HPP

static char const * dashedVertexShaderSource =
        "#version 410 core\n"
        "layout (location = 0) in vec3 inPos;\n"
        "flat out vec3 startPos;\n"
        "out vec3 vertPos;\n"
        "uniform mat4 u_mvp;\n"
        "void main()\n"
        "{\n"
        "    vec4 pos = u_mvp * vec4(inPos, 1.0);\n"
        "    gl_Position = pos;\n"
        "    vertPos = pos.xyz / pos.w;\n"
        "    startPos = vertPos;\n"
        "}\n";

static char const * dashedFragmentShaderSource =
        "#version 410 core\n"
        "flat in vec3 startPos;\n"
        "in vec3 vertPos;\n"
        "out vec4 fragColor;\n"
        "uniform vec2 u_resolution;\n"
        "uniform float u_dashSize;\n"
        "uniform float u_gapSize;\n"
        "void main()\n"
        "{\n"
        "    vec2 dir = (vertPos.xy - startPos.xy) * u_resolution / 2.0;\n"
        "    float dist = length(dir);\n"
        "    if (fract(dist / (u_dashSize + u_gapSize)) > u_dashSize / (u_dashSize + u_gapSize))\n"
        "        discard;\n"
        "    fragColor = vec4(1.0);\n"
        "}\n";


#endif//WHISKERTOOLBOX_DASHED_LINE_SHADER_HPP
