#ifndef SVGEXPORTER_HPP
#define SVGEXPORTER_HPP

#include <QString>
#include <QStringList>
#include <glm/glm.hpp>

#include <memory>
#include <string>

class OpenGLWidget;
struct PlottingManager;
struct NewAnalogTimeSeriesDisplayOptions;
struct NewDigitalEventSeriesDisplayOptions;
struct NewDigitalIntervalSeriesDisplayOptions;
class AnalogTimeSeriesInMemory;
using AnalogTimeSeries = AnalogTimeSeriesInMemory;
class DigitalEventSeries;
class DigitalIntervalSeries;
class TimeFrame;

/**
 * @brief SVG export utility for DataViewer plots
 * 
 * Exports the current plot state to SVG format by reusing the same MVP
 * matrix transformations used in OpenGL rendering. This ensures that
 * the SVG output matches the on-screen visualization exactly.
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
     * @param gl_widget OpenGL widget containing plot data and state
     * @param plotting_manager Plotting manager for coordinate allocation
     */
    SVGExporter(OpenGLWidget * gl_widget, PlottingManager * plotting_manager);

    /**
     * @brief Export current plot to SVG format
     * 
     * Generates an SVG document representing the current plot state,
     * using the same MVP transformations as OpenGL rendering.
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
     * @brief Transform vertex from data space to SVG coordinates
     * 
     * Applies Model-View-Projection transformation to convert data coordinates
     * to normalized device coordinates (NDC), then maps NDC to SVG pixel coordinates.
     * 
     * @param vertex Input vertex in data coordinates (x, y, z, w)
     * @param mvp Combined Model-View-Projection matrix
     * @return 2D point in SVG coordinate space
     */
    glm::vec2 transformVertexToSVG(glm::vec4 const & vertex, glm::mat4 const & mvp) const;

    /**
     * @brief Convert RGB color to SVG hex string
     * 
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     * @return Hex color string (e.g., "#FF5733")
     */
    static QString colorToHex(int r, int g, int b);

    /**
     * @brief Add analog time series to SVG output
     * 
     * Renders analog series as polyline or path elements using the same
     * MVP transformation as OpenGL rendering.
     * 
     * @param key Series identifier
     * @param series Analog time series data
     * @param time_frame Time frame for the series
     * @param display_options Display configuration (color, scaling, visibility)
     * @param start_time Visible range start time
     * @param end_time Visible range end time
     */
    void addAnalogSeries(
            std::string const & key,
            std::shared_ptr<AnalogTimeSeries> const & series,
            std::shared_ptr<TimeFrame> const & time_frame,
            NewAnalogTimeSeriesDisplayOptions const & display_options,
            int start_time,
            int end_time);

    /**
     * @brief Add digital event series to SVG output
     * 
     * Renders digital events as vertical line elements using the same
     * MVP transformation as OpenGL rendering.
     * 
     * @param key Series identifier
     * @param series Digital event series data
     * @param time_frame Time frame for the series
     * @param display_options Display configuration (color, visibility, stacking)
     * @param start_time Visible range start time
     * @param end_time Visible range end time
     */
    void addDigitalEventSeries(
            std::string const & key,
            std::shared_ptr<DigitalEventSeries> const & series,
            std::shared_ptr<TimeFrame> const & time_frame,
            NewDigitalEventSeriesDisplayOptions const & display_options,
            int start_time,
            int end_time);

    /**
     * @brief Add digital interval series to SVG output
     * 
     * Renders digital intervals as rectangle elements with transparency
     * using the same MVP transformation as OpenGL rendering.
     * 
     * @param key Series identifier
     * @param series Digital interval series data
     * @param time_frame Time frame for the series
     * @param display_options Display configuration (color, alpha, visibility)
     * @param start_time Visible range start time
     * @param end_time Visible range end time
     */
    void addDigitalIntervalSeries(
            std::string const & key,
            std::shared_ptr<DigitalIntervalSeries> const & series,
            std::shared_ptr<TimeFrame> const & time_frame,
            NewDigitalIntervalSeriesDisplayOptions const & display_options,
            float start_time,
            float end_time);

    /**
     * @brief Build complete SVG document from accumulated elements
     * 
     * @return Complete SVG XML document as QString
     */
    QString buildSVGDocument() const;

    /**
     * @brief Add horizontal scalebar to SVG output
     * 
     * Draws a black horizontal scalebar in the bottom-right corner of the canvas
     * with a label indicating its length in time units.
     * 
     * @param start_time Visible time range start (for coordinate conversion)
     * @param end_time Visible time range end (for coordinate conversion)
     */
    void addScalebar(float start_time, float end_time);

    OpenGLWidget * gl_widget_;
    PlottingManager * plotting_manager_;

    // SVG canvas dimensions
    int svg_width_{1920};
    int svg_height_{1080};

    // Scalebar configuration
    bool scalebar_enabled_{false};
    int scalebar_length_{100};

    // Accumulated SVG elements
    QStringList svg_elements_;
};

#endif // SVGEXPORTER_HPP
