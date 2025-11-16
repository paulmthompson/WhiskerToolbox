#ifndef ANALOG_TIME_SERIES_STATISTICS_HPP
#define ANALOG_TIME_SERIES_STATISTICS_HPP

#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <span>
#include <vector>

class AnalogTimeSeriesInMemory;
using AnalogTimeSeries = AnalogTimeSeriesInMemory;

// ========== Mean ==========

/**
 * @brief Raw mean calculation implementation using iterators
 * 
 * This is the core implementation that all other mean functions call.
 * 
 * @param begin Iterator to the start of the data range
 * @param end Iterator to the end of the data range (exclusive)
 * @return float The mean value of the range
 */
template<typename Iterator>
float calculate_mean_impl(Iterator begin, Iterator end) {
    if (begin == end) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    auto const distance = std::distance(begin, end);
    if (distance <= 0) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    float sum = 0.0f;
    for (auto it = begin; it != end; ++it) {
        sum += *it;
    }

    return sum / static_cast<float>(distance);
}

/**
 * @brief Raw mean calculation implementation using vector with indices
 * 
 * This is an alternative implementation for when you have indices rather than iterators.
 * 
 * @param data Vector containing the data
 * @param start Start index of the range (inclusive)
 * @param end End index of the range (exclusive)
 * @return float The mean value in the specified range
 */
float calculate_mean_impl(std::vector<float> const & data, size_t start, size_t end);

/**
 * @brief Calculate the mean value of a span of data
 * 
 * @param data_span Span of float data
 * @return float The mean value
 */
float calculate_mean(std::span<float const> data_span);

/**
 * @brief Calculate the mean value of an AnalogTimeSeries
 *
 * @param series The time series to calculate the mean from
 * @return float The mean value
 */
float calculate_mean(AnalogTimeSeries const & series);

/**
 * @brief Calculate the mean value of an AnalogTimeSeries in a specific range
 *
 * @param series The time series to calculate the mean from
 * @param start Start index of the range (inclusive)
 * @param end End index of the range (exclusive)
 * @return float The mean value in the specified range
 */
float calculate_mean(AnalogTimeSeries const & series, int64_t start, int64_t end);

/**
 * @brief Calculate the mean value of an AnalogTimeSeries within a TimeFrameIndex range
 * 
 * This function uses the new getDataInTimeFrameIndexRange functionality to efficiently
 * calculate the mean for data points where TimeFrameIndex >= start_time and <= end_time.
 * It automatically handles boundary approximation if exact times don't exist.
 * 
 * @param series The time series to calculate the mean from
 * @param start_time The start TimeFrameIndex (inclusive boundary)
 * @param end_time The end TimeFrameIndex (inclusive boundary)
 * @return float The mean value in the specified time range
 */
float calculate_mean_in_time_range(AnalogTimeSeries const & series, TimeFrameIndex start_time, TimeFrameIndex end_time);

// ========== Standard Deviation ==========

/**
 * @brief Raw standard deviation calculation implementation using iterators
 * 
 * This is the core implementation that all other standard deviation functions call.
 * 
 * @param begin Iterator to the start of the data range
 * @param end Iterator to the end of the data range (exclusive)
 * @return float The standard deviation of the range
 */
template<typename Iterator>
float calculate_std_dev_impl(Iterator begin, Iterator end) {
    if (begin == end) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    auto const distance = std::distance(begin, end);
    if (distance <= 0) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    // Calculate mean first
    float const mean = calculate_mean_impl(begin, end);
    if (std::isnan(mean)) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    // Calculate variance
    float sum = 0.0f;
    for (auto it = begin; it != end; ++it) {
        float const diff = *it - mean;
        sum += diff * diff;
    }

    return std::sqrt(sum / static_cast<float>(distance));
}

/**
 * @brief Raw standard deviation calculation implementation using vector with indices
 * 
 * This is an alternative implementation for when you have indices rather than iterators.
 * 
 * @param data Vector containing the data
 * @param start Start index of the range (inclusive)
 * @param end End index of the range (exclusive)
 * @return float The standard deviation in the specified range
 */
float calculate_std_dev_impl(std::vector<float> const & data, size_t start, size_t end);

/**
 * @brief Calculate the standard deviation of a span of data
 * 
 * @param data_span Span of float data
 * @return float The standard deviation
 */
float calculate_std_dev(std::span<float const> data_span);

