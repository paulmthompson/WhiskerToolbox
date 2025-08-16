#ifndef DATAMANAGER_DATA_AGGREGATION_HPP
#define DATAMANAGER_DATA_AGGREGATION_HPP

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TimeFrame/interval_data.hpp"
#include "Points/Point_Data.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

/**
 * @file DataAggregation.hpp
 * @brief Data aggregation functionality for interval-based analysis
 * 
 * This module provides functionality to aggregate data across time intervals in spreadsheet format.
 * Each row corresponds to an interval from a "row interval series", and each column represents
 * a transformation applied to that interval or related reference data.
 * 
 * @section Usage Example
 * @code
 * // Define row intervals (e.g., Interval_Foo)
 * std::vector<Interval> row_intervals = {{100, 200}, {240, 500}, {700, 900}};
 * 
 * // Define reference intervals (e.g., Interval_Bar) 
 * std::vector<Interval> ref_intervals = {{40, 550}, {650, 1000}};
 * std::map<std::string, std::vector<Interval>> reference_intervals = {{"interval_bar", ref_intervals}};
 * 
 * // Define reference analog data (e.g., sensor readings)
 * auto analog_data = std::make_shared<AnalogTimeSeries>(std::vector<float>{1.0f, 2.0f, 3.0f, 4.0f, 5.0f});
 * std::map<std::string, std::shared_ptr<AnalogTimeSeries>> reference_analog = {{"sensor", analog_data}};
 * 
 * // Define reference point data (e.g., tracked features)
 * auto point_data = std::make_shared<PointData>();
 * std::map<std::string, std::shared_ptr<PointData>> reference_points = {{"features", point_data}};
 * 
 * // Configure transformations for columns
 * std::vector<TransformationConfig> transformations = {
 *     {TransformationType::IntervalStart, "start_time"},
 *     {TransformationType::IntervalEnd, "end_time"},
 *     {TransformationType::IntervalID, "bar_id", "interval_bar", OverlapStrategy::First},
 *     {TransformationType::IntervalCount, "bar_count", "interval_bar"},
 *     {TransformationType::AnalogMean, "sensor_mean", "sensor"},
 *     {TransformationType::AnalogMax, "sensor_max", "sensor"},
 *     {TransformationType::PointMeanX, "feature_x_mean", "features"},
 *     {TransformationType::PointMeanY, "feature_y_mean", "features"}
 * };
 * 
 * // Generate aggregated data
 * auto result = aggregateData(row_intervals, transformations, reference_intervals, reference_analog, reference_points);
 * 
 * // Result will be a 3x8 matrix with interval, analog, and point statistics for each row interval
 * @endcode
 */
namespace DataAggregation {

/**
 * @brief Available transformation types for interval data
 */
enum class TransformationType {
    // Interval-based transformations
    IntervalStart,   // Start time of the interval
    IntervalEnd,     // End time of the interval
    IntervalDuration,// Duration of the interval (end - start + 1)
    IntervalID,      // ID of overlapping interval from reference data
    IntervalCount,   // Count of overlapping intervals from reference data

    // Analog time series transformations
    AnalogMean, // Mean value of analog data within the interval
    AnalogMin,  // Minimum value of analog data within the interval
    AnalogMax,  // Maximum value of analog data within the interval
    AnalogStdDev,// Standard deviation of analog data within the interval
    
    // Point data transformations
    PointMeanX, // Mean X coordinate of points within the interval
    PointMeanY  // Mean Y coordinate of points within the interval
};

/**
 * @brief Strategies for handling overlapping intervals in IntervalID transformations
 */
enum class OverlapStrategy {
    First,    // Take the first overlapping interval (lowest index)
    Last,     // Take the last overlapping interval (highest index)
    MaxOverlap// Take the interval with maximum overlap duration
};

/**
 * @brief Configuration for a single transformation column
 */
struct TransformationConfig {
    TransformationType type;
    std::string column_name;
    std::string reference_data_key;                           // Used for IntervalID and IntervalCount transformations
    OverlapStrategy overlap_strategy = OverlapStrategy::First;// Only used for IntervalID

    TransformationConfig(TransformationType t, std::string name)
        : type(t),
          column_name(std::move(name)) {}

    TransformationConfig(TransformationType t, std::string name,
                         std::string ref_key, OverlapStrategy strategy = OverlapStrategy::First)
        : type(t),
          column_name(std::move(name)),
          reference_data_key(std::move(ref_key)),
          overlap_strategy(strategy) {}
};

/**
 * @brief Calculate overlap duration between two intervals
 * @param a First interval
 * @param b Second interval  
 * @return Overlap duration, 0 if no overlap
 */
int64_t calculateOverlapDuration(Interval const & a, Interval const & b);

/**
 * @brief Find overlapping intervals and return their indices
 * @param target_interval The interval to find overlaps for
 * @param reference_intervals The intervals to search for overlaps
 * @param strategy Strategy for handling multiple overlaps
 * @return Index of the overlapping interval, or -1 if no overlap
 */
int findOverlappingIntervalIndex(Interval const & target_interval,
                                 std::vector<Interval> const & reference_intervals,
                                 OverlapStrategy strategy);

/**
 * @brief Apply a single transformation to an interval
 * @param interval The interval to transform
 * @param config The transformation configuration
 * @param reference_intervals Map of reference interval data for IntervalID/IntervalCount transformations
 * @param reference_analog Map of reference analog data for analog transformations
 * @param reference_points Map of reference point data for point transformations
 * @return The transformed value (NaN for invalid/no overlap cases)
 */
double applyTransformation(Interval const & interval,
                           TransformationConfig const & config,
                           std::map<std::string, std::vector<Interval>> const & reference_intervals,
                           std::map<std::string, std::shared_ptr<AnalogTimeSeries>> const & reference_analog,
                           std::map<std::string, std::shared_ptr<PointData>> const & reference_points);

/**
 * @brief Aggregate data according to transformation configurations
 * @param row_intervals The intervals that define the rows
 * @param transformations Vector of transformation configurations for columns
 * @param reference_intervals Map of reference interval data for IntervalID/IntervalCount transformations
 * @param reference_analog Map of reference analog data for analog transformations
 * @param reference_points Map of reference point data for point transformations
 * @return 2D vector where result[row][col] contains the aggregated value
 */
std::vector<std::vector<double>> aggregateData(
        std::vector<Interval> const & row_intervals,
        std::vector<TransformationConfig> const & transformations,
        std::map<std::string, std::vector<Interval>> const & reference_intervals = {},
        std::map<std::string, std::shared_ptr<AnalogTimeSeries>> const & reference_analog = {},
        std::map<std::string, std::shared_ptr<PointData>> const & reference_points = {});


}// namespace DataAggregation

#endif// DATAMANAGER_DATA_AGGREGATION_HPP