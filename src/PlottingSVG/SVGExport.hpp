#ifndef PLOTTINGSVG_SVGEXPORT_HPP
#define PLOTTINGSVG_SVGEXPORT_HPP

/**
 * @file SVGExport.hpp
 * @brief Legacy flat SVG document assembly for DataViewer-style export (scene + scalebar).
 */

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <string>
#include <vector>

namespace PlottingSVG {

/**
 * @brief Canvas size and background fill for legacy DataViewer SVG export.
 *
 * `background_color` is inserted verbatim into a double-quoted `fill` attribute; callers must supply a
 * safe SVG paint token (see `buildSVGDocument` @pre).
 */
struct SVGExportParams {
    int canvas_width{1920};
    int canvas_height{1080};
    std::string background_color{"#1E1E1E"};
};

/**
 * @brief Visible time window used to map “full canvas width” ↔ elapsed time for the scalebar.
 *
 * Passed to `SVGScalebar`; bar pixel length is `scalebar_length * (canvas_width / (end - start))` when
 * `render()` emits geometry.
 */
struct ScalebarTimeRange {
    float start{};
    float end{};
};

/**
 * @brief Flatten `scene` batches to SVG element strings in fixed draw order.
 *
 * Order: all `rectangle_batches`, then all `poly_line_batches`, then all `glyph_batches`. Uses
 * `scene.view_matrix` and `scene.projection_matrix` for every batch. `spatial_index`, entity maps,
 * selection, and preview fields are not read.
 *
 * @pre `params.canvas_width > 0` and `params.canvas_height > 0` if coordinates should match OpenGL-style
 *      NDC→pixel mapping in the sub-renderers (enforcement: none) [IMPORTANT]
 * @pre Batch contents satisfy the preconditions of `SVGRectangleRenderer::render`,
 *      `SVGPolyLineRenderer::render`, and `SVGGlyphRenderer::render` respectively (enforcement: none)
 *      [LOW]
 *
 * @post Returns the concatenation of per-batch renderer outputs in the order above (each batch may
 *       contribute zero or more strings).
 * @post Does not throw under normal allocator / string behavior.
 */
[[nodiscard]] std::vector<std::string> renderSceneToSVG(
        CorePlotting::RenderableScene const & scene,
        SVGExportParams const & params);

/**
 * @brief Build a full SVG 1.1 document: XML declaration, root `<svg>`, fixed `<desc>`, background
 *        `<rect>`, then scene elements.
 *
 * Implementation is string-based (not `SVGDocument`); fragment strings are not XML-escaped.
 *
 * @pre `params.canvas_width` and `params.canvas_height` should be positive for a conventional canvas
 *      (enforcement: none) [IMPORTANT]
 * @pre `params.background_color` must be safe inside `fill="…"` (no `"`, `&`, `<`, etc.)
 *      (enforcement: none) [IMPORTANT]
 *
 * @post Returns a document starting with `<?xml version="1.0" encoding="UTF-8" standalone="no"?>`, a
 *       root `<svg>` with `viewBox="0 0 width height"`, a constant `<desc>WhiskerToolbox DataViewer
 *       Export</desc>`, a full-viewport background `<rect>`, then each string from `renderSceneToSVG`
 *       on its own line with leading indentation, and a closing `</svg>`.
 * @post Does not throw under normal stream behavior.
 */
[[nodiscard]] std::string buildSVGDocument(
        CorePlotting::RenderableScene const & scene,
        SVGExportParams const & params);

/**
 * @brief Bottom-right scalebar (default `SVGScalebar` corner): horizontal bar, end ticks, numeric label.
 *
 * Delegates to `SVGScalebar(scalebar_length, time_range.start, time_range.end)` and `render(canvas)`.
 *
 * @pre For non-empty output, `time_range.end > time_range.start` with a finite positive span, and
 *      `params.canvas_width > 0`, `params.canvas_height > 0` (matches `SVGScalebar::render` early exit)
 *      (enforcement: none) [IMPORTANT]
 * @pre `scalebar_length` non-zero if the bar should have non-zero extent in typical ranges
 *      (enforcement: none) [LOW]
 *
 * @post Returns an empty vector when `SVGScalebar::render` would return empty; otherwise the same four
 *       SVG fragments as that `render` (bar, ticks, label) for the default decoration configuration.
 * @post Does not throw under normal behavior of the callee.
 */
[[nodiscard]] std::vector<std::string> createScalebarSVG(
        int scalebar_length,
        ScalebarTimeRange time_range,
        SVGExportParams const & params);

}// namespace PlottingSVG

#endif// PLOTTINGSVG_SVGEXPORT_HPP