/**
 * @brief Calculate the standard deviation of an AnalogTimeSeries
 *
 * @param series The time series to calculate the standard deviation from
 * @return float The standard deviation
 */
float calculate_std_dev(AnalogTimeSeries const & series);

/**
 * @brief Calculate the standard deviation of an AnalogTimeSeries in a specific range
 *
 * @param series The time series to calculate the standard deviation from
 * @param start Start index of the range (inclusive)
 * @param end End index of the range (exclusive)
 * @return float The standard deviation in the specified range
 */
float calculate_std_dev(AnalogTimeSeries const & series, int64_t start, int64_t end);

/**
 * @brief Calculate the standard deviation of an AnalogTimeSeries within a TimeFrameIndex range
 * 
 * This function uses the getDataInTimeFrameIndexRange functionality to efficiently
 * calculate the standard deviation for data points where TimeFrameIndex >= start_time and <= end_time.
 * It automatically handles boundary approximation if exact times don't exist.
 * 
 * @param series The time series to calculate the standard deviation from
 * @param start_time The start TimeFrameIndex (inclusive boundary)
 * @param end_time The end TimeFrameIndex (inclusive boundary)
 * @return float The standard deviation in the specified time range
 */
float calculate_std_dev_in_time_range(AnalogTimeSeries const & series, TimeFrameIndex start_time, TimeFrameIndex end_time);

/**
 * @brief Calculate an approximate standard deviation using systematic sampling
 *
 * Uses systematic sampling (every Nth element) to estimate standard deviation efficiently.
 * If the sample size would be below the minimum threshold, falls back to exact calculation.
 *
 * @param series The time series to calculate the standard deviation from
 * @param sample_percentage Percentage of data to sample (e.g., 0.1 for 0.1%)
 * @param min_sample_threshold Minimum number of samples before falling back to exact calculation
 * @return float The approximate standard deviation
 */
float calculate_std_dev_approximate(AnalogTimeSeries const & series,
                                    float sample_percentage = 0.1f,
                                    size_t min_sample_threshold = 1000);

/**
 * @brief Calculate an approximate standard deviation using adaptive sampling
 *
 * Starts with a small sample and progressively increases until the estimate
 * converges within the specified tolerance or reaches maximum samples.
 *
 * @param series The time series to calculate the standard deviation from
 * @param initial_sample_size Starting number of samples
 * @param max_sample_size Maximum number of samples to use
 * @param convergence_tolerance Relative tolerance for convergence (e.g., 0.01 for 1%)
 * @return float The approximate standard deviation
 */
float calculate_std_dev_adaptive(AnalogTimeSeries const & series,
                                 size_t initial_sample_size = 100,
                                 size_t max_sample_size = 10000,
                                 float convergence_tolerance = 0.01f);

/**
 * @brief Calculate an approximate standard deviation using systematic sampling within a TimeFrameIndex range
 *
 * Uses systematic sampling on the data within the specified time range to estimate standard deviation efficiently.
 * If the sample size would be below the minimum threshold, falls back to exact calculation.
 *
 * @param series The time series to calculate the standard deviation from
 * @param start_time The start TimeFrameIndex (inclusive boundary)
 * @param end_time The end TimeFrameIndex (inclusive boundary)
 * @param sample_percentage Percentage of data to sample (e.g., 0.1 for 0.1%)
 * @param min_sample_threshold Minimum number of samples before falling back to exact calculation
 * @return float The approximate standard deviation in the specified time range
 */
float calculate_std_dev_approximate_in_time_range(AnalogTimeSeries const & series,
                                                  TimeFrameIndex start_time,
                                                  TimeFrameIndex end_time,
                                                  float sample_percentage = 0.1f,
                                                  size_t min_sample_threshold = 1000);

// ========== Minimum ==========

/**
 * @brief Raw minimum calculation implementation using iterators
 * 
 * This is the core implementation that all other minimum functions call.
 * 
 * @param begin Iterator to the start of the data range
 * @param end Iterator to the end of the data range (exclusive)
 * @return float The minimum value of the range
 */
template<typename Iterator>
float calculate_min_impl(Iterator begin, Iterator end) {
    if (begin == end) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    auto const distance = std::distance(begin, end);
    if (distance <= 0) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    return *std::min_element(begin, end);
}

