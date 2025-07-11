#ifndef WHISKERTOOLBOX_COLORED_VERTEX_SHADER_HPP
#define WHISKERTOOLBOX_COLORED_VERTEX_SHADER_HPP

static char const * vertexShaderSource =
        "#version 410 core\n"
        "in vec4 vertex;\n"
        "uniform vec3 u_color;\n"
        "uniform float u_alpha;\n"
        "out vec3 fragColor;\n"
        "out float fragAlpha;\n"
        "uniform mat4 projMatrix;\n"
        "uniform mat4 viewMatrix;\n"
        "uniform mat4 modelMatrix;\n"
        "void main() {\n"
        "   gl_Position = projMatrix * viewMatrix * modelMatrix * vertex;\n"
        "   fragColor = u_color;\n"
        "   fragAlpha = u_alpha;\n"
        "}\n";

static char const * fragmentShaderSource =
        "#version 410 core\n"
        "in vec3 fragColor;\n"
        "in float fragAlpha;\n"
        "out vec4 outColor;\n"
        "void main() {\n"
        "   outColor = vec4(fragColor, fragAlpha);\n"
        "}\n";


#endif//WHISKERTOOLBOX_COLORED_VERTEX_SHADER_HPP
