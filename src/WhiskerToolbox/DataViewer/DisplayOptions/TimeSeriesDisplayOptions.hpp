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
