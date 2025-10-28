#include "SVGExporter.hpp"

#include "OpenGLWidget.hpp"
#include "DataViewer/PlottingManager/PlottingManager.hpp"
#include "DataViewer/AnalogTimeSeries/AnalogTimeSeriesDisplayOptions.hpp"
#include "DataViewer/AnalogTimeSeries/MVP_AnalogTimeSeries.hpp"
#include "DataViewer/DigitalEvent/DigitalEventSeriesDisplayOptions.hpp"
#include "DataViewer/DigitalEvent/MVP_DigitalEvent.hpp"
#include "DataViewer/DigitalInterval/DigitalIntervalSeriesDisplayOptions.hpp"
#include "DataViewer/DigitalInterval/MVP_DigitalInterval.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "DataManager/utils/color.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

SVGExporter::SVGExporter(OpenGLWidget * gl_widget, PlottingManager * plotting_manager)
    : gl_widget_(gl_widget),
      plotting_manager_(plotting_manager) {
}

QString SVGExporter::exportToSVG() {
    svg_elements_.clear();

    // Get current visible time range from OpenGL widget
    auto const x_axis = gl_widget_->getXAxis();
    auto const start_time = x_axis.getStart();
    auto const end_time = x_axis.getEnd();
    auto const y_min = gl_widget_->getYMin();
    auto const y_max = gl_widget_->getYMax();

    std::cout << "SVG Export - Time range: " << start_time << " to " << end_time << std::endl;
    std::cout << "SVG Export - Y range: " << y_min << " to " << y_max << std::endl;

    // Export in same order as OpenGL rendering: intervals first (background), then analog, then events
    
    // 1. Export digital interval series (rendered as background)
    auto const & interval_series_map = gl_widget_->getDigitalIntervalSeriesMap();
    for (auto const & [key, interval_data] : interval_series_map) {
        if (interval_data.display_options->is_visible) {
            addDigitalIntervalSeries(
                key,
                interval_data.series,
                interval_data.time_frame,
                *interval_data.display_options,
                static_cast<float>(start_time),
                static_cast<float>(end_time));
        }
    }

    // 2. Export analog time series
    auto const & analog_series_map = gl_widget_->getAnalogSeriesMap();
    for (auto const & [key, analog_data] : analog_series_map) {
        if (analog_data.display_options->is_visible) {
            addAnalogSeries(
                key,
                analog_data.series,
                analog_data.time_frame,
                *analog_data.display_options,
                start_time,
                end_time);
        }
    }

    // 3. Export digital event series
    auto const & event_series_map = gl_widget_->getDigitalEventSeriesMap();
    for (auto const & [key, event_data] : event_series_map) {
        if (event_data.display_options->is_visible) {
            addDigitalEventSeries(
                key,
                event_data.series,
                event_data.time_frame,
                *event_data.display_options,
                start_time,
                end_time);
        }
    }

    return buildSVGDocument();
}

glm::vec2 SVGExporter::transformVertexToSVG(glm::vec4 const & vertex, glm::mat4 const & mvp) const {
    // Apply MVP transformation to get normalized device coordinates (NDC)
    glm::vec4 ndc = mvp * vertex;
    
    // Perform perspective divide if w != 0
    if (std::abs(ndc.w) > 1e-6f) {
        ndc /= ndc.w;
    }

    // Map NDC [-1, 1] to SVG coordinates [0, width] × [0, height]
    // Note: SVG Y-axis is inverted (top = 0, bottom = height)
    float const svg_x = static_cast<float>(svg_width_) * (ndc.x + 1.0f) / 2.0f;
    float const svg_y = static_cast<float>(svg_height_) * (1.0f - ndc.y) / 2.0f;

    return glm::vec2(svg_x, svg_y);
}

QString SVGExporter::colorToHex(int r, int g, int b) {
    return QString("#%1%2%3")
        .arg(r, 2, 16, QChar('0'))
        .arg(g, 2, 16, QChar('0'))
        .arg(b, 2, 16, QChar('0'));
}

