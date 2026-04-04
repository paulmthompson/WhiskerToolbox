#ifndef PLOTTINGSVG_RENDERERS_SVGPOLYLINERENDERER_HPP
#define PLOTTINGSVG_RENDERERS_SVGPOLYLINERENDERER_HPP

/**
 * @file SVGPolyLineRenderer.hpp
 * @brief RenderablePolyLineBatch to SVG `<polyline>` elements.
 */

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <glm/mat4x4.hpp>

#include <string>
#include <vector>

namespace PlottingSVG {

/**
 * @brief Stateless conversion from `CorePlotting::RenderablePolyLineBatch` to SVG `<polyline>` strings.
 *
 * Applies `projection * view * batch.model_matrix`, maps each vertex with `transformVertexToSVG`, and
 * emits one fragment per logical line (subject to `vertex_count` and buffer bounds). Stroke color uses
 * `colorToSVGHex` (RGB only; alpha is not written to SVG attributes).
 */
class SVGPolyLineRenderer {
public:
    /**
     * @brief Convert one polyline batch to SVG fragments in canvas pixel space.
     *
     * @param batch           Flat `vertices` as `x,y` pairs in world space; `line_start_indices` and
     *                        `line_vertex_counts` describe each polyline within that buffer.
     * @param view            Shared view matrix (same convention as OpenGL rendering).
     * @param projection      Shared projection matrix.
     * @param canvas_width    Target SVG width in pixels.
     * @param canvas_height   Target SVG height in pixels.
     *
     * @pre `batch.line_start_indices.size()` must equal `batch.line_vertex_counts.size()`. The
     *      implementation indexes both by `line_idx` without a length guard (enforcement: none)
     *      [CRITICAL]
     * @pre For each line `i`, `batch.line_start_indices[i] >= 0`, and the vertex pairs
     *      `[start_index + k]` for `k in [0, vertex_count)` must lie within `batch.vertices` when
     *      `vertex_count >= 2` (each vertex uses two floats). If the buffer is too short, the inner
     *      loop stops early and a `<polyline>` may be emitted with fewer than `vertex_count` points, or
     *      with an empty `points` attribute (enforcement: none) [IMPORTANT]
     * @pre `canvas_width > 0` and `canvas_height > 0` if output should match the same NDC→pixel mapping
     *      as live OpenGL rendering (enforcement: none) [IMPORTANT]
     * @pre If `batch.colors` is non-empty, only indices `line_idx < batch.colors.size()` read a
     *      per-line color; otherwise `batch.global_color` is used (enforcement: none) [LOW]
     *
     * @post Returns an empty vector when `batch.vertices` is empty or `batch.line_start_indices` is empty.
     * @post Otherwise emits at most one `<polyline fill="none" stroke="…" …/>` string per line index;
     *       lines with `line_vertex_counts[line_idx] < 2` are skipped (no string for that line).
     * @post Stroke uses `batch.thickness`, `stroke-linejoin="round"`, and `stroke-linecap="round"`.
     * @post Does not throw under normal `std::string`/`ostringstream` behavior.
     */
    [[nodiscard]] static std::vector<std::string>
    render(CorePlotting::RenderablePolyLineBatch const & batch,
           glm::mat4 const & view,
           glm::mat4 const & projection,
           int canvas_width,
           int canvas_height);
};

} // namespace PlottingSVG

#endif // PLOTTINGSVG_RENDERERS_SVGPOLYLINERENDERER_HPP
