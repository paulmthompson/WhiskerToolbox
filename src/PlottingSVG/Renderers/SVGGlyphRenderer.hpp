#ifndef PLOTTINGSVG_RENDERERS_SVGGLYPHRENDERER_HPP
#define PLOTTINGSVG_RENDERERS_SVGGLYPHRENDERER_HPP

/**
 * @file SVGGlyphRenderer.hpp
 * @brief RenderableGlyphBatch to SVG primitives (tick, circle, square, cross).
 */

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <glm/mat4x4.hpp>

#include <string>
#include <vector>

namespace PlottingSVG {

/**
 * @brief Stateless conversion from `CorePlotting::RenderableGlyphBatch` to SVG element strings.
 *
 * Applies `projection * view * batch.model_matrix` per batch, maps vertices with
 * `PlottingSVG::transformVertexToSVG`, and emits one or two fragments per glyph depending on
 * `GlyphType` (cross uses two `<line>` elements).
 */
class SVGGlyphRenderer {
public:
    /**
     * @brief Convert one glyph batch to SVG fragments in canvas pixel space.
     *
     * @param batch           Glyph instances: `positions` in world space; optional parallel `colors`
     *                        (per-glyph RGBA when `colors.size() > i`, otherwise white).
     * @param view            Shared view matrix (same convention as OpenGL rendering).
     * @param projection      Shared projection matrix.
     * @param canvas_width    Target SVG width in pixels.
     * @param canvas_height   Target SVG height in pixels.
     *
     * @pre `batch.glyph_type` must be `Tick`, `Circle`, `Square`, or `Cross`. Other values are not
     *      handled by the `switch` and produce an empty fragment for that glyph (enforcement: none)
     *      [IMPORTANT]
     * @pre `canvas_width > 0` and `canvas_height > 0` if output should match the same NDC→pixel mapping
     *      as live OpenGL rendering (enforcement: none) [IMPORTANT]
     * @pre `batch.positions[i]` and the MVP should yield finite NDC after transform for on-canvas
     *      coordinates (enforcement: none) [LOW]
     * @pre If `batch.colors` is non-empty, only indices `i < batch.colors.size()` read per-glyph color;
     *      shorter `colors` is allowed (remaining glyphs use opaque white) (enforcement: none) [LOW]
     *
     * @post Returns an empty vector when `batch.positions` is empty.
     * @post Otherwise returns one `<line>`, `<circle>`, or `<rect>` string per glyph for `Tick`,
     *       `Circle`, and `Square`, and **two** `<line>` strings per glyph for `Cross` (so the vector
     *       length can exceed `batch.positions.size()`).
     * @post Output strings use `colorToSVGHex` for stroke/fill color and separate opacity attributes
     *       where applicable.
     * @post Does not throw under normal `std::string`/`ostringstream` behavior.
     */
    [[nodiscard]] static std::vector<std::string>
    render(CorePlotting::RenderableGlyphBatch const & batch,
           glm::mat4 const & view,
           glm::mat4 const & projection,
           int canvas_width,
           int canvas_height);
};

}// namespace PlottingSVG

#endif// PLOTTINGSVG_RENDERERS_SVGGLYPHRENDERER_HPP
