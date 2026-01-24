#include "AnalogSeriesHelpers.hpp"

#include "AnalogTimeSeriesDisplayOptions.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/utils/statistics.hpp"
#include "CorePlotting/DataTypes/SeriesDataCache.hpp"

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

void setAnalogIntrinsicPropertiesForCache(AnalogTimeSeries const * analog,
                                          CorePlotting::SeriesDataCache & data_cache) {
    if (!analog) {
        data_cache.cached_mean = 0.0f;
        data_cache.cached_std_dev = 0.0f;
        data_cache.intrinsic_scale = 1.0f;
        data_cache.mean_cache_valid = true;
        data_cache.std_dev_cache_valid = true;
        return;
    }

    // Calculate mean
    data_cache.cached_mean = calculate_mean(*analog);
    data_cache.mean_cache_valid = true;

    // Calculate standard deviation
    data_cache.cached_std_dev = calculate_std_dev_approximate(*analog);
    data_cache.std_dev_cache_valid = true;
    
    // Calculate intrinsic scale (3 * std_dev for typical data range)
    if (data_cache.cached_std_dev > 0.0f) {
        data_cache.intrinsic_scale = 1.0f / (3.0f * data_cache.cached_std_dev);
    } else {
        data_cache.intrinsic_scale = 1.0f;
    }
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
