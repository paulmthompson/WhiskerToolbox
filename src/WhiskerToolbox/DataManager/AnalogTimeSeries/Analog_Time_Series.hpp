#ifndef ANALOG_TIME_SERIES_HPP
#define ANALOG_TIME_SERIES_HPP

#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrameV2.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <vector>
#include <variant>
#include <span>

/**
 * @brief The AnalogTimeSeries class
 *
 * Analog time series is used for storing continuous data
 * The data may be sampled at irregular intervals as long as the time vector is provided
 *
 */
class AnalogTimeSeries : public ObserverData {
public:

    // ========== Constructors ==========
    /**
     * @brief Default constructor
     * 
     * This constructor creates an empty AnalogTimeSeries with no data
     */ 
    AnalogTimeSeries();

    /**
     * @brief Constructor for AnalogTimeSeries from a vector of floats and a vector of TimeFrameIndex values
     * 
     * Use this constructor when the data is sampled at irregular intervals
     * 
     * @param analog_vector Vector of floats
     * @param time_vector Vector of TimeFrameIndex values
     * @see AnalogTimeSeries(std::vector<float> analog_vector, size_t num_samples) 
     * for a constructor that takes a vector of floats that are consecutive samples
     * @see AnalogTimeSeries(std::map<int, float> analog_map) 
     * for a constructor that takes a map of int to float
     */
    explicit AnalogTimeSeries(std::vector<float> analog_vector, std::vector<TimeFrameIndex> time_vector);

    /**
     * @brief Constructor for AnalogTimeSeries from a map of int to float
     * 
     * The key in the map is assumed to the be TimeFrameIndex for each sample
     * 
     * @param analog_map Map of int to float
     * @see AnalogTimeSeries(std::vector<float> analog_vector, std::vector<TimeFrameIndex> time_vector) 
     * for a constructor that takes a vector of floats and a vector of TimeFrameIndex values
     * @see AnalogTimeSeries(std::vector<float> analog_vector, size_t num_samples) 
     * for a constructor that takes a vector of floats that are consecutive samples
     */
    explicit AnalogTimeSeries(std::map<int, float> analog_map);

    /**
     * @brief Constructor for AnalogTimeSeries from a vector of floats and a number of samples
     * 
     * Use this constructor when the data is sampled at regular intervals
     * 
     * @param analog_vector Vector of floats
     * @param num_samples Number of samples
     */
    explicit AnalogTimeSeries(std::vector<float> analog_vector, size_t num_samples);

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
    void overwriteAtTimeIndexes(std::vector<float> & analog_data, std::vector<TimeFrameIndex> & time_indices);
    
    /**
     * @brief Overwrite data at specific DataArrayIndex positions
     * 
     * This function directly overwrites data at the specified DataArrayIndex positions.
     * Bounds checking is performed - indices outside the data array range will be ignored.
     * 
     * @param analog_data Vector of new analog values
     * @param data_indices Vector of DataArrayIndex positions where data should be overwritten
     */
    void overwriteAtDataArrayIndexes(std::vector<float> & analog_data, std::vector<DataArrayIndex> & data_indices);

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
    [[nodiscard]] float getDataAtDataArrayIndex(DataArrayIndex i) const { return _data[i.getValue()]; };

    [[nodiscard]] size_t getNumSamples() const { return _data.size(); };

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
     * @see getDataInRange() for accessing data within a specific time range
     */
    [[nodiscard]] std::vector<float> const & getAnalogTimeSeries() const { return _data; };

    /**
    * @brief Get a span (view) of data values in a time range using any coordinate type
    *
    * This method returns a std::span<const float> over the contiguous block of data
    * corresponding to the given coordinate range. No data is copied.
    *
    * @param start_coord Start coordinate (any coordinate type)
    * @param end_coord End coordinate (any coordinate type)
    * @return std::span<const float> view into the data in the specified range
    * @throws std::runtime_error if no TimeFrameV2 is associated or coordinate types don't match
    * @note The returned span is valid as long as the AnalogTimeSeries is not modified.
    */
    [[nodiscard]] std::span<const float> getDataSpanInCoordinateRange(TimeCoordinate start_coord, 
                                                                      TimeCoordinate end_coord) const;

    //[[nodiscard]] std::span<const float> getDataSpanInTimeFrameIndexRange(TimeFrameIndex start, 
    //                                                                      TimeFrameIndex end) const; 

