#ifndef TIMESERIES_DISPLAY_OPTIONS_HPP
#define TIMESERIES_DISPLAY_OPTIONS_HPP

#include "utils/color.hpp"

#include <string>
#include <vector>

namespace TimeSeriesDefaultValues {
    const std::string COLOR = "#007bff";
    const float ALPHA = 1.0f;
    const float INTERVAL_ALPHA = 0.3f; // 30% transparency for intervals
    const bool VISIBLE = false;
    const float SCALE_FACTOR = 1.0f;
    const float Y_OFFSET = 0.0f;
    const int LINE_THICKNESS = 1;
    const bool SHOW_EVENTS_AS_LINES = true;
    const float EVENT_LINE_HEIGHT = 1.0f;
    const bool SHOW_INTERVALS_AS_FILLED = true;
    const float INTERVAL_HEIGHT = 1.0f;
    
    // Analog series gap handling defaults
    const float GAP_THRESHOLD = 5.0f; // Default gap threshold (in time units)
    const bool ENABLE_GAP_DETECTION = false; // Default: always connect points
    
    const std::vector<std::string> DEFAULT_COLORS = {
        "#ff0000", // Red
        "#008000", // Green
        "#0000ff", // Blue
        "#ff00ff", // Magenta
        "#ffff00", // Yellow
        "#00ffff", // Cyan
        "#ffa500", // Orange
        "#800080"  // Purple
    };
    
    // Get color from index, returns random color if index exceeds DEFAULT_COLORS size
    inline std::string getColorForIndex(size_t index) {
        if (index < DEFAULT_COLORS.size()) {
            return DEFAULT_COLORS[index];
        } else {
            return generateRandomColor();
        }
    }
}

struct BaseTimeSeriesDisplayOptions {
    std::string hex_color{TimeSeriesDefaultValues::COLOR};
    float alpha{TimeSeriesDefaultValues::ALPHA};
    bool is_visible{TimeSeriesDefaultValues::VISIBLE};
    float y_offset{TimeSeriesDefaultValues::Y_OFFSET};

    virtual ~BaseTimeSeriesDisplayOptions() = default;
};

enum class AnalogGapHandling {
    AlwaysConnect,    // Always connect points (current behavior)
    DetectGaps,       // Break lines when gaps exceed threshold
    ShowMarkers       // Show individual markers instead of lines
};

struct AnalogTimeSeriesDisplayOptions : public BaseTimeSeriesDisplayOptions {
    float scale_factor{TimeSeriesDefaultValues::SCALE_FACTOR}; // Internal scale factor (stdDev * 5.0f * user_scale)
    float user_scale_factor{1.0f}; // User-friendly scale factor (1.0 = normal, 2.0 = double size, etc.)
    int line_thickness{TimeSeriesDefaultValues::LINE_THICKNESS};
    
    // Gap handling options
    AnalogGapHandling gap_handling{AnalogGapHandling::AlwaysConnect};
    float gap_threshold{TimeSeriesDefaultValues::GAP_THRESHOLD}; // Time units above which to break lines
    bool enable_gap_detection{TimeSeriesDefaultValues::ENABLE_GAP_DETECTION};
    
    // Future: line_style (e.g., solid, dashed, dotted enum)
    // Future: show_markers_at_samples
};

struct DigitalEventSeriesDisplayOptions : public BaseTimeSeriesDisplayOptions {
    bool show_as_lines{TimeSeriesDefaultValues::SHOW_EVENTS_AS_LINES};
    float event_line_height{TimeSeriesDefaultValues::EVENT_LINE_HEIGHT};
    int line_thickness{TimeSeriesDefaultValues::LINE_THICKNESS};
    // Future: event_marker_style (e.g., line, arrow, dot enum)
};

struct DigitalIntervalSeriesDisplayOptions : public BaseTimeSeriesDisplayOptions {
    bool show_as_filled{TimeSeriesDefaultValues::SHOW_INTERVALS_AS_FILLED};
    float interval_height{TimeSeriesDefaultValues::INTERVAL_HEIGHT};
    // Future: border_thickness, fill_pattern
    
    // Override the default alpha for intervals
    DigitalIntervalSeriesDisplayOptions() {
        alpha = TimeSeriesDefaultValues::INTERVAL_ALPHA;
    }
};

#endif // TIMESERIES_DISPLAY_OPTIONS_HPP 