#ifndef NEW_FILTER_BRIDGE_HPP
#define NEW_FILTER_BRIDGE_HPP

#include "IFilter.hpp"
#include "FilterFactory.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "filter.hpp"
#include <memory>

/**
 * @brief Bridge function to apply new filter interface to AnalogTimeSeries
 * 
 * This function creates a bridge between the new filter interface (using std::span)
 * and the existing AnalogTimeSeries data structure.
 * 
 * @param analog_time_series Input time series to filter
 * @param options Filter configuration options
 * @return FilterResult containing filtered data using new interface
 */
FilterResult filterAnalogTimeSeriesNew(
    AnalogTimeSeries const * analog_time_series,
    FilterOptions const & options);

/**
 * @brief Apply new filter interface to AnalogTimeSeries within time range
 * 
 * @param analog_time_series Input time series to filter
 * @param start_time Start of time range to filter (inclusive)
 * @param end_time End of time range to filter (inclusive)
 * @param options Filter configuration options
 * @return FilterResult containing filtered data using new interface
 */
FilterResult filterAnalogTimeSeriesNew(
    AnalogTimeSeries const * analog_time_series,
    TimeFrameIndex start_time,
    TimeFrameIndex end_time,
    FilterOptions const & options);

#endif // NEW_FILTER_BRIDGE_HPP
