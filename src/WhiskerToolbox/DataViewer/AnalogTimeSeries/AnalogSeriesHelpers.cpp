#include "AnalogSeriesHelpers.hpp"

#include "AnalogTimeSeriesDisplayOptions.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/utils/statistics.hpp"

void setAnalogIntrinsicProperties(AnalogTimeSeries const * analog,
                                  NewAnalogTimeSeriesDisplayOptions & display_options) {
    if (!analog) {
        display_options.data_cache.cached_mean = 0.0f;
        display_options.data_cache.cached_std_dev = 0.0f;
        display_options.data_cache.mean_cache_valid = true;
        display_options.data_cache.std_dev_cache_valid = true;
        return;
    }

    // Calculate mean
    display_options.data_cache.cached_mean = calculate_mean(*analog);
    display_options.data_cache.mean_cache_valid = true;

    // Calculate standard deviation
    display_options.data_cache.cached_std_dev = calculate_std_dev_approximate(*analog);
    display_options.data_cache.std_dev_cache_valid = true;
}

float getCachedStdDev(AnalogTimeSeries const & series, 
                     NewAnalogTimeSeriesDisplayOptions & display_options) {
    if (!display_options.data_cache.std_dev_cache_valid) {
        // Calculate and cache the standard deviation
        display_options.data_cache.cached_std_dev = calculate_std_dev(series);
        display_options.data_cache.std_dev_cache_valid = true;
    }
    return display_options.data_cache.cached_std_dev;
}

void invalidateDisplayCache(NewAnalogTimeSeriesDisplayOptions & display_options) {
    display_options.data_cache.std_dev_cache_valid = false;
    display_options.data_cache.mean_cache_valid = false;
}