void SVGExporter::addAnalogSeries(
        std::string const & key,
        std::shared_ptr<AnalogTimeSeries> const & series,
        std::shared_ptr<TimeFrame> const & time_frame,
        NewAnalogTimeSeriesDisplayOptions const & display_options,
        int start_time,
        int end_time) {

    std::cout << "SVG Export - Adding analog series: " << key << std::endl;

    // Get data in visible range
    auto const series_start_index = getTimeIndexForSeries(
        TimeFrameIndex(start_time),
        gl_widget_->getMasterTimeFrame().get(),
        time_frame.get());
    auto const series_end_index = getTimeIndexForSeries(
        TimeFrameIndex(end_time),
        gl_widget_->getMasterTimeFrame().get(),
        time_frame.get());

    auto analog_range = series->getTimeValueSpanInTimeFrameIndexRange(
        series_start_index, series_end_index);

    if (analog_range.values.empty()) {
        std::cout << "SVG Export - No data in range for " << key << std::endl;
        return;
    }

    // Build MVP matrices using same logic as OpenGL rendering
    auto const Model = new_getAnalogModelMat(
        display_options,
        display_options.cached_std_dev,
        display_options.cached_mean,
        *plotting_manager_);
    auto const View = new_getAnalogViewMat(*plotting_manager_);
    auto const Projection = new_getAnalogProjectionMat(
        TimeFrameIndex(start_time),
        TimeFrameIndex(end_time),
        gl_widget_->getYMin(),
        gl_widget_->getYMax(),
        *plotting_manager_);

    glm::mat4 const mvp = Projection * View * Model;

    // Convert color
    int r, g, b;
    hexToRGB(display_options.hex_color, r, g, b);
    QString const color = colorToHex(r, g, b);

    // Build polyline path
    // TODO: Implement gap handling (DetectGaps, ShowMarkers) similar to OpenGL rendering
    QStringList points;
    auto time_iter = analog_range.time_indices.begin();
    for (size_t i = 0; i < analog_range.values.size(); i++) {
        auto const x_data = time_frame->getTimeAtIndex(**time_iter);
        auto const y_data = analog_range.values[i];

        glm::vec4 const vertex(x_data, y_data, 0.0f, 1.0f);
        glm::vec2 const svg_pos = transformVertexToSVG(vertex, mvp);

        points.append(QString("%1,%2").arg(svg_pos.x).arg(svg_pos.y));
        ++(*time_iter);
    }

    // Create SVG polyline element
    QString const line_thickness = QString::number(display_options.line_thickness);
    QString const polyline = QString(
        R"(<polyline points="%1" fill="none" stroke="%2" stroke-width="%3" stroke-linejoin="round" stroke-linecap="round"/>)")
        .arg(points.join(" "))
        .arg(color)
        .arg(line_thickness);

    svg_elements_.append(polyline);
    std::cout << "SVG Export - Added " << points.size() << " points for " << key << std::endl;
}

void SVGExporter::addDigitalEventSeries(
        std::string const & key,
        std::shared_ptr<DigitalEventSeries> const & series,
        std::shared_ptr<TimeFrame> const & time_frame,
        NewDigitalEventSeriesDisplayOptions const & display_options,
        int start_time,
        int end_time) {

    std::cout << "SVG Export - Adding digital event series: " << key << std::endl;

    // Get events in visible range
    auto const series_start = getTimeIndexForSeries(
        TimeFrameIndex(start_time),
        gl_widget_->getMasterTimeFrame().get(),
        time_frame.get());
    auto const series_end = getTimeIndexForSeries(
        TimeFrameIndex(end_time),
        gl_widget_->getMasterTimeFrame().get(),
        time_frame.get());

    auto visible_events = series->getEventsInRange(series_start, series_end);

    // Build MVP matrices using same logic as OpenGL rendering
    auto const Model = new_getEventModelMat(display_options, *plotting_manager_);
    auto const View = new_getEventViewMat(display_options, *plotting_manager_);
    auto const Projection = new_getEventProjectionMat(
        start_time,
        end_time,
        gl_widget_->getYMin(),
        gl_widget_->getYMax(),
        *plotting_manager_);

    glm::mat4 const mvp = Projection * View * Model;

    // Convert color
    int r, g, b;
    hexToRGB(display_options.hex_color, r, g, b);
    QString const color = colorToHex(r, g, b);

    // Draw each event as a vertical line
    int event_count = 0;
    for (auto const & event_index : visible_events) {
        event_count++;
        auto const event_time = time_frame->getTimeAtIndex(TimeFrameIndex(event_index));

        // Create vertical line using same coordinates as OpenGL
        // Events extend based on display mode (Stacked or FullCanvas)
        float y_bottom, y_top;
        if (display_options.display_mode == EventDisplayMode::Stacked) {
            // Use allocated bounds with event height
            y_bottom = display_options.allocated_y_center - display_options.event_height / 2.0f;
            y_top = display_options.allocated_y_center + display_options.event_height / 2.0f;
        } else {
            // Full canvas mode
            y_bottom = gl_widget_->getYMin();
            y_top = gl_widget_->getYMax();
        }

        glm::vec4 const bottom_vertex(event_time, y_bottom, 0.0f, 1.0f);
        glm::vec4 const top_vertex(event_time, y_top, 0.0f, 1.0f);

        glm::vec2 const svg_bottom = transformVertexToSVG(bottom_vertex, mvp);
        glm::vec2 const svg_top = transformVertexToSVG(top_vertex, mvp);

        QString const line_thickness = QString::number(display_options.line_thickness);
        QString const line = QString(
            R"(<line x1="%1" y1="%2" x2="%3" y2="%4" stroke="%5" stroke-width="%6"/>)")
            .arg(svg_bottom.x)
            .arg(svg_bottom.y)
            .arg(svg_top.x)
            .arg(svg_top.y)
            .arg(color)
            .arg(line_thickness);

        svg_elements_.append(line);
    }

    std::cout << "SVG Export - Added " << event_count << " events for " << key << std::endl;
}

