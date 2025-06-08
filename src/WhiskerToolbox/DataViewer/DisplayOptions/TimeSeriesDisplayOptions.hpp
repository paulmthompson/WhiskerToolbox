#ifndef TIMESERIES_DISPLAY_OPTIONS_HPP
#define TIMESERIES_DISPLAY_OPTIONS_HPP

#include "utils/color.hpp"

#include <string>
#include <vector>

// Forward declaration
class AnalogTimeSeries;

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
    
    // VerticalSpaceManager integration
    float allocated_height{0.0f}; // Height allocated by VerticalSpaceManager (normalized coordinates)
    
    // Performance cache for display calculations
    mutable float cached_std_dev{0.0f}; // Cached standard deviation for performance
    mutable bool std_dev_cache_valid{false}; // Whether cached std dev is valid
    
    // Gap handling options
    AnalogGapHandling gap_handling{AnalogGapHandling::AlwaysConnect};
    float gap_threshold{TimeSeriesDefaultValues::GAP_THRESHOLD}; // Time units above which to break lines
    bool enable_gap_detection{TimeSeriesDefaultValues::ENABLE_GAP_DETECTION};
    
    // Future: line_style (e.g., solid, dashed, dotted enum)
    // Future: show_markers_at_samples
};

/**
 * @brief Event display modes for digital event series
 * 
 * Controls how digital events are visually presented on the canvas:
 * - Stacked: Events are positioned in separate horizontal lanes with configurable spacing
 * - FullCanvas: Events stretch from top to bottom of the entire canvas
 */
enum class EventDisplayMode {
    Stacked,      ///< Stack events with configurable spacing (default)
    FullCanvas    ///< Events stretch from top to bottom of canvas
};

struct DigitalEventSeriesDisplayOptions : public BaseTimeSeriesDisplayOptions {
    bool show_as_lines{TimeSeriesDefaultValues::SHOW_EVENTS_AS_LINES};
    float event_line_height{TimeSeriesDefaultValues::EVENT_LINE_HEIGHT};
    int line_thickness{TimeSeriesDefaultValues::LINE_THICKNESS};
    
    // Event stacking options
    EventDisplayMode display_mode{EventDisplayMode::Stacked}; ///< Display mode (stacked vs full-canvas)
    float vertical_spacing{0.1f}; ///< Spacing between stacked event series in normalized coordinates
    float event_height{0.08f}; ///< Height of each event line in stacked mode (normalized coordinates)
    
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

/**
 * @brief Get cached standard deviation for an analog series
 * 
 * Calculates and caches the standard deviation of the analog series for performance.
 * The cache is automatically invalidated when the series data changes.
 * 
 * @param series The analog time series to calculate standard deviation for
 * @param display_options The display options containing the cache
 * @return Cached standard deviation value
 */
float getCachedStdDev(AnalogTimeSeries const & series, AnalogTimeSeriesDisplayOptions & display_options);

#endif // TIMESERIES_DISPLAY_OPTIONS_HPP
