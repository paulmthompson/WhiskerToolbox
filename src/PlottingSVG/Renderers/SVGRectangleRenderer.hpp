#ifndef PLOTTINGSVG_RENDERERS_SVGRECTANGLERENDERER_HPP
#define PLOTTINGSVG_RENDERERS_SVGRECTANGLERENDERER_HPP

/**
 * @file SVGRectangleRenderer.hpp
 * @brief RenderableRectangleBatch to SVG `<rect>` elements.
 */

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <glm/mat4x4.hpp>

#include <string>
#include <vector>

namespace PlottingSVG {

/**
 * @brief Stateless conversion from `CorePlotting::RenderableRectangleBatch` to SVG `<rect>` strings.
 *
 * Each `bounds[i]` is `(x, y, width, height)` in world space (same packing as the GL path). Corners are
 * transformed with `transformVertexToSVG`; SVG `x`/`y`/`width`/`height` use `min` and `abs` so negative
 * `width` or `height` still yield a non-negative axis-aligned box. `entity_ids` and `selection_flags` on
 * the batch are not consulted here (no stroke or highlight pass in SVG).
 */
class SVGRectangleRenderer {
public:
    /**
     * @brief Convert one rectangle batch to SVG fragments in canvas pixel space.
     *
     * @param batch           Per-instance `bounds` as `glm::vec4(x, y, width, height)` in world space;
     *                        optional parallel `colors` (RGBA per rectangle when `colors.size() > i`).
     * @param view            Shared view matrix (same convention as OpenGL rendering).
     * @param projection      Shared projection matrix.
     * @param canvas_width    Target SVG width in pixels.
     * @param canvas_height   Target SVG height in pixels.
     *
     * @pre `canvas_width > 0` and `canvas_height > 0` if output should match the same NDC→pixel mapping
     *      as live OpenGL rendering (enforcement: none) [IMPORTANT]
     * @pre `batch.bounds[i]` components should be finite for meaningful pixel geometry (enforcement: none)
     *      [LOW]
     * @pre If `batch.colors` is non-empty, only indices `i < batch.colors.size()` read a per-rect color;
     *      otherwise each rectangle uses RGB white with `fill-opacity` `0.5f` as the implementation default
     *      (enforcement: none) [LOW]
     *
     * @post Returns an empty vector when `batch.bounds` is empty.
     * @post Otherwise returns exactly one `<rect fill="…" fill-opacity="…" stroke="none"/>` string per
     *       bounds entry, in order.
     * @post Fill RGB comes from `colorToSVGHex`; alpha is written as `fill-opacity` (unlike stroke-only
     *       paths that sometimes omit alpha).
     * @post Does not throw under normal `std::string`/`ostringstream` behavior.
     */
    [[nodiscard]] static std::vector<std::string>
    render(CorePlotting::RenderableRectangleBatch const & batch,
           glm::mat4 const & view,
           glm::mat4 const & projection,
           int canvas_width,
           int canvas_height);
};

}// namespace PlottingSVG

#endif// PLOTTINGSVG_RENDERERS_SVGRECTANGLERENDERER_HPP
