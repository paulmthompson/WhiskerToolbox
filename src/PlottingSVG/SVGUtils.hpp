#ifndef PLOTTINGSVG_SVGUTILS_HPP
#define PLOTTINGSVG_SVGUTILS_HPP

/**
 * @file SVGUtils.hpp
 * @brief Shared SVG export helpers: MVP → canvas pixels and RGBA → `#RRGGBB`.
 */

#include <glm/glm.hpp>

#include <string>

namespace PlottingSVG {

/**
 * @brief Apply `mvp` to a homogeneous world-space vertex and map the result to SVG pixel coordinates.
 *
 * Performs `ndc = mvp * vertex`, then a perspective divide when `std::abs(ndc.w) > 1e-6f` (if `w` is
 * smaller in magnitude, `x`/`y` are **not** divided). Maps NDC `x` from `[-1, 1]` to `[0, canvas_width]`
 * and flips `y` so NDC `+1` is the top of the canvas (`svg_y = height * (1 - ndc.y) / 2`).
 *
 * @param vertex        Typically `(x, y, 0, 1)` in the same space as `mvp`.
 * @param mvp           Model–view–projection matrix (column-major, GL convention).
 * @param canvas_width  Target width in pixels.
 * @param canvas_height Target height in pixels.
 *
 * @pre `canvas_width > 0` and `canvas_height > 0` if the mapping should match the usual OpenGL viewport
 *      convention (enforcement: none) [IMPORTANT]
 * @pre For finite pixel output, `mvp * vertex` and the post-divide NDC values should be finite; near-zero
 *      `w` skips the divide and can amplify coordinates (enforcement: none) [LOW]
 *
 * @post Returns `{svg_x, svg_y}` with components computed from the formulas above (points may lie outside
 *       the `[0, width] × [0, height]` range when NDC is outside `[-1, 1]`).
 * @post Does not throw.
 */
[[nodiscard]] glm::vec2 transformVertexToSVG(
        glm::vec4 const & vertex,
        glm::mat4 const & mvp,
        int canvas_width,
        int canvas_height);

/**
 * @brief Convert linear RGB channels to an uppercase `#RRGGBB` string for SVG `fill` / `stroke`.
 *
 * Each channel is `clamp(channel * 255, 0, 255)`; values outside `[0, 1]` are saturated. The alpha
 * component is **not** encoded (callers must emit `fill-opacity` / `stroke-opacity` separately if needed).
 *
 * @pre For predictable channel bytes, `color.r`, `color.g`, and `color.b` should be finite (enforcement:
 *      none) [LOW]
 *
 * @post Returns exactly seven characters: `'#'` plus six uppercase hexadecimal digits.
 * @post Does not throw under normal `std::ostringstream` behavior.
 */
[[nodiscard]] std::string colorToSVGHex(glm::vec4 const & color);

}// namespace PlottingSVG

#endif// PLOTTINGSVG_SVGUTILS_HPP
