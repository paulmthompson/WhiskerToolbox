/**
 * @file SVGUtils.cpp
 * @brief Implementations for PlottingSVG coordinate and color helpers.
 */

#include "PlottingSVG/SVGUtils.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace PlottingSVG {

glm::vec2 transformVertexToSVG(
        glm::vec4 const & vertex,
        glm::mat4 const & mvp,
        int canvas_width,// NOLINT(bugprone-easily-swappable-parameters)
        int canvas_height) {
    glm::vec4 ndc = mvp * vertex;
    if (std::abs(ndc.w) > 1e-6f) {
        ndc /= ndc.w;
    }
    float const svg_x = static_cast<float>(canvas_width) * (ndc.x + 1.0f) / 2.0f;
    float const svg_y = static_cast<float>(canvas_height) * (1.0f - ndc.y) / 2.0f;
    return {svg_x, svg_y};
}

std::string colorToSVGHex(glm::vec4 const & color) {
    int const r = static_cast<int>(std::clamp(color.r * 255.0f, 0.0f, 255.0f));
    int const g = static_cast<int>(std::clamp(color.g * 255.0f, 0.0f, 255.0f));
    int const b = static_cast<int>(std::clamp(color.b * 255.0f, 0.0f, 255.0f));

    std::ostringstream ss;
    ss << '#' << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << r
       << std::setw(2) << g << std::setw(2) << b;
    return ss.str();
}

}// namespace PlottingSVG
