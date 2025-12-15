#ifndef DATAVIEWER_ANALOGSERIESHELPERS_HPP
#define DATAVIEWER_ANALOGSERIESHELPERS_HPP

class AnalogTimeSeries;
struct NewAnalogTimeSeriesDisplayOptions;

/**
 * @brief Calculate and cache intrinsic properties (mean, std dev) for analog series
 * 
 * Computes statistical properties of the analog series and stores them in the
 * display options cache. These values are used for data-driven scaling.
 * 
 * @param analog Pointer to analog time series (nullptr safe)
 * @param display_options Display options with data cache to populate
 */
void setAnalogIntrinsicProperties(AnalogTimeSeries const * analog,
                                  NewAnalogTimeSeriesDisplayOptions & display_options);

/**
 * @brief Get cached standard deviation, computing if necessary
 * 
 * Returns the cached standard deviation from display options, computing and
 * caching it if the cache is invalid.
 * 
 * @param series Analog time series to compute std dev for
 * @param display_options Display options with std dev cache
 * @return Cached or freshly computed standard deviation
 */
float getCachedStdDev(AnalogTimeSeries const & series, 
                     NewAnalogTimeSeriesDisplayOptions & display_options);

/**
 * @brief Invalidate the display cache for an analog series
 * 
 * Marks cached statistical values as invalid, forcing recomputation on next access.
 * Call this when the underlying data changes.
 * 
 * @param display_options Display options with cache to invalidate
 */
void invalidateDisplayCache(NewAnalogTimeSeriesDisplayOptions & display_options);

#endif // DATAVIEWER_ANALOGSERIESHELPERS_HPP