void SVGExporter::addDigitalIntervalSeries(
        std::string const & key,
        std::shared_ptr<DigitalIntervalSeries> const & series,
        std::shared_ptr<TimeFrame> const & time_frame,
        NewDigitalIntervalSeriesDisplayOptions const & display_options,
        float start_time,
        float end_time) {

    std::cout << "SVG Export - Adding digital interval series: " << key << std::endl;

    // Get intervals that overlap with visible range
    auto visible_intervals = series->getIntervalsInRange<DigitalIntervalSeries::RangeMode::OVERLAPPING>(
        TimeFrameIndex(static_cast<int64_t>(start_time)),
        TimeFrameIndex(static_cast<int64_t>(end_time)),
        gl_widget_->getMasterTimeFrame().get(),
        time_frame.get());

    // Build MVP matrices using same logic as OpenGL rendering
    auto const Model = new_getIntervalModelMat(display_options, *plotting_manager_);
    auto const View = new_getIntervalViewMat(*plotting_manager_);
    auto const Projection = new_getIntervalProjectionMat(
        start_time,
        end_time,
        gl_widget_->getYMin(),
        gl_widget_->getYMax(),
        *plotting_manager_);

    glm::mat4 const mvp = Projection * View * Model;

    // Convert color
    int r, g, b;
    hexToRGB(display_options.hex_color, r, g, b);
    QString const color = colorToHex(r, g, b);

    // Draw each interval as a filled rectangle
    int interval_count = 0;
    for (auto const & interval : visible_intervals) {
        interval_count++;
        // Interval spans from start to end time
        float const interval_start = static_cast<float>(interval.start);
        float const interval_end = static_cast<float>(interval.end);

        // Full canvas height for intervals
        float const y_bottom = gl_widget_->getYMin();
        float const y_top = gl_widget_->getYMax();

        // Define rectangle corners
        glm::vec4 const bottom_left(interval_start, y_bottom, 0.0f, 1.0f);
        glm::vec4 const top_right(interval_end, y_top, 0.0f, 1.0f);

        glm::vec2 const svg_bottom_left = transformVertexToSVG(bottom_left, mvp);
        glm::vec2 const svg_top_right = transformVertexToSVG(top_right, mvp);

        // Calculate SVG rectangle parameters
        float const svg_x = std::min(svg_bottom_left.x, svg_top_right.x);
        float const svg_y = std::min(svg_bottom_left.y, svg_top_right.y);
        float const svg_width = std::abs(svg_top_right.x - svg_bottom_left.x);
        float const svg_height = std::abs(svg_top_right.y - svg_bottom_left.y);

        QString const rect = QString(
            R"(<rect x="%1" y="%2" width="%3" height="%4" fill="%5" fill-opacity="%6" stroke="none"/>)")
            .arg(svg_x)
            .arg(svg_y)
            .arg(svg_width)
            .arg(svg_height)
            .arg(color)
            .arg(display_options.alpha);

        svg_elements_.append(rect);
    }

    std::cout << "SVG Export - Added " << interval_count << " intervals for " << key << std::endl;
}

QString SVGExporter::buildSVGDocument() const {
    // Get background color from OpenGL widget
    auto const bg_color = gl_widget_->getBackgroundColor();
    
    // Build SVG document with fixed canvas size and viewBox for scaling
    QString svg_header = QString(
        R"(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<svg width="%1" height="%2" viewBox="0 0 %1 %2" xmlns="http://www.w3.org/2000/svg" version="1.1">
  <desc>WhiskerToolbox DataViewer Export</desc>
  <rect width="100%%" height="100%%" fill="%3"/>
)")
        .arg(svg_width_)
        .arg(svg_height_)
        .arg(QString::fromStdString(bg_color));

    QString const svg_footer = "</svg>";

    // Combine all elements
    QString document = svg_header;
    for (auto const & element : svg_elements_) {
        document += "  " + element + "\n";
    }
    document += svg_footer;

    return document;
}
