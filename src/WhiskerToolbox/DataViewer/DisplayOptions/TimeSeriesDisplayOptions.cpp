#include "TimeSeriesDisplayOptions.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/AnalogTimeSeriesDisplayOptions.hpp"

#include "utils/color.hpp"

namespace TimeSeriesDefaultValues {
std::string getColorForIndex(size_t index) {
    if (index < DEFAULT_COLORS.size()) {
        return DEFAULT_COLORS[index];
    } else {
        return generateRandomColor();
    }
}
}// namespace TimeSeriesDefaultValues


float getCachedStdDev(AnalogTimeSeries const & series, NewAnalogTimeSeriesDisplayOptions & display_options) {
    if (!display_options.std_dev_cache_valid) {
        // Calculate and cache the standard deviation
        display_options.cached_std_dev = calculate_std_dev(series);
        display_options.std_dev_cache_valid = true;
    }
    return display_options.cached_std_dev;
}

void invalidateDisplayCache(NewAnalogTimeSeriesDisplayOptions & display_options) {
    display_options.std_dev_cache_valid = false;
}
