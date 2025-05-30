#include "TimeSeriesDisplayOptions.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

float getCachedStdDev(AnalogTimeSeries const & series, AnalogTimeSeriesDisplayOptions & display_options) {
    if (!display_options.std_dev_cache_valid) {
        // Calculate and cache the standard deviation
        display_options.cached_std_dev = calculate_std_dev(series);
        display_options.std_dev_cache_valid = true;
    }
    return display_options.cached_std_dev;
}

void invalidateDisplayCache(AnalogTimeSeriesDisplayOptions & display_options) {
    display_options.std_dev_cache_valid = false;
} 