    auto getDataInRange(float start_time, float stop_time) const {
        struct DataPoint {
            TimeFrameIndex time_idx;
            float value;
        };

        return std::views::iota(size_t{0}, getNumSamples()) |
               std::views::filter([this, start_time, stop_time](size_t i) {
                   auto transformed_time = getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(i)).getValue();
                   return transformed_time >= start_time && transformed_time <= stop_time;
               }) |
               std::views::transform([this](size_t i) {
                   return DataPoint{getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(i)), getDataAtDataArrayIndex(DataArrayIndex(i))};
               });
    }

    template<typename TransformFunc>
    auto getDataInRange(float start_time, float stop_time, TransformFunc time_transform) const {
        struct DataPoint {
            TimeFrameIndex time_idx;
            float value;
        };

        return std::views::iota(size_t{0}, getNumSamples()) |
               std::views::filter([this, start_time, stop_time, time_transform](size_t i) {
                   auto transformed_time = time_transform(getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(i)).getValue());
                   return transformed_time >= start_time && transformed_time <= stop_time;
               }) |
               std::views::transform([this](size_t i) {
                   return DataPoint{getTimeFrameIndexAtDataArrayIndex(DataArrayIndex(i)), getDataAtDataArrayIndex(DataArrayIndex(i))};
               });
    }


    // ========== TimeFrameV2 Support ==========

    /**
    * @brief Set a TimeFrameV2 reference for this data series
    *
    * Associates this AnalogTimeSeries with a strongly-typed TimeFrameV2,
    * enabling type-safe time coordinate operations.
    *
    * @param timeframe The TimeFrameV2 variant to associate with this data
    */
    void setTimeFrameV2(AnyTimeFrame timeframe) {
        _timeframe_v2 = std::move(timeframe);
    }

    /**
    * @brief Get the TimeFrameV2 reference for this data series
    *
    * @return Optional TimeFrameV2 variant if set, nullopt otherwise
    */
    [[nodiscard]] std::optional<AnyTimeFrame> getTimeFrameV2() const {
        return _timeframe_v2;
    }

    /**
    * @brief Check if this series has a TimeFrameV2 reference
    *
    * @return True if a TimeFrameV2 is associated with this series
    */
    [[nodiscard]] bool hasTimeFrameV2() const {
        return _timeframe_v2.has_value();
    }

    /**
    * @brief Get the coordinate type of the associated TimeFrameV2
    *
    * @return String identifier for the coordinate type, or "none" if no TimeFrameV2 is set
    */
    [[nodiscard]] std::string getCoordinateType() const {
        if (!_timeframe_v2.has_value()) {
            return "none";
        }

        return std::visit([](auto const & timeframe_ptr) -> std::string {
            using FrameType = std::decay_t<decltype(*timeframe_ptr)>;
            using CoordType = typename FrameType::coordinate_type;
            
            if constexpr (std::is_same_v<CoordType, ClockTicks>) {
                return "ClockTicks";
            } else if constexpr (std::is_same_v<CoordType, CameraFrameIndex>) {
                return "CameraFrameIndex";
            } else if constexpr (std::is_same_v<CoordType, Seconds>) {
                return "Seconds";
            } else if constexpr (std::is_same_v<CoordType, UncalibratedIndex>) {
                return "UncalibratedIndex";
            } else {
                return "unknown";
            }
        }, _timeframe_v2.value());
    }

    /**
    * @brief Check if the TimeFrameV2 uses a specific coordinate type
    *
    * @tparam CoordinateType The coordinate type to check for
    * @return True if the TimeFrameV2 uses the specified coordinate type
    */
    template<typename CoordinateType>
    [[nodiscard]] bool hasCoordinateType() const {
        if (!_timeframe_v2.has_value()) {
            return false;
        }

        return std::visit([](auto const & timeframe_ptr) -> bool {
            using FrameType = std::decay_t<decltype(*timeframe_ptr)>;
            using FrameCoordType = typename FrameType::coordinate_type;
            return std::is_same_v<FrameCoordType, CoordinateType>;
        }, _timeframe_v2.value());
    }

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
    [[nodiscard]] std::optional<DataArrayIndex> findDataArrayIndexForTimeFrameIndex(TimeFrameIndex time_index) const;

    /**
     * @brief Find the DataArrayIndex for the smallest TimeFrameIndex >= target_time
     * 
     * This function finds the first data point where TimeFrameIndex >= target_time.
     * Useful for finding the start boundary of a time range when the exact time may not exist.
     * 
     * @param target_time The target TimeFrameIndex
     * @return std::optional<DataArrayIndex> containing the DataArrayIndex of the first TimeFrameIndex >= target_time, or std::nullopt if no such index exists
     */
    [[nodiscard]] std::optional<DataArrayIndex> findDataArrayIndexGreaterOrEqual(TimeFrameIndex target_time) const;

    /**
     * @brief Find the DataArrayIndex for the largest TimeFrameIndex <= target_time
     * 
     * This function finds the last data point where TimeFrameIndex <= target_time.
     * Useful for finding the end boundary of a time range when the exact time may not exist.
     * 
     * @param target_time The target TimeFrameIndex
     * @return std::optional<DataArrayIndex> containing the DataArrayIndex of the last TimeFrameIndex <= target_time, or std::nullopt if no such index exists
     */
    [[nodiscard]] std::optional<DataArrayIndex> findDataArrayIndexLessOrEqual(TimeFrameIndex target_time) const;

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
     * @note The span is valid as long as the AnalogTimeSeries object exists and is not modified
     * @see findDataArrayIndexGreaterOrEqual() and findDataArrayIndexLessOrEqual() for the underlying boundary logic
     */
    [[nodiscard]] std::span<const float> getDataInTimeFrameIndexRange(TimeFrameIndex start_time, TimeFrameIndex end_time) const;

    // ========== Time-Value Range Access ==========

    /**
     * @brief Data structure representing a single time-value point
     */
    struct TimeValuePoint {
        TimeFrameIndex time_frame_index{TimeFrameIndex(0)};
        float value{0.0f};
        
        TimeValuePoint() = default;
        TimeValuePoint(TimeFrameIndex time_idx, float val) : time_frame_index(time_idx), value(val) {}
    };

    /**
     * @brief Iterator for time-value ranges that handles both dense and sparse storage efficiently
     */
    class TimeValueRangeIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = TimeValuePoint;
        using difference_type = std::ptrdiff_t;
        using pointer = const TimeValuePoint*;
        using reference = const TimeValuePoint&;

        TimeValueRangeIterator(AnalogTimeSeries const* series, DataArrayIndex start_index, DataArrayIndex end_index, bool is_end = false);

        reference operator*() const;
        pointer operator->() const;
        TimeValueRangeIterator& operator++();
        TimeValueRangeIterator operator++(int);
        bool operator==(TimeValueRangeIterator const& other) const;
        bool operator!=(TimeValueRangeIterator const& other) const;

    private:
        AnalogTimeSeries const* _series;
        DataArrayIndex _current_index;
        DataArrayIndex _end_index;
        mutable TimeValuePoint _current_point; // mutable for lazy evaluation in operator*
        bool _is_end;

        void _updateCurrentPoint() const;
    };

    /**
     * @brief Range view for time-value pairs that supports range-based for loops
     */
    class TimeValueRangeView {
    public:
        TimeValueRangeView(AnalogTimeSeries const* series, DataArrayIndex start_index, DataArrayIndex end_index);

        TimeValueRangeIterator begin() const;
        TimeValueRangeIterator end() const;
        size_t size() const;
        bool empty() const;

    private:
        AnalogTimeSeries const* _series;
        DataArrayIndex _start_index;
        DataArrayIndex _end_index;
    };

    /**
     * @brief Abstract base class for time index iteration over ranges
     */
    class TimeIndexIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = TimeFrameIndex;
        using difference_type = std::ptrdiff_t;
        using pointer = const TimeFrameIndex*;
        using reference = const TimeFrameIndex&;

        virtual ~TimeIndexIterator() = default;
        virtual reference operator*() const = 0;
        virtual TimeIndexIterator& operator++() = 0;
        virtual bool operator==(TimeIndexIterator const& other) const = 0;
        virtual bool operator!=(TimeIndexIterator const& other) const = 0;
        virtual std::unique_ptr<TimeIndexIterator> clone() const = 0;
    };

    /**
     * @brief Time index range abstraction that handles both dense and sparse storage
     */
    class TimeIndexRange {
    public:
        TimeIndexRange(AnalogTimeSeries const* series, DataArrayIndex start_index, DataArrayIndex end_index);

        std::unique_ptr<TimeIndexIterator> begin() const;
        std::unique_ptr<TimeIndexIterator> end() const;
        size_t size() const;
        bool empty() const;

    private:
        AnalogTimeSeries const* _series;
        DataArrayIndex _start_index;
        DataArrayIndex _end_index;
    };

    /**
     * @brief Paired span and time iterator for zero-copy time-value access
     */
    struct TimeValueSpanPair {
        std::span<const float> values;
        TimeIndexRange time_indices;

        TimeValueSpanPair(std::span<const float> data_span, AnalogTimeSeries const* series, DataArrayIndex start_index, DataArrayIndex end_index);
    };

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
    [[nodiscard]] TimeValueRangeView getTimeValueRangeInTimeFrameIndexRange(TimeFrameIndex start_time, TimeFrameIndex end_time) const;

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
    [[nodiscard]] TimeValueSpanPair getTimeValueSpanInTimeFrameIndexRange(TimeFrameIndex start_time, TimeFrameIndex end_time) const;


    /**
     * @brief Get the TimeFrameIndex that corresponds to a given DataArrayIndex
     * 
     * @param i The DataArrayIndex to get the TimeFrameIndex for
     * @return The TimeFrameIndex that corresponds to the given DataArrayIndex
     */
    [[nodiscard]] TimeFrameIndex getTimeFrameIndexAtDataArrayIndex(DataArrayIndex i) const {
        return std::visit([i](auto const & time_storage) -> TimeFrameIndex {
            return time_storage.getTimeFrameIndexAtDataArrayIndex(i);
        }, _time_storage);
    }

      /**
     * @brief Get the time indices as a vector
     * 
     * Returns a vector containing the time indices corresponding to each analog data sample.
     * For dense time storage, this generates the indices on-demand.
     * For sparse time storage, this returns a copy of the stored indices.
     * 
     * @return std::vector<size_t> containing the time indices
     * 
     * @note For dense storage, this method has O(n) time complexity as it generates the vector.
     *       Consider using getTimeAtIndex() for single lookups or iterating with getNumSamples().
     *       For sparse storage, this returns a copy of the internal vector.
     * 
     * @see getAnalogTimeSeries() for accessing the corresponding data values
     * @see getDataInRange() for accessing time-value pairs within a specific range
     * @see getTimeAtIndex() for single index lookups
     */
    [[nodiscard]] std::vector<TimeFrameIndex> getTimeSeries() const {
        return std::visit([](auto const & time_storage) -> std::vector<TimeFrameIndex> {
            if constexpr (std::is_same_v<std::decay_t<decltype(time_storage)>, DenseTimeRange>) {
                // Generate vector for dense storage
                std::vector<TimeFrameIndex> result;
                result.reserve(time_storage.count);
                for (size_t i = 0; i < time_storage.count; ++i) {
                    result.push_back(time_storage.start_time_frame_index + TimeFrameIndex(static_cast<int64_t>(i)));
                }
                return result;
            } else {
                // Return copy for sparse storage
                return time_storage.time_frame_indices;
            }
        }, _time_storage);
    }

    // ========== Time Storage Optimization ==========
    
    /**
     * @brief Dense time representation for regularly sampled data
     * 
     * More memory efficient than storing every index individually.
     * Represents: start_index, start_index+1, start_index+2, ..., start_index+count-1
     * 
     * Now strongly typed: indexed by DataArrayIndex, returns TimeFrameIndex
     */
    struct DenseTimeRange {
        TimeFrameIndex start_time_frame_index;
        size_t count;
        
        DenseTimeRange(TimeFrameIndex start, size_t num_samples) 
            : start_time_frame_index(start), count(num_samples) {}
            
        [[nodiscard]] TimeFrameIndex getTimeFrameIndexAtDataArrayIndex(DataArrayIndex i) const {
            if (i.getValue() >= count) {
                throw std::out_of_range("DataArrayIndex out of range for DenseTimeRange");
            }
            return TimeFrameIndex(start_time_frame_index.getValue() + static_cast<int64_t>(i.getValue()));
        }
        
        [[nodiscard]] size_t size() const { return count; }
    };
    
    /**
     * @brief Sparse time representation for irregularly sampled data
     * 
     * Stores explicit time indices for each sample.
     * 
     * Now strongly typed: indexed by DataArrayIndex, returns TimeFrameIndex
     */
    struct SparseTimeIndices {
        std::vector<TimeFrameIndex> time_frame_indices;
        
        explicit SparseTimeIndices(std::vector<TimeFrameIndex> time_indices) 
            : time_frame_indices(std::move(time_indices)) {}
            
        [[nodiscard]] TimeFrameIndex getTimeFrameIndexAtDataArrayIndex(DataArrayIndex i) const {
            if (i.getValue() >= time_frame_indices.size()) {
                throw std::out_of_range("DataArrayIndex out of range for SparseTimeIndices");
            }
            return time_frame_indices[i.getValue()];
        }

        [[nodiscard]] size_t size() const { return time_frame_indices.size(); }
    };
    
    using TimeStorage = std::variant<DenseTimeRange, SparseTimeIndices>;

    /**
    * @brief Get a const reference to the time storage variant (DenseTimeRange or SparseTimeIndices)
    *
    * This allows efficient access to the underlying time mapping without copying.
    *
    * @return const TimeStorage& (std::variant<DenseTimeRange, SparseTimeIndices>)
    */
    const TimeStorage& getTimeStorage() const noexcept { return _time_storage; }

protected:
private:
    std::vector<float> _data;
    TimeStorage _time_storage;

    // New TimeFrameV2 support
    std::optional<AnyTimeFrame> _timeframe_v2;

    void setData(std::vector<float> analog_vector);
    void setData(std::vector<float> analog_vector, std::vector<TimeFrameIndex> time_vector);
    void setData(std::map<int, float> analog_map);
};

#endif// ANALOG_TIME_SERIES_HPP