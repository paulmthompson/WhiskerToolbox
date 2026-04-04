#ifndef PLOTTINGSVG_SVGUTILS_HPP
#define PLOTTINGSVG_SVGUTILS_HPP

/**
 * @file SVGUtils.hpp
 * @brief Shared SVG export helpers: MVP → canvas pixels and RGBA → `#RRGGBB`.
 */

#include <glm/glm.hpp>

#include <string>

namespace PlottingSVG {

/// @brief Transform a homogeneous vertex by MVP and map NDC to SVG pixel coordinates.
[[nodiscard]] glm::vec2 transformVertexToSVG(
        glm::vec4 const & vertex,
        glm::mat4 const & mvp,
        int canvas_width,
        int canvas_height);

/// @brief Map linear RGBA in [0, 1] to an uppercase SVG hex color (`#RRGGBB`).
[[nodiscard]] std::string colorToSVGHex(glm::vec4 const & color);

}// namespace PlottingSVG

#endif// PLOTTINGSVG_SVGUTILS_HPP
