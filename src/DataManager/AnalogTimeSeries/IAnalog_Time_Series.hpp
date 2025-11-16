#ifndef IANALOG_TIME_SERIES_HPP
#define IANALOG_TIME_SERIES_HPP

#include "Observer/Observer_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

/**
 * @brief Interface for analog time series data structures
 *
 * This interface defines the contract for storing and accessing continuous analog data
 * that may be sampled at regular or irregular intervals. Different implementations
 * can provide various storage strategies (e.g., in-memory, memory-mapped files).
 *
 * The interface provides methods for:
 * - Accessing data by DataArrayIndex or TimeFrameIndex
 * - Finding boundaries in time ranges
 * - Iterating over time-value pairs
 * - Managing time frame associations
 */
class IAnalogTimeSeries : public ObserverData {
public:
    virtual ~IAnalogTimeSeries() = default;

    // ========== Overwriting Data ==========
    
    /**
     * @brief Overwrite data at specific TimeFrameIndex values
     * 
     * This function finds DataArrayIndex positions that correspond to the given TimeFrameIndex values
     * and overwrites the data at those positions. If a TimeFrameIndex doesn't exist in the series,
     * it will be ignored (no overwrite occurs).
     * 
     * @param analog_data Vector of new analog values
     * @param time_indices Vector of TimeFrameIndex values where data should be overwritten
     */
    virtual void overwriteAtTimeIndexes(std::vector<float> & analog_data, std::vector<TimeFrameIndex> & time_indices) = 0;

    /**
     * @brief Overwrite data at specific DataArrayIndex positions
     * 
     * This function directly overwrites data at the specified DataArrayIndex positions.
     * Bounds checking is performed - indices outside the data array range will be ignored.
     * 
     * @param analog_data Vector of new analog values
     * @param data_indices Vector of DataArrayIndex positions where data should be overwritten
     */
    virtual void overwriteAtDataArrayIndexes(std::vector<float> & analog_data, std::vector<DataArrayIndex> & data_indices) = 0;

    // ========== Getting Data ==========

    /**
     * @brief Get the data value at a specific DataArrayIndex
     * 
     * This does not consider time information so DataArrayIndex 1 and 2 may represent 
     * values at are irregularly spaced. Use this if you are processing data
     * where the time information is not important (e.g. statistical calculations)
     * 
     * @param i The DataArrayIndex to get the data value at
     * @return The data value at the specified DataArrayIndex
     */
    [[nodiscard]] virtual float getDataAtDataArrayIndex(DataArrayIndex i) const = 0;

    /**
     * @brief Get the number of samples in the time series
     * 
     * @return The total number of data points
     */
    [[nodiscard]] virtual size_t getNumSamples() const = 0;

    /**
     * @brief Get a const reference to the analog data vector
     * 
     * Returns a const reference to the internal vector containing the analog time series data values.
     * This method provides efficient read-only access to the data without copying.
     * 
     * @return const reference to std::vector<float> containing the analog data values
     * 
     * @note This method returns by const reference for performance - no data copying occurs.
     *       Use this when you need to iterate over or access the raw data values efficiently.
     * 
     * @see getTimeSeries() for accessing the corresponding time indices
     * @see getDataInTimeFrameIndexRange() for accessing data within a specific time range
     */
    [[nodiscard]] virtual std::vector<float> const & getAnalogTimeSeries() const = 0;

    /**
     * @brief Get a span (view) of data values within a TimeFrameIndex range
     * 
     * This function returns a std::span over the data array for all points where 
     * TimeFrameIndex >= start_time and TimeFrameIndex <= end_time. The span provides
     * a zero-copy view into the underlying data.
     * 
     * If the exact start_time or end_time don't exist in the series, it finds the closest available times:
     * - For start: finds the smallest TimeFrameIndex >= start_time  
     * - For end: finds the largest TimeFrameIndex <= end_time
     * 
     * @param start_time The start time (inclusive boundary)
     * @param end_time The end time (inclusive boundary)
     * @return std::span<const float> view over the data in the specified range
     * 
     * @note Returns an empty span if no data points fall within the specified range
     * @note The span is valid as long as the IAnalogTimeSeries object exists and is not modified
     * @see findDataArrayIndexGreaterOrEqual() and findDataArrayIndexLessOrEqual() for the underlying boundary logic
     */
    [[nodiscard]] virtual std::span<float const> getDataInTimeFrameIndexRange(TimeFrameIndex start_time, TimeFrameIndex end_time) const = 0;

