#ifndef COREPLOTTING_MAPPERS_HISTOGRAMMAPPER_HPP
#define COREPLOTTING_MAPPERS_HISTOGRAMMAPPER_HPP

/**
 * @file HistogramMapper.hpp
 * @brief Maps HistogramData into renderable scene batches
 *
 * Provides conversion from HistogramData to either:
 *  - RenderableRectangleBatch (bar mode)
 *  - RenderablePolyLineBatch  (line mode)
 *
 * This is the shared plotting infrastructure used by PSTHWidget,
 * ACFWidget, and any future histogram-style plots.
 */

#include "CorePlotting/DataTypes/HistogramData.hpp"
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <glm/glm.hpp>

namespace CorePlotting {

/**
 * @brief Style options for histogram rendering
 */
struct HistogramStyle {
    glm::vec4 fill_color{0.3f, 0.6f, 1.0f, 0.8f};   ///< Bar fill / line color
    glm::vec4 line_color{0.4f, 0.7f, 1.0f, 1.0f};    ///< Line color (line mode)
    float line_thickness{2.0f};                         ///< Line thickness (line mode)
    float bar_gap_fraction{0.05f};                      ///< Gap between bars as fraction of bin_width (0 = touching)
};

/**
 * @brief Converts HistogramData into renderable primitives
 *
 * Usage:
 * @code
 *   HistogramData hist = computeHistogram(...);
 *   HistogramStyle style;
 *   style.fill_color = {0.2f, 0.6f, 1.0f, 0.8f};
 *
 *   // Bar mode:
 *   auto rect_batch = HistogramMapper::toBars(hist, style);
 *   scene.rectangle_batches.push_back(std::move(rect_batch));
 *
 *   // Line mode:
 *   auto line_batch = HistogramMapper::toLine(hist, style);
 *   scene.poly_line_batches.push_back(std::move(line_batch));
 * @endcode
 */
class HistogramMapper {
public:
    /**
     * @brief Convert histogram bins to a rectangle batch (bar chart)
     *
     * Each bin becomes a rectangle with:
     *   x      = bin left edge (+ gap)
     *   y      = 0
     *   width  = bin_width (- 2*gap)
     *   height = bin count
     *
     * @param data     Histogram bin data
     * @param style    Visual style
     * @return Rectangle batch ready for SceneRenderer
     */
    [[nodiscard]] static RenderableRectangleBatch toBars(
        HistogramData const & data,
        HistogramStyle const & style = {});

    /**
     * @brief Convert histogram bins to a polyline batch (line chart)
     *
     * Generates a step-style polyline that traces the histogram outline:
     * For each bin: left-edge → right-edge at the bin height,
     * with vertical segments connecting adjacent bins.
     *
     * @param data     Histogram bin data
     * @param style    Visual style
     * @return Polyline batch ready for SceneRenderer
     */
    [[nodiscard]] static RenderablePolyLineBatch toLine(
        HistogramData const & data,
        HistogramStyle const & style = {});

    /**
     * @brief Build a complete RenderableScene from histogram data
     *
     * Convenience that creates the batch (bar or line), sets up
     * view/projection as identity (caller will override), and
     * returns a ready-to-upload scene.
     *
     * @param data  Histogram bin data
     * @param mode  Bar or Line display mode
     * @param style Visual style
     * @return Complete scene
     */
    [[nodiscard]] static RenderableScene buildScene(
        HistogramData const & data,
        HistogramDisplayMode mode,
        HistogramStyle const & style = {});
};

} // namespace CorePlotting

#endif // COREPLOTTING_MAPPERS_HISTOGRAMMAPPER_HPP
