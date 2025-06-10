#ifndef DATAVIEWER_DIGITALINTERVALSERIESDISPLAYOPTIONS_HPP
#define DATAVIEWER_DIGITALINTERVALSERIESDISPLAYOPTIONS_HPP

#include <string>

/**
 * @brief Display options for new digital interval series visualization system
 *
 * Configuration for digital interval series display including visual properties,
 * positioning, and rendering options. Intervals are rendered as rectangles
 * extending from top to bottom of canvas with lighter alpha.
 */
struct NewDigitalIntervalSeriesDisplayOptions {
    // Visual properties
    std::string hex_color{"#ff6b6b"};///< Color of the intervals (default: light red)
    float alpha{0.3f};               ///< Alpha transparency for intervals (default: 30%)
    bool is_visible{true};           ///< Whether intervals are visible

    // Global scaling applied by PlottingManager
    float global_zoom{1.0f};          ///< Global zoom factor
    float global_vertical_scale{1.0f};///< Global vertical scale factor

    // Positioning allocated by PlottingManager
    float allocated_y_center{0.0f};///< Y-coordinate center allocated by plotting manager
    float allocated_height{2.0f};  ///< Height allocated by plotting manager (full canvas by default)

    // Rendering options
    bool extend_full_canvas{true};///< Whether intervals extend from top to bottom of canvas
    float margin_factor{0.95f};   ///< Margin factor for interval height (0.95 = 95% of allocated space)
    float interval_height{1.0f};  ///< Height of the interval (1.0 = full canvas by default)
};

#endif// DATAVIEWER_DIGITALINTERVALSERIESDISPLAYOPTIONS_HPP