/**
 * @brief Raw minimum calculation implementation using vector with indices
 * 
 * This is an alternative implementation for when you have indices rather than iterators.
 * 
 * @param data Vector containing the data
 * @param start Start index of the range (inclusive)
 * @param end End index of the range (exclusive)
 * @return float The minimum value in the specified range
 */
float calculate_min_impl(std::vector<float> const & data, size_t start, size_t end);

/**
 * @brief Calculate the minimum value of a span of data
 * 
 * @param data_span Span of float data
 * @return float The minimum value
 */
float calculate_min(std::span<float const> data_span);

/**
 * @brief Calculate the minimum value in an AnalogTimeSeries
 *
 * @param series The time series to find the minimum value in
 * @return float The minimum value
 */
float calculate_min(AnalogTimeSeries const & series);

/**
 * @brief Calculate the minimum value in an AnalogTimeSeries in a specific range
 *
 * @param series The time series to find the minimum value in
 * @param start Start index of the range (inclusive)
 * @param end End index of the range (exclusive)
 * @return float The minimum value in the specified range
 */
float calculate_min(AnalogTimeSeries const & series, int64_t start, int64_t end);

/**
 * @brief Calculate the minimum value of an AnalogTimeSeries within a TimeFrameIndex range
 * 
 * This function uses the getDataInTimeFrameIndexRange functionality to efficiently
 * calculate the minimum for data points where TimeFrameIndex >= start_time and <= end_time.
 * It automatically handles boundary approximation if exact times don't exist.
 * 
 * @param series The time series to calculate the minimum from
 * @param start_time The start TimeFrameIndex (inclusive boundary)
 * @param end_time The end TimeFrameIndex (inclusive boundary)
 * @return float The minimum value in the specified time range
 */
float calculate_min_in_time_range(AnalogTimeSeries const & series, TimeFrameIndex start_time, TimeFrameIndex end_time);

// ========== Maximum ==========

/**
 * @brief Raw maximum calculation implementation using iterators
 * 
 * This is the core implementation that all other maximum functions call.
 * 
 * @param begin Iterator to the start of the data range
 * @param end Iterator to the end of the data range (exclusive)
 * @return float The maximum value of the range
 */
template<typename Iterator>
float calculate_max_impl(Iterator begin, Iterator end) {
    if (begin == end) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    auto const distance = std::distance(begin, end);
    if (distance <= 0) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    return *std::max_element(begin, end);
}

/**
 * @brief Raw maximum calculation implementation using vector with indices
 * 
 * This is an alternative implementation for when you have indices rather than iterators.
 * 
 * @param data Vector containing the data
 * @param start Start index of the range (inclusive)
 * @param end End index of the range (exclusive)
 * @return float The maximum value in the specified range
 */
float calculate_max_impl(std::vector<float> const & data, size_t start, size_t end);

/**
 * @brief Calculate the maximum value of a span of data
 * 
 * @param data_span Span of float data
 * @return float The maximum value
 */
float calculate_max(std::span<float const> data_span);

/**
 * @brief Calculate the maximum value in an AnalogTimeSeries
 *
 * @param series The time series to find the maximum value in
 * @return float The maximum value
 */
float calculate_max(AnalogTimeSeries const & series);

/**
 * @brief Calculate the maximum value in an AnalogTimeSeries in a specific range
 *
 * @param series The time series to find the maximum value in
 * @param start Start index of the range (inclusive)
 * @param end End index of the range (exclusive)
 * @return float The maximum value in the specified range
 */
float calculate_max(AnalogTimeSeries const & series, int64_t start, int64_t end);

/**
 * @brief Calculate the maximum value of an AnalogTimeSeries within a TimeFrameIndex range
 * 
 * This function uses the getDataInTimeFrameIndexRange functionality to efficiently
 * calculate the maximum for data points where TimeFrameIndex >= start_time and <= end_time.
 * It automatically handles boundary approximation if exact times don't exist.
 * 
 * @param series The time series to calculate the maximum from
 * @param start_time The start TimeFrameIndex (inclusive boundary)
 * @param end_time The end TimeFrameIndex (inclusive boundary)
 * @return float The maximum value in the specified time range
 */
float calculate_max_in_time_range(AnalogTimeSeries const & series, TimeFrameIndex start_time, TimeFrameIndex end_time);


#endif// ANALOG_TIME_SERIES_STATISTICS_HPP