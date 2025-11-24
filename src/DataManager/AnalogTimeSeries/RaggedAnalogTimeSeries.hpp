#ifndef RAGGED_ANALOG_TIME_SERIES_HPP
#define RAGGED_ANALOG_TIME_SERIES_HPP

#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TypeTraits/DataTypeTraits.hpp"

#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <vector>

class RaggedAnalogTimeSeriesView;

/**
 * @brief Ragged analog time series data structure.
 *
 * RaggedAnalogTimeSeries stores time series data where each TimeFrameIndex
 * can have a variable-length vector of float values. Unlike regular AnalogTimeSeries,
 * the number of samples at each time point need not be constant.
 *
 * This class manages:
 * - Time series storage as a map from TimeFrameIndex to vectors of floats
 * - TimeFrame association for time-based operations
 * - Observer pattern integration for data change notifications
 * - Range-based iteration interface
 *
 * Use cases include:
 * - Spike trains with varying numbers of detected events per time bin
 * - Multi-unit recordings where channel count varies over time
 * - Feature vectors with time-varying dimensionality
 */
class RaggedAnalogTimeSeries : public ObserverData {
public:
    // ========== Type Traits ==========
    /**
     * @brief Type traits for RaggedAnalogTimeSeries
     * 
     * Defines compile-time properties of RaggedAnalogTimeSeries for use in generic algorithms
     * and the transformation system.
     */
    struct DataTraits : WhiskerToolbox::TypeTraits::DataTypeTraitsBase<RaggedAnalogTimeSeries, float> {
        static constexpr bool is_ragged = true;
        static constexpr bool is_temporal = true;
        static constexpr bool has_entity_ids = false;
        static constexpr bool is_spatial = false;
    };

    // ========== Constructors ==========
    RaggedAnalogTimeSeries() = default;
    virtual ~RaggedAnalogTimeSeries() = default;

    /**
     * @brief Construct from a range of time-value pairs
     * 
     * Accepts any input range that yields pairs of (TimeFrameIndex, values) where values
     * can be either std::vector<float> or std::span<float const> or individual floats.
     * This enables efficient construction from transformed views.
     * 
     * @tparam R Range type (must satisfy std::ranges::input_range)
     * @param time_value_pairs Range of (TimeFrameIndex, values) pairs
     * 
     * @example
     * ```cpp
     * auto transformed = mask_data.elements() 
     *     | std::views::transform([](auto entry) { 
     *         return std::pair{entry.time, calculateMaskArea(entry.data)};
     *     });
     * RaggedAnalogTimeSeries ragged(transformed);
     * ```
     */
    template<std::ranges::input_range R>
    requires requires(std::ranges::range_value_t<R> pair) {
        { pair.first } -> std::convertible_to<TimeFrameIndex>;
        { pair.second };  // Can be float, vector<float>, or span<float const>
    }
    explicit RaggedAnalogTimeSeries(R&& time_value_pairs) {
        for (auto&& [time, values] : time_value_pairs) {
            // Handle different value types
            if constexpr (std::convertible_to<decltype(values), float>) {
                // Single float value - append to existing data at this time
                _data[time].push_back(static_cast<float>(values));
            }
            else if constexpr (std::convertible_to<decltype(values), std::vector<float>>) {
                // Vector of floats
                if (_data.find(time) == _data.end()) {
                    // No existing data at this time - move or copy the entire vector
                    if constexpr (std::is_rvalue_reference_v<decltype(values)>) {
                        _data[time] = std::move(values);
                    } else {
                        _data[time] = values;
                    }
                } else {
                    // Existing data at this time - append
                    _data[time].insert(_data[time].end(), values.begin(), values.end());
                }
            }
            else if constexpr (std::ranges::range<decltype(values)>) {
                // Span or other range type - append to existing data
                if (_data.find(time) == _data.end()) {
                    _data[time] = std::vector<float>(std::ranges::begin(values), std::ranges::end(values));
                } else {
                    _data[time].insert(_data[time].end(), std::ranges::begin(values), std::ranges::end(values));
                }
            }
        }
    }

    // Delete copy constructor and copy assignment
    RaggedAnalogTimeSeries(RaggedAnalogTimeSeries const &) = delete;
    RaggedAnalogTimeSeries & operator=(RaggedAnalogTimeSeries const &) = delete;

    // Enable move semantics
    RaggedAnalogTimeSeries(RaggedAnalogTimeSeries &&) = default;
    RaggedAnalogTimeSeries & operator=(RaggedAnalogTimeSeries &&) = default;

    // ========== Time Frame ==========
    /**
     * @brief Set the time frame for this data structure
     * 
     * @param time_frame Shared pointer to the TimeFrame to associate with this data
     */
    void setTimeFrame(std::shared_ptr<TimeFrame> time_frame);

