#ifndef SVGEXPORTER_HPP
#define SVGEXPORTER_HPP

#include "CorePlotting/Export/SVGPrimitives.hpp"
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <QString>
#include <QStringList>
#include <glm/glm.hpp>

#include <memory>
#include <string>

class OpenGLWidget;
struct NewAnalogTimeSeriesDisplayOptions;
struct NewDigitalEventSeriesDisplayOptions;
struct NewDigitalIntervalSeriesDisplayOptions;
class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;
class TimeFrame;

/**
 * @brief SVG export utility for DataViewer plots
 * 
 * Exports the current plot state to SVG format by building a RenderableScene
 * from the DataViewer series data and rendering it to SVG using CorePlotting.
 * This ensures that the SVG output matches the on-screen visualization exactly,
 * as it uses the same batch building and coordinate transformation code.
 * 
 * Architecture (Phase 2 refactoring):
 * - Builds RenderableBatches from series data (same as OpenGL rendering)
 * - Uses CorePlotting::renderSceneToSVG() for SVG generation
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
     * it to SVG using CorePlotting export functions.
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
     * @param start_time Visible time range start
     * @param end_time Visible time range end
     * @return Complete scene ready for SVG rendering
     */
    CorePlotting::RenderableScene buildScene(int start_time, int end_time) const;

    /**
     * @brief Build a RenderablePolyLineBatch from an analog time series.
     */
    CorePlotting::RenderablePolyLineBatch buildAnalogBatch(
            std::shared_ptr<AnalogTimeSeries> const & series,
            NewAnalogTimeSeriesDisplayOptions const & display_options,
            int start_time,
            int end_time) const;

    /**
     * @brief Build a RenderableGlyphBatch from a digital event series.
     */
    CorePlotting::RenderableGlyphBatch buildEventBatch(
            std::shared_ptr<DigitalEventSeries> const & series,
            NewDigitalEventSeriesDisplayOptions const & display_options,
            int start_time,
            int end_time) const;

    /**
     * @brief Build a RenderableRectangleBatch from a digital interval series.
     */
    CorePlotting::RenderableRectangleBatch buildIntervalBatch(
            std::shared_ptr<DigitalIntervalSeries> const & series,
            NewDigitalIntervalSeriesDisplayOptions const & display_options,
            float start_time,
            float end_time) const;

    OpenGLWidget * gl_widget_;

    // SVG canvas dimensions
    int svg_width_{1920};
    int svg_height_{1080};

    // Scalebar configuration
    bool scalebar_enabled_{false};
    int scalebar_length_{100};
};

#endif// SVGEXPORTER_HPP