    /**
     * @brief Get a span (view) of data values within a TimeFrameIndex range, with timeframe conversion
     *
     * This function returns a std::span over the data array for all points where
     * TimeFrameIndex >= start_time and TimeFrameIndex <= end_time. The span provides
     * a zero-copy view into the underlying data.
     *
     * If the exact start_time or end_time don't exist in the series, it finds the closest available times:
     * - For start: finds the smallest TimeFrameIndex >= start_time
     * - For end: finds the largest TimeFrameIndex <= end_time
     *
     * @param start_time The start time (inclusive boundary)
     * @param end_time The end time (inclusive boundary)
     * @param source_timeFrame The timeframe that the start and end times are expressed in
     * @return std::span<const float> view over the data in the specified range
     *
     * @note Returns an empty span if no data points fall within the specified range
     * @note The span is valid as long as the IAnalogTimeSeries object exists and is not modified
     * @see findDataArrayIndexGreaterOrEqual() and findDataArrayIndexLessOrEqual() for the underlying boundary logic
     */
    [[nodiscard]] virtual std::span<float const> getDataInTimeFrameIndexRange(TimeFrameIndex start_time,
                                                                              TimeFrameIndex end_time,
                                                                              TimeFrame const * source_timeFrame) const = 0;

    /**
     * @brief Find the DataArrayIndex that corresponds to a given TimeFrameIndex
     * 
     * This function searches for the DataArrayIndex position that corresponds to the given TimeFrameIndex.
     * For dense storage, it calculates the position if the TimeFrameIndex falls within the range.
     * For sparse storage, it searches for the TimeFrameIndex in the stored indices.
     * 
     * @param time_index The TimeFrameIndex to search for
     * @return std::optional<DataArrayIndex> containing the corresponding DataArrayIndex, or std::nullopt if not found
     */
    [[nodiscard]] virtual std::optional<DataArrayIndex> findDataArrayIndexForTimeFrameIndex(TimeFrameIndex time_index) const = 0;

    /**
     * @brief Find the DataArrayIndex for the smallest TimeFrameIndex >= target_time
     * 
     * This function finds the first data point where TimeFrameIndex >= target_time.
     * Useful for finding the start boundary of a time range when the exact time may not exist.
     * 
     * @param target_time The target TimeFrameIndex
     * @return std::optional<DataArrayIndex> containing the DataArrayIndex of the first TimeFrameIndex >= target_time, or std::nullopt if no such index exists
     */
    [[nodiscard]] virtual std::optional<DataArrayIndex> findDataArrayIndexGreaterOrEqual(TimeFrameIndex target_time) const = 0;

    /**
     * @brief Find the DataArrayIndex for the largest TimeFrameIndex <= target_time
     * 
     * This function finds the last data point where TimeFrameIndex <= target_time.
     * Useful for finding the end boundary of a time range when the exact time may not exist.
     * 
     * @param target_time The target TimeFrameIndex
     * @return std::optional<DataArrayIndex> containing the DataArrayIndex of the last TimeFrameIndex <= target_time, or std::nullopt if no such index exists
     */
    [[nodiscard]] virtual std::optional<DataArrayIndex> findDataArrayIndexLessOrEqual(TimeFrameIndex target_time) const = 0;

    // ========== Time-Value Range Access ==========

    /**
     * @brief Data structure representing a single time-value point
     */
    struct TimeValuePoint {
        TimeFrameIndex time_frame_index{TimeFrameIndex(0)};
        float value{0.0f};

        TimeValuePoint() = default;
        TimeValuePoint(TimeFrameIndex time_idx, float val)
            : time_frame_index(time_idx),
              value(val) {}
    };

    // Forward declarations for nested types
    class TimeValueRangeIterator;
    class TimeValueRangeView;
    class TimeIndexIterator;
    class TimeIndexRange;
    struct TimeValueSpanPair;