    /**
     * @brief Get the current time frame
     * 
     * @return Shared pointer to the associated TimeFrame, or nullptr if none is set
     */
    [[nodiscard]] std::shared_ptr<TimeFrame> getTimeFrame() const;

    // ========== Data Access ==========

    /**
     * @brief Get the data at a specific time
     * 
     * Returns a span view over the float vector at the specified time.
     * 
     * @param time The time to query
     * @return std::span<float const> view over the data, or empty span if no data exists
     */
    [[nodiscard]] std::span<float const> getDataAtTime(TimeFrameIndex time) const;

    /**
     * @brief Get the data at a specific time with timeframe conversion
     * 
     * @param time_index_and_frame The time and timeframe to query
     * @return std::span<float const> view over the data, or empty span if no data exists
     */
    [[nodiscard]] std::span<float const> getDataAtTime(TimeIndexAndFrame const & time_index_and_frame) const;

    /**
     * @brief Check if data exists at a specific time
     * 
     * @param time The time to check
     * @return true if data exists at the given time, false otherwise
     */
    [[nodiscard]] bool hasDataAtTime(TimeFrameIndex time) const;

    /**
     * @brief Get the number of values at a specific time
     * 
     * @param time The time to query
     * @return The number of float values at the given time, or 0 if no data exists
     */
    [[nodiscard]] size_t getCountAtTime(TimeFrameIndex time) const;

    /**
     * @brief Get all time indices that have data
     * 
     * @return Vector of TimeFrameIndex values where data exists
     */
    [[nodiscard]] std::vector<TimeFrameIndex> getTimeIndices() const;

    /**
     * @brief Get the total number of time points with data
     * 
     * @return The number of time indices that have data
     */
    [[nodiscard]] size_t getNumTimePoints() const;

    // ========== Data Modification ==========

    /**
     * @brief Add or replace data at a specific time (by copying)
     * 
     * If data already exists at the given time, it will be replaced.
     * 
     * @param time The time to add the data at
     * @param data The vector of float values to add (will be copied)
     * @param notify Whether to notify observers after the operation
     */
    void setDataAtTime(TimeFrameIndex time, std::vector<float> const & data, NotifyObservers notify);

    /**
     * @brief Add or replace data at a specific time (by moving)
     * 
     * If data already exists at the given time, it will be replaced.
     * 
     * @param time The time to add the data at
     * @param data The vector of float values to add (will be moved)
     * @param notify Whether to notify observers after the operation
     */
    void setDataAtTime(TimeFrameIndex time, std::vector<float> && data, NotifyObservers notify);

    /**
     * @brief Add or replace data at a specific time with timeframe conversion (by copying)
     * 
     * @param time_index_and_frame The time and timeframe to add at
     * @param data The vector of float values to add (will be copied)
     * @param notify Whether to notify observers after the operation
     */
    void setDataAtTime(TimeIndexAndFrame const & time_index_and_frame, std::vector<float> const & data, NotifyObservers notify);

    /**
     * @brief Add or replace data at a specific time with timeframe conversion (by moving)
     * 
     * @param time_index_and_frame The time and timeframe to add at
     * @param data The vector of float values to add (will be moved)
     * @param notify Whether to notify observers after the operation
     */
    void setDataAtTime(TimeIndexAndFrame const & time_index_and_frame, std::vector<float> && data, NotifyObservers notify);

    /**
     * @brief Append values to existing data at a specific time (by copying)
     * 
     * If no data exists at the given time, creates a new entry.
     * 
     * @param time The time to append data at
     * @param data The vector of float values to append (will be copied)
     * @param notify Whether to notify observers after the operation
     */
    void appendAtTime(TimeFrameIndex time, std::vector<float> const & data, NotifyObservers notify);

    /**
     * @brief Append values to existing data at a specific time (by moving)
     * 
     * If no data exists at the given time, creates a new entry.
     * 
     * @param time The time to append data at
     * @param data The vector of float values to append (will be moved)
     * @param notify Whether to notify observers after the operation
     */
    void appendAtTime(TimeFrameIndex time, std::vector<float> && data, NotifyObservers notify);

    /**
     * @brief Clear data at a specific time
     * 
     * @param time The time to clear
     * @param notify Whether to notify observers after the operation
     * @return true if data was found and cleared, false otherwise
     */
    [[nodiscard]] bool clearAtTime(TimeFrameIndex time, NotifyObservers notify);

    /**
     * @brief Clear data at a specific time with timeframe conversion
     * 
     * @param time_index_and_frame The time and timeframe to clear at
     * @param notify Whether to notify observers after the operation
     * @return true if data was found and cleared, false otherwise
     */
    [[nodiscard]] bool clearAtTime(TimeIndexAndFrame const & time_index_and_frame, NotifyObservers notify);

