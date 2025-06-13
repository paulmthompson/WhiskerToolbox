#ifndef ANALOG_TIME_SERIES_STATISTICS_HPP
#define ANALOG_TIME_SERIES_STATISTICS_HPP

class AnalogTimeSeries;

#include <cstdint>
#include <cstddef>

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




#endif // ANALOG_TIME_SERIES_STATISTICS_HPP