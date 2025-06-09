#ifndef TIMESERIES_DISPLAY_OPTIONS_HPP
#define TIMESERIES_DISPLAY_OPTIONS_HPP

#include <string>
#include <vector>

// Forward declaration
class AnalogTimeSeries;

namespace TimeSeriesDefaultValues {
std::string const COLOR = "#007bff";
float const ALPHA = 1.0f;
float const INTERVAL_ALPHA = 0.3f;// 30% transparency for intervals
bool const VISIBLE = false;
float const SCALE_FACTOR = 1.0f;
float const Y_OFFSET = 0.0f;
int const LINE_THICKNESS = 1;
bool const SHOW_EVENTS_AS_LINES = true;
float const EVENT_LINE_HEIGHT = 1.0f;
bool const SHOW_INTERVALS_AS_FILLED = true;
float const INTERVAL_HEIGHT = 1.0f;

// Analog series gap handling defaults
float const GAP_THRESHOLD = 5.0f;       // Default gap threshold (in time units)
bool const ENABLE_GAP_DETECTION = false;// Default: always connect points

std::vector<std::string> const DEFAULT_COLORS = {
        "#ff0000",// Red
        "#008000",// Green
        "#0000ff",// Blue
        "#ff00ff",// Magenta
        "#ffff00",// Yellow
        "#00ffff",// Cyan
        "#ffa500",// Orange
        "#800080" // Purple
};

// Get color from index, returns random color if index exceeds DEFAULT_COLORS size
std::string getColorForIndex(size_t index);
}// namespace TimeSeriesDefaultValues

struct BaseTimeSeriesDisplayOptions {
    std::string hex_color{TimeSeriesDefaultValues::COLOR};
    float alpha{TimeSeriesDefaultValues::ALPHA};
    bool is_visible{TimeSeriesDefaultValues::VISIBLE};
    float y_offset{TimeSeriesDefaultValues::Y_OFFSET};

    virtual ~BaseTimeSeriesDisplayOptions() = default;
};

enum class AnalogGapHandling {
    AlwaysConnect,// Always connect points (current behavior)
    DetectGaps,   // Break lines when gaps exceed threshold
    ShowMarkers   // Show individual markers instead of lines
};

/**
 * @brief Event display modes for digital event series
 * 
 * Controls how digital events are visually presented on the canvas:
 * - Stacked: Events are positioned in separate horizontal lanes with configurable spacing
 * - FullCanvas: Events stretch from top to bottom of the entire canvas
 */
enum class EventDisplayMode {
    Stacked,  ///< Stack events with configurable spacing (default)
    FullCanvas///< Events stretch from top to bottom of canvas
};

// Forward declaration for new display options
struct NewAnalogTimeSeriesDisplayOptions;

/**
 * @brief Get cached standard deviation for an analog series (new display options)
 * 
 * Overload for the new display options structure.
 * 
 * @param series The analog time series to calculate standard deviation for
 * @param display_options The new display options containing the cache
 * @return Cached standard deviation value
 */
float getCachedStdDev(AnalogTimeSeries const & series, NewAnalogTimeSeriesDisplayOptions & display_options);

/**
 * @brief Invalidate cached display calculations (new display options)
 * 
 * Overload for the new display options structure.
 * 
 * @param display_options The new display options to invalidate
 */
void invalidateDisplayCache(NewAnalogTimeSeriesDisplayOptions & display_options);

#endif// TIMESERIES_DISPLAY_OPTIONS_HPP