    /**
     * @brief Clear all data from the time series
     * 
     * @param notify Whether to notify observers after the operation
     */
    void clearAll(NotifyObservers notify);

    // ========== Range-based Iteration ==========

    /**
     * @brief Entry structure for iteration
     */
    struct TimeValueEntry {
        TimeFrameIndex time;
        std::span<float const> values;
    };

    /**
     * @brief Iterator for range-based iteration over time-value pairs
     */
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = TimeValueEntry;
        using pointer = TimeValueEntry const *;
        using reference = TimeValueEntry const &;

        Iterator(std::map<TimeFrameIndex, std::vector<float>>::const_iterator it)
            : _it(it) {}

        TimeValueEntry operator*() const {
            return {_it->first, std::span<float const>(_it->second)};
        }

        Iterator& operator++() {
            ++_it;
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(Iterator const & a, Iterator const & b) {
            return a._it == b._it;
        }

        friend bool operator!=(Iterator const & a, Iterator const & b) {
            return a._it != b._it;
        }

    private:
        std::map<TimeFrameIndex, std::vector<float>>::const_iterator _it;
    };

    /**
     * @brief Get iterator to the beginning of the time series
     */
    [[nodiscard]] Iterator begin() const {
        return Iterator(_data.begin());
    }

    /**
     * @brief Get iterator to the end of the time series
     */
    [[nodiscard]] Iterator end() const {
        return Iterator(_data.end());
    }

    /**
     * @brief Get a view over the entire time series
     * 
     * Returns a view compatible with std::ranges that can be used with
     * range-based for loops and range algorithms.
     */
    [[nodiscard]] auto view() const;

    /**
     * @brief Get a flattened view of (TimeFrameIndex, float) pairs
     * 
     * This creates a lazy view that flattens the ragged structure into individual
     * (time, value) pairs. This is the analog of RaggedTimeSeries<T>::elements()
     * and enables uniform iteration API across all ragged container types.
     * 
     * Usage: for (auto [time, value] : ragged_analog.elements()) { ... }
     * 
     * @return A lazy range view of (TimeFrameIndex, float) pairs
     */
    [[nodiscard]] auto elements() const {
        return _data | std::views::transform([](auto const& pair) {
            // Capture the time for each value in the vector
            TimeFrameIndex const time = pair.first;
            std::vector<float> const& values = pair.second;
            
            // Create a view that pairs the time with each float value
            return values | std::views::transform([time](float value) {
                return std::make_pair(time, value);
            });
        }) | std::views::join;  // Flatten the nested ranges
    }

    /**
     * @brief Get a view of (TimeFrameIndex, std::span<float const>) pairs
     * 
     * Similar to RaggedTimeSeries<T>::time_slices(), this provides access to
     * all values at each time point as a span. Useful when you need to process
     * all values at a time together rather than individually.
     * 
     * Usage: for (auto [time, values_span] : ragged_analog.time_slices()) { ... }
     * 
     * @return A lazy range view of (TimeFrameIndex, span) pairs
     */
    [[nodiscard]] auto time_slices() const {
        return _data | std::views::transform([](auto const& pair) {
            return std::make_pair(pair.first, std::span<float const>{pair.second});
        });
    }

private:
    /**
     * @brief Convert time index from source timeframe to this data's timeframe
     * 
     * @param time_index_and_frame The time and timeframe to convert
     * @return The converted TimeFrameIndex in this data's timeframe
     */
    [[nodiscard]] TimeFrameIndex _convertTimeIndex(TimeIndexAndFrame const & time_index_and_frame) const;

    /// Map from TimeFrameIndex to vectors of float values
    std::map<TimeFrameIndex, std::vector<float>> _data;

    /// Associated time frame (optional)
    std::shared_ptr<TimeFrame> _time_frame{nullptr};
};

/**
 * @brief View class for range-based iteration
 * 
 * Provides std::ranges::view_interface compatibility
 */
class RaggedAnalogTimeSeriesView : public std::ranges::view_interface<RaggedAnalogTimeSeriesView> {
public:
    explicit RaggedAnalogTimeSeriesView(RaggedAnalogTimeSeries const * ts)
        : _ts(ts) {}

    [[nodiscard]] auto begin() const { return _ts->begin(); }
    [[nodiscard]] auto end() const { return _ts->end(); }

private:
    RaggedAnalogTimeSeries const * _ts;
};

inline auto RaggedAnalogTimeSeries::view() const {
    return RaggedAnalogTimeSeriesView(this);
}

#endif // RAGGED_ANALOG_TIME_SERIES_HPP
