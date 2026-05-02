#ifndef SVGEXPORTER_HPP
#define SVGEXPORTER_HPP

#include "CorePlotting/Layout/LayoutTransform.hpp"
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <QString>
#include <QStringList>
#include <glm/glm.hpp>

#include <memory>
#include <string>

class OpenGLWidget;
class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;
struct AnalogSeriesOptionsData;
struct DigitalEventSeriesOptionsData;
struct DigitalIntervalSeriesOptionsData;

namespace CorePlotting {
struct SeriesDataCache;
}

/**
 * @brief SVG export utility for DataViewer plots
 * 
 * Exports the current plot state to SVG format by building a RenderableScene
 * from the DataViewer series data and rendering it to SVG via PlottingSVG.
 * This ensures that the SVG output matches the on-screen visualization exactly,
 * as it uses the same batch building and coordinate transformation code.
 *
 * Architecture:
 * - Builds RenderableBatches from series data (same as OpenGL rendering)
 * - Uses PlottingSVG::buildSVGDocument() for SVG generation
 * - Shared View/Projection matrices ensure coordinate consistency
 * 
 * Features:
 * - Fixed canvas size (1920x1080) with viewBox for clean scaling
 * - Supports analog time series, digital events, and digital intervals
 * - Preserves colors, transparency, and visual styles
 * - Uses same coordinate transformations as OpenGL rendering
 */
class SVGExporter {
public:
    /**
     * @brief Construct SVG exporter with references to plot state
     * 
     * @param gl_widget OpenGL widget containing plot data and state (including view state parameters)
     */
    explicit SVGExporter(OpenGLWidget * gl_widget);

    /**
     * @brief Export current plot to SVG format
     * 
     * Builds a RenderableScene from the current plot state and converts
     * it to SVG using PlottingSVG export functions.
     * 
     * @return QString containing complete SVG document
     */
    QString exportToSVG();

    /**
     * @brief Set SVG canvas dimensions
     * 
     * @param width SVG canvas width in pixels
     * @param height SVG canvas height in pixels
     */
    void setCanvasSize(int width, int height) {
        svg_width_ = width;
        svg_height_ = height;
    }

    /**
     * @brief Get current SVG canvas width
     */
    [[nodiscard]] int getCanvasWidth() const { return svg_width_; }

    /**
     * @brief Get current SVG canvas height
     */
    [[nodiscard]] int getCanvasHeight() const { return svg_height_; }

    /**
     * @brief Enable or disable scalebar in SVG output
     * 
     * @param enabled Whether to include a scalebar
     * @param length Length of the scalebar in time units
     */
    void enableScalebar(bool enabled, int length) {
        scalebar_enabled_ = enabled;
        scalebar_length_ = length;
    }

private:
    /**
     * @brief Build a complete RenderableScene from current plot state.
     *
     * Iterates over all visible series and builds the appropriate batch types:
     * - Analog series → RenderablePolyLineBatch
     * - Digital events → RenderableGlyphBatch
     * - Digital intervals → RenderableRectangleBatch
     *
     * @param master_window_start Inclusive visible window start as a @c TimeFrameIndex into the
     *        DataViewer master @c TimeFrame (same convention as @c ViewStateData::x_min).
     * @param master_window_end Inclusive visible window end as a @c TimeFrameIndex into the
     *        master @c TimeFrame (same convention as @c ViewStateData::x_max).
     * @return Complete scene ready for SVG rendering
     */
    CorePlotting::RenderableScene buildScene(
            TimeFrameIndex master_window_start,
            TimeFrameIndex master_window_end) const;

    /**
     * @brief Build a RenderablePolyLineBatch from an analog time series.
     *
     * @param master_window_start Inclusive query range start (master @c TimeFrame indices).
     * @param master_window_end Inclusive query range end (master @c TimeFrame indices).
     */
    CorePlotting::RenderablePolyLineBatch buildAnalogBatch(
            std::shared_ptr<AnalogTimeSeries> const & series,
            CorePlotting::LayoutTransform const & layout_transform,
            CorePlotting::SeriesDataCache const & data_cache,
            AnalogSeriesOptionsData const & options,
            TimeFrameIndex master_window_start,
            TimeFrameIndex master_window_end) const;

    /**
     * @brief Build a RenderableGlyphBatch from a digital event series.
     *
     * @param master_window_start Inclusive query range start (master @c TimeFrame indices).
     * @param master_window_end Inclusive query range end (master @c TimeFrame indices).
     */
    CorePlotting::RenderableGlyphBatch buildEventBatch(
            std::shared_ptr<DigitalEventSeries> const & series,
            CorePlotting::LayoutTransform const & layout_transform,
            DigitalEventSeriesOptionsData const & options,
            TimeFrameIndex master_window_start,
            TimeFrameIndex master_window_end) const;

    /**
     * @brief Build event markers as filled rectangles (box glyph) for SVG export.
     */
    CorePlotting::RenderableRectangleBatch buildEventBoxBatch(
            std::shared_ptr<DigitalEventSeries> const & series,
            CorePlotting::LayoutTransform const & layout_transform,
            DigitalEventSeriesOptionsData const & options,
            TimeFrameIndex master_window_start,
            TimeFrameIndex master_window_end) const;

    /**
     * @brief Build a RenderableRectangleBatch from a digital interval series.
     *
     * @param series The digital interval series data
     * @param layout_transform The layout transform for the series
     * @param options The options for the series
     * @param master_window_start Inclusive query range start (master @c TimeFrame indices).
     * @param master_window_end Inclusive query range end (master @c TimeFrame indices).
     * @return A RenderableRectangleBatch for the series
     */
    CorePlotting::RenderableRectangleBatch buildIntervalBatch(
            std::shared_ptr<DigitalIntervalSeries> const & series,
            CorePlotting::LayoutTransform const & layout_transform,
            DigitalIntervalSeriesOptionsData const & options,
            TimeFrameIndex master_window_start,
            TimeFrameIndex master_window_end) const;

    OpenGLWidget * gl_widget_;

    // SVG canvas dimensions
    int svg_width_{1920};
    int svg_height_{1080};

    // Scalebar configuration
    bool scalebar_enabled_{false};
    int scalebar_length_{100};
};

#endif// SVGEXPORTER_HPP
