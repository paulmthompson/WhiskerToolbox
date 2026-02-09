#ifndef PLOTS_COMMON_LINE_SELECTION_HELPERS_HPP
#define PLOTS_COMMON_LINE_SELECTION_HELPERS_HPP

/**
 * @file LineSelectionHelpers.hpp
 * @brief Shared helpers for line-drag selection of batch lines
 *
 * Used by LinePlotOpenGLWidget and TemporalProjectionOpenGLWidget to build
 * the selection preview glyph and to run the line-vs-batch intersection
 * query. Widget-specific logic (e.g. mapping hit indices to trial indices
 * vs entity IDs) remains in each widget.
 *
 * Depends only on CorePlotting and glm; no PlottingOpenGL or Qt beyond QPoint.
 */

#include "CorePlotting/Interaction/GlyphPreview.hpp"
#include "CorePlotting/LineBatch/ILineBatchIntersector.hpp"

#include <QPoint>

#include <glm/glm.hpp>

#include <vector>

namespace WhiskerToolbox::Plots {

/**
 * @brief Build a line-selection GlyphPreview for the selection rubber-band.
 *
 * PreviewRenderer expects canvas pixel coordinates (top-left origin).
 * White stroke for normal selection, red for remove mode.
 *
 * @param start_screen Selection line start in widget pixel coordinates.
 * @param end_screen   Selection line end in widget pixel coordinates.
 * @param remove_mode  If true, use red stroke; otherwise white.
 * @return GlyphPreview with Type::Line and stroke styling.
 */
[[nodiscard]] inline CorePlotting::Interaction::GlyphPreview
buildLineSelectionPreview(QPoint const & start_screen,
                         QPoint const & end_screen,
                         bool remove_mode)
{
    CorePlotting::Interaction::GlyphPreview preview;
    preview.type = CorePlotting::Interaction::GlyphPreview::Type::Line;
    preview.line_start = glm::vec2{static_cast<float>(start_screen.x()),
                                   static_cast<float>(start_screen.y())};
    preview.line_end =
        glm::vec2{static_cast<float>(end_screen.x()),
                  static_cast<float>(end_screen.y())};
    if (remove_mode) {
        preview.stroke_color = glm::vec4{1.0f, 0.3f, 0.3f, 0.9f};
    } else {
        preview.stroke_color = glm::vec4{1.0f, 1.0f, 1.0f, 0.9f};
    }
    preview.stroke_width = 2.0f;
    return preview;
}

/**
 * @brief Run line-vs-batch intersection and return hit line indices.
 *
 * Builds a LineIntersectionQuery from the selection segment (NDC) and
 * current projection/view matrices, then calls the intersector. The caller
 * applies the result to its own selection state (e.g. trial indices or
 * entity IDs).
 *
 * @param intersector       The intersector (CPU or GPU backend).
 * @param cpu_data          Current line batch CPU data from the store.
 * @param start_ndc         Selection line start in NDC [-1, 1].
 * @param end_ndc           Selection line end in NDC [-1, 1].
 * @param projection_matrix Current orthographic projection matrix.
 * @param view_matrix       Current view matrix (typically identity).
 * @return Indices of lines that intersect the selection segment (0-based
 *         into cpu_data.lines). Empty if no intersector or batch empty.
 */
[[nodiscard]] inline std::vector<CorePlotting::LineBatchIndex>
runLineSelectionIntersection(
    CorePlotting::ILineBatchIntersector & intersector,
    CorePlotting::LineBatchData const & cpu_data,
    glm::vec2 start_ndc,
    glm::vec2 end_ndc,
    glm::mat4 const & projection_matrix,
    glm::mat4 const & view_matrix)
{
    if (cpu_data.lines.empty()) {
        return {};
    }
    CorePlotting::LineIntersectionQuery query;
    query.start_ndc = start_ndc;
    query.end_ndc = end_ndc;
    query.tolerance = 0.02f;
    query.mvp = projection_matrix * view_matrix;
    CorePlotting::LineIntersectionResult result =
        intersector.intersect(cpu_data, query);
    return result.intersected_line_indices;
}

} // namespace WhiskerToolbox::Plots

#endif // PLOTS_COMMON_LINE_SELECTION_HELPERS_HPP
