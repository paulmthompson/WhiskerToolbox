#ifndef RAGGED_ANALOG_TIME_SERIES_HPP
#define RAGGED_ANALOG_TIME_SERIES_HPP

#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TypeTraits/DataTypeTraits.hpp"
#include "utils/RaggedAnalogStorage.hpp"

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
 * - Time series storage via type-erased storage wrapper (supports owning, view, and lazy)
 * - TimeFrame association for time-based operations
 * - Observer pattern integration for data change notifications
 * - Range-based iteration interface
 * - Cache optimization for fast-path iteration over contiguous storage
 *
 * Storage Backends:
 * - OwningRaggedAnalogStorage: Default, owns data in SoA layout
 * - ViewRaggedAnalogStorage: Zero-copy filtered view of another storage
 * - LazyRaggedAnalogStorage: On-demand computation from a transform view
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
                // Single float value
                _storage.append(time, static_cast<float>(values));
            }
            else if constexpr (std::convertible_to<decltype(values), std::vector<float>>) {
                // Vector of floats - use batch append
                _storage.appendBatch(time, values);
            }
            else if constexpr (std::ranges::range<decltype(values)>) {
                // Span or other range type
                std::vector<float> vec(std::ranges::begin(values), std::ranges::end(values));
                _storage.appendBatch(time, std::move(vec));
            }
        }
        _updateStorageCache();
    }

    // Delete copy constructor and copy assignment
    RaggedAnalogTimeSeries(RaggedAnalogTimeSeries const &) = delete;
    RaggedAnalogTimeSeries & operator=(RaggedAnalogTimeSeries const &) = delete;

    // Enable move semantics
    RaggedAnalogTimeSeries(RaggedAnalogTimeSeries &&) = default;
    RaggedAnalogTimeSeries & operator=(RaggedAnalogTimeSeries &&) = default;

    // ========== Lazy Transform Factory Methods ==========

    /**
     * @brief Create RaggedAnalogTimeSeries from a lazy view
     * 
     * Creates a RaggedAnalogTimeSeries that computes values on-demand from a ranges view.
     * The view must be a random-access range that yields pairs compatible with
     * (TimeFrameIndex, float). No intermediate data is materialized.
     * 
     * @tparam ViewType Random-access range type
     * @param view Random-access range view yielding (TimeFrameIndex, float) pairs
     * @param time_frame Shared time frame
     * @return std::shared_ptr<RaggedAnalogTimeSeries> Lazy time series
     * 
     * @example
     * ```cpp
     * auto scaled_view = original.elements()
     *     | std::views::transform([](auto pair) {
     *         return std::make_pair(pair.first, pair.second * 2.0f);
     *     });
     * auto lazy_scaled = RaggedAnalogTimeSeries::createFromView(scaled_view, original.getTimeFrame());
     * ```
     */
    template<std::ranges::random_access_range ViewType>
    [[nodiscard]] static std::shared_ptr<RaggedAnalogTimeSeries> createFromView(
            ViewType view,
            std::shared_ptr<TimeFrame> time_frame)
    {
        size_t num_elements = std::ranges::size(view);
        
        // Create lazy storage
        auto lazy_storage = LazyRaggedAnalogStorage<ViewType>(std::move(view), num_elements);
        RaggedAnalogStorageWrapper storage_wrapper(std::move(lazy_storage));
        
        auto result = std::make_shared<RaggedAnalogTimeSeries>();
        result->_storage = std::move(storage_wrapper);
        result->_time_frame = std::move(time_frame);
        result->_updateStorageCache();
        
        return result;
    }

    /**
     * @brief Materialize lazy storage into owning storage
     * 
     * If this series has lazy storage, creates a new series with all values
     * computed and stored in owning storage. Useful when:
     * - The source data for a lazy view is about to be destroyed
     * - Random access patterns would cause repeated computation
     * - The data needs to be saved to disk
     * 
     * @return New RaggedAnalogTimeSeries with owning storage
     */
    [[nodiscard]] std::shared_ptr<RaggedAnalogTimeSeries> materialize() const {
        auto result = std::make_shared<RaggedAnalogTimeSeries>();
        result->_time_frame = _time_frame;
        
        // Copy all elements to new owning storage
        for (size_t i = 0; i < _storage.size(); ++i) {
            result->_storage.append(_storage.getTime(i), _storage.getValue(i));
        }
        result->_updateStorageCache();
        
        return result;
    }

    /**
     * @brief Check if this series uses lazy storage
     * 
     * @return true if underlying storage is LazyRaggedAnalogStorage
     */
    [[nodiscard]] bool isLazy() const {
        return _storage.isLazy();
    }

    /**
     * @brief Check if this series uses view storage
     * 
     * @return true if underlying storage is ViewRaggedAnalogStorage
     */
    [[nodiscard]] bool isView() const {
        return _storage.isView();
    }

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
     * For lazy storage, this may return empty span; use getValuesAtTimeVec() instead.
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
     * @brief Get the data at a specific time as a vector (works with lazy storage)
     * 
     * This method always works, including for lazy storage where getDataAtTime()
     * might return empty span.
     * 
     * @param time The time to query
     * @return Vector of float values at the time, empty if no data exists
     */
    [[nodiscard]] std::vector<float> getValuesAtTimeVec(TimeFrameIndex time) const;

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

    /**
     * @brief Get total number of float values across all times
     * 
     * @return Total element count
     */
    [[nodiscard]] size_t getTotalValueCount() const {
        return _storage.size();
    }

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
     * @brief Entry structure for flat element iteration
     */
    struct FlatElement {
        TimeFrameIndex time;
        float value;
    };

    /**
     * @brief Iterator for range-based iteration over time-value pairs
     * 
     * This iterator groups values by time, yielding TimeValueEntry for each
     * distinct time point. For lazy storage, the span may be empty.
     */
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = TimeValueEntry;
        using pointer = TimeValueEntry const *;
        using reference = TimeValueEntry const &;

        Iterator() = default;

        Iterator(RaggedAnalogTimeSeries const* series,
                 std::map<TimeFrameIndex, std::pair<size_t, size_t>>::const_iterator time_it)
            : _series(series), _time_iter(time_it) {}

        TimeValueEntry operator*() const {
            TimeFrameIndex const time = _time_iter->first;
            return {time, _series->_storage.getValuesAtTime(time)};
        }

        Iterator& operator++() {
            ++_time_iter;
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(Iterator const & a, Iterator const & b) {
            return a._time_iter == b._time_iter;
        }

        friend bool operator!=(Iterator const & a, Iterator const & b) {
            return a._time_iter != b._time_iter;
        }

    private:
        RaggedAnalogTimeSeries const* _series = nullptr;
        std::map<TimeFrameIndex, std::pair<size_t, size_t>>::const_iterator _time_iter;
    };

    /**
     * @brief Get iterator to the beginning of the time series
     */
    [[nodiscard]] Iterator begin() const {
        return Iterator(this, _storage.timeRanges().begin());
    }

    /**
     * @brief Get iterator to the end of the time series
     */
    [[nodiscard]] Iterator end() const {
        return Iterator(this, _storage.timeRanges().end());
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
        // Create an index-based view over all elements
        return std::views::iota(size_t{0}, _storage.size())
            | std::views::transform([this](size_t idx) {
                return std::make_pair(_storage.getTime(idx), _storage.getValue(idx));
            });
    }

    /**
     * @brief Get a view of (TimeFrameIndex, std::span<float const>) pairs
     * 
     * Similar to RaggedTimeSeries<T>::time_slices(), this provides access to
     * all values at each time point as a span. Useful when you need to process
     * all values at a time together rather than individually.
     * 
     * Note: For lazy storage, the spans may be empty. Use time_slices_vec()
     * instead if you need guaranteed access.
     * 
     * Usage: for (auto [time, values_span] : ragged_analog.time_slices()) { ... }
     * 
     * @return A lazy range view of (TimeFrameIndex, span) pairs
     */
    [[nodiscard]] auto time_slices() const {
        return _storage.timeRanges()
            | std::views::transform([this](auto const& pair) {
                return std::make_pair(pair.first, _storage.getValuesAtTime(pair.first));
            });
    }

    // ========== Storage Access (Advanced) ==========

    /**
     * @brief Get storage cache for fast-path iteration
     * 
     * Returns cached pointers if storage is contiguous, otherwise invalid cache.
     */
    [[nodiscard]] RaggedAnalogStorageCache getStorageCache() const {
        return _cached_storage;
    }

private:
    /**
     * @brief Convert time index from source timeframe to this data's timeframe
     * 
     * @param time_index_and_frame The time and timeframe to convert
     * @return The converted TimeFrameIndex in this data's timeframe
     */
    [[nodiscard]] TimeFrameIndex _convertTimeIndex(TimeIndexAndFrame const & time_index_and_frame) const;

    /**
     * @brief Update the storage cache after mutations
     */
    void _updateStorageCache() {
        _cached_storage = _storage.tryGetCache();
    }

    /**
     * @brief Invalidate the storage cache before mutations
     */
    void _invalidateStorageCache() {
        _cached_storage = RaggedAnalogStorageCache{};
    }

    /// Type-erased storage wrapper
    RaggedAnalogStorageWrapper _storage;

    /// Cached pointers for fast-path iteration
    RaggedAnalogStorageCache _cached_storage;

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