    /**
     * @brief Get time-value pairs as a range for convenient iteration
     * 
     * Returns a range view that can be used with range-based for loops to iterate over
     * time-value pairs within the specified TimeFrameIndex range. This is the high-level,
     * convenient interface.
     * 
     * @param start_time The start TimeFrameIndex (inclusive boundary)
     * @param end_time The end TimeFrameIndex (inclusive boundary)
     * @return TimeValueRangeView that supports range-based for loops
     * 
     * @note Uses the same boundary logic as getDataInTimeFrameIndexRange()
     * @see getTimeValueSpanInTimeFrameIndexRange() for zero-copy alternative
     */
    [[nodiscard]] virtual TimeValueRangeView getTimeValueRangeInTimeFrameIndexRange(TimeFrameIndex start_time, TimeFrameIndex end_time) const = 0;

    /**
     * @brief Get time-value pairs as span and time iterator for zero-copy access
     * 
     * Returns a structure containing a zero-copy span over the data values and a
     * time iterator for the corresponding TimeFrameIndex values. This is the 
     * performance-optimized interface.
     * 
     * @param start_time The start TimeFrameIndex (inclusive boundary)
     * @param end_time The end TimeFrameIndex (inclusive boundary)
     * @return TimeValueSpanPair with zero-copy data access
     * 
     * @note Uses the same boundary logic as getDataInTimeFrameIndexRange()
     * @see getTimeValueRangeInTimeFrameIndexRange() for convenient range-based alternative
     */
    [[nodiscard]] virtual TimeValueSpanPair getTimeValueSpanInTimeFrameIndexRange(TimeFrameIndex start_time, TimeFrameIndex end_time) const = 0;

    /**
     * @brief Get time-value pairs with timeframe conversion
     * 
     * Similar to getTimeValueSpanInTimeFrameIndexRange, but accepts a source timeframe
     * to convert the start and end time indices from the source coordinate system to
     * the analog time series coordinate system.
     * 
     * @param start_time The start TimeFrameIndex in source timeframe coordinates
     * @param end_time The end TimeFrameIndex in source timeframe coordinates
     * @param source_timeFrame The timeframe that start_time and end_time are expressed in
     * @return TimeValueSpanPair with zero-copy data access
     * 
     * @note If source_timeFrame equals the analog series' timeframe, or if either is null,
     *       falls back to the non-converting version
     */
    [[nodiscard]] virtual TimeValueSpanPair getTimeValueSpanInTimeFrameIndexRange(TimeFrameIndex start_time, 
                                                                                  TimeFrameIndex end_time,
                                                                                  TimeFrame const * source_timeFrame) const = 0;

    /**
     * @brief Get the TimeFrameIndex that corresponds to a given DataArrayIndex
     * 
     * @param i The DataArrayIndex to get the TimeFrameIndex for
     * @return The TimeFrameIndex that corresponds to the given DataArrayIndex
     */
    [[nodiscard]] virtual TimeFrameIndex getTimeFrameIndexAtDataArrayIndex(DataArrayIndex i) const = 0;

    /**
     * @brief Get the time indices as a vector
     * 
     * Returns a vector containing the time indices corresponding to each analog data sample.
     * For dense time storage, this generates the indices on-demand.
     * For sparse time storage, this returns a copy of the stored indices.
     * 
     * @return std::vector<TimeFrameIndex> containing the time indices
     * 
     * @note For dense storage, this method has O(n) time complexity as it generates the vector.
     *       Consider using getTimeFrameIndexAtDataArrayIndex() for single lookups or iterating with getNumSamples().
     *       For sparse storage, this returns a copy of the internal vector.
     * 
     * @see getAnalogTimeSeries() for accessing the corresponding data values
     * @see getTimeValueRangeInTimeFrameIndexRange() for accessing time-value pairs within a specific range
     * @see getTimeFrameIndexAtDataArrayIndex() for single index lookups
     */
    [[nodiscard]] virtual std::vector<TimeFrameIndex> getTimeSeries() const = 0;

    // ========== Time Frame ==========
    /**
     * @brief Set the time frame
     * 
     * @param time_frame The time frame to set
     */
    virtual void setTimeFrame(std::shared_ptr<TimeFrame> time_frame) = 0;

protected:
    IAnalogTimeSeries() = default;
};

#endif// IANALOG_TIME_SERIES_HPP
