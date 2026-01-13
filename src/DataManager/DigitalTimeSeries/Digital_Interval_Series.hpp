#ifndef DIGITAL_INTERVAL_SERIES_HPP
#define DIGITAL_INTERVAL_SERIES_HPP

/**
 * @file Digital_Interval_Series.hpp
 * @brief DigitalIntervalSeries class for managing time interval data with entity tracking
 * 
 * This file provides the DigitalIntervalSeries class, which represents a sorted collection
 * of time intervals, where each interval is defined by a start and end TimeFrameIndex
 * (indices into an accompanying TimeFrame) and an EntityId (a unique identifier).
 * 
 * ## Storage Backends
 * 
 * DigitalIntervalSeries supports three storage backends via a type-erased wrapper:
 * 
 * ### Owning Storage (DigitalIntervalStorageType::Owning)
 * - **Default storage type** for newly created series
 * - Owns interval data in Structure-of-Arrays (SoA) layout for cache efficiency
 * - Supports all mutation operations (addEvent, removeInterval, setEventAtTime)
 * - Intervals are always maintained in sorted order by start time
 * - O(log n) lookups by start time, O(1) lookups by EntityId via hash map
 * 
 * ### View Storage (DigitalIntervalStorageType::View)
 * - **Zero-copy filtered view** of another series
 * - Created via createView() factory methods
 * - References source data via index vector; no data copying
 * - Supports filtering by time range or EntityId set
 * - **Read-only**: mutation operations will materialize to owning storage first
 * - Returns valid cache if view indices are contiguous
 * 
 * ### Lazy Storage (DigitalIntervalStorageType::Lazy)
 * - **On-demand computation** from transform views
 * - Created via createFromView() template method
 * - Stores a C++20 ranges view that computes IntervalWithId on access
 * - Useful for transform pipelines without materializing intermediate results
 * - **Read-only**: mutation operations will materialize to owning storage first
 * - Always returns invalid cache (forces virtual dispatch)
 * 
 * ## TimeFrame Integration
 * 
 * Each interval's start/end is stored as a TimeFrameIndex, which is an index into the
 * series' associated TimeFrame. This enables:
 * - Different data sources to use different sampling rates
 * - Automatic time conversion when querying across timeframes
 * - Efficient range queries using binary search on indices
 * 
 * ## Entity System Integration
 * 
 * Each interval can have an associated EntityId for tracking across the application.
 * When setIdentityContext() is called with an EntityRegistry, new intervals are
 * automatically assigned unique EntityIds. This enables:
 * - Linking intervals to analysis results
 * - Group-based selection and filtering
 * - Cross-data-type entity tracking
 * 
 * @see DigitalIntervalStorage.hpp for storage implementation details
 * @see IntervalWithId for the element type returned by iterators
 * @see TimeFrame for time base management
 * @see EntityRegistry for entity ID management
 */

#include "DigitalIntervalStorage.hpp"
#include "DigitalTimeSeries/IntervalWithId.hpp"
#include "Entity/EntityTypes.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <ranges>
#include <unordered_set>
#include <utility>
#include <vector>


class EntityRegistry;
template<typename T>
inline constexpr bool always_false_v = false;


/**
 * @brief A sorted collection of time intervals with entity tracking
 * 
 * DigitalIntervalSeries stores intervals as (start, end, EntityId) tuples, maintaining
 * intervals in sorted order by start time. Each interval represents a time range
 * (compare to DigitalEventSeries for discrete time points).
 * 
 * ## Primary Interface
 * 
 * - **view()**: Returns a lazy range of IntervalWithId objects for iteration
 * - **viewInRange()**: Returns intervals overlapping a time range (requires TimeFrame)
 * - **hasIntervalAtTime()**: Check if any interval contains a time (requires TimeFrame)
 * - **addEvent()/removeInterval()**: Modify intervals (owning storage only)
 * - **createView()/createFromView()**: Create view/lazy backed series
 * - **materialize()**: Convert any storage type to owning storage
 * 
 * ## Storage Backends
 * 
 * The class supports three storage backends (Owning, View, Lazy) that provide
 * different performance tradeoffs. See @ref Digital_Interval_Series.hpp for details.
 * 
 * ## Example Usage
 * 
 * ```cpp
 * // Create and populate a series
 * DigitalIntervalSeries series;
 * series.setTimeFrame(my_time_frame);
 * series.addEvent(Interval{100, 200});
 * series.addEvent(Interval{300, 400});
 * 
 * // Iterate all intervals
 * for (auto interval : series.view()) {
 *     std::cout << "Interval [" << interval.interval.start 
 *               << ", " << interval.interval.end << "] with id " 
 *               << interval.entity_id.getValue() << "\n";
 * }
 * 
 * // Query range with timeframe
 * for (auto interval : series.viewInRange(start, end, source_timeframe)) {
 *     process(interval);
 * }
 * 
 * // Create filtered view
 * auto filtered = DigitalIntervalSeries::createView(
 *     std::make_shared<DigitalIntervalSeries>(series),
 *     50, 250);  // Intervals overlapping [50, 250]
 * ```
 * 
 * @note Intervals are always sorted by start time.
 * @note View and Lazy storage will auto-materialize on mutation.
 * 
 * @see IntervalWithId for element accessors (time(), id(), value())
 * @see DigitalEventSeries for discrete event data
 */
class DigitalIntervalSeries : public ObserverData {
public:
    // ========== Constructors ==========
    /**
     * @brief Default constructor
     * 
     * This constructor creates an empty DigitalIntervalSeries with owning storage.
     */
    DigitalIntervalSeries() = default;

    /**
     * @brief Constructor for DigitalIntervalSeries from a vector of intervals
     * 
     * Intervals are sorted during construction. EntityIds are initialized to 0.
     * 
     * @param digital_vector Vector of intervals (will be sorted)
     */
    explicit DigitalIntervalSeries(std::vector<Interval> digital_vector);

    /**
     * @brief Constructor from float pairs (legacy support)
     * 
     * @param digital_vector Vector of (start, end) pairs as floats
     */
    explicit DigitalIntervalSeries(std::vector<std::pair<float, float>> const & digital_vector);

    // =============================================================
    // ==================== RANGES INTERFACE =======================
    // =============================================================

    /**
     * @brief Get a std::ranges compatible view of the series.
     * 
     * Returns a random-access view that synthesizes IntervalWithId objects on demand.
     * Uses cached pointers for fast-path iteration when storage is contiguous.
     * Allows iterating over IntervalWithId objects directly.
     */
    [[nodiscard]] auto view() const {
        return std::views::iota(size_t{0}, size()) | std::views::transform([this](size_t idx) {
                   // Fast path: use cached pointers if valid
                   if (_cached_storage.isValid()) {
                       return IntervalWithId(
                               _cached_storage.getInterval(idx),
                               _cached_storage.getEntityId(idx));
                   }
                   // Slow path: virtual dispatch through wrapper
                   return IntervalWithId(
                           _storage.getInterval(idx),
                           _storage.getEntityId(idx));
               });
    }

    /**
     * @brief Get intervals in a time range as a lazy view of IntervalWithId objects
     * 
     * This is the primary range query interface. It requires a source TimeFrame
     * parameter to enable proper time conversion when the query time base differs
     * from the series' internal time base.
     * 
     * Returns intervals that overlap with the given range [start_index, stop_index].
     * 
     * @param start_index Start time index (inclusive)
     * @param stop_index Stop time index (inclusive)
     * @param source_time_frame The time frame that start_index/stop_index are expressed in
     * @return Lazy view of IntervalWithId objects in the range
     * 
     * @note If source_time_frame matches the series' time frame, no conversion occurs
     * @note If the series has no time frame set, indices are used directly
     */
    [[nodiscard]] auto viewInRange(TimeFrameIndex start_index,
                                   TimeFrameIndex stop_index,
                                   TimeFrame const & source_time_frame) const {
        auto time_range = _getConvertedTimeRange(start_index, stop_index, source_time_frame);
        int64_t const range_start = time_range.first;
        int64_t const range_stop = time_range.second;
        return std::views::iota(size_t{0}, size()) | std::views::filter([this, range_start, range_stop](size_t idx) {
                   Interval const interval = _storage.getInterval(idx);
                   // Overlapping: interval.start <= stop_time && interval.end >= start_time
                   return interval.start <= range_stop && interval.end >= range_start;
               }) |
               std::views::transform([this](size_t idx) {
                   return IntervalWithId(_storage.getInterval(idx), _storage.getEntityId(idx));
               });
    }

    /**
     * @brief Get just the Interval values in a range as a lazy view
     * 
     * @param start_index Start time index (inclusive)
     * @param stop_index Stop time index (inclusive)
     * @param source_time_frame The time frame that start_index/stop_index are expressed in
     * @return Lazy view of Interval values in the range
     */
    [[nodiscard]] auto viewIntervalsInRange(TimeFrameIndex start_index,
                                            TimeFrameIndex stop_index,
                                            TimeFrame const & source_time_frame) const {
        auto time_range = _getConvertedTimeRange(start_index, stop_index, source_time_frame);
        int64_t const range_start = time_range.first;
        int64_t const range_stop = time_range.second;
        return std::views::iota(size_t{0}, size()) | std::views::filter([this, range_start, range_stop](size_t idx) {
                   Interval const interval = _storage.getInterval(idx);
                   return interval.start <= range_stop && interval.end >= range_start;
               }) |
               std::views::transform([this](size_t idx) {
                   return _storage.getInterval(idx);
               });
    }

    /**
     * @brief Check if any interval contains the specified time
     * 
     * @param time The time index to check
     * @param source_time_frame The time frame that the time index is expressed in
     * @return true if any interval contains the time, false otherwise
     */
    [[nodiscard]] bool hasIntervalAtTime(TimeFrameIndex time, TimeFrame const & source_time_frame) const {
        auto converted_time = _getConvertedTime(time, source_time_frame);
        return _hasIntervalAtTime(converted_time);
    }

    // ========== Setters ==========

    void addEvent(Interval new_interval);

    void addEvent(TimeFrameIndex start, TimeFrameIndex end) {

        if (start > end) {
            std::cout << "Start time is greater than end time" << std::endl;
            return;
        }

        addEvent(Interval{start.getValue(), end.getValue()});
    }

    void setEventAtTime(TimeFrameIndex time, bool event);

    /**
     * @brief Remove an interval from the series
     * 
     * @param interval The interval to remove
     * @return True if the interval was found and removed, false otherwise
     */
    bool removeInterval(Interval const & interval);

    /**
     * @brief Remove multiple intervals from the series
     * 
     * @param intervals The intervals to remove
     * @return The number of intervals that were successfully removed
     */
    size_t removeIntervals(std::vector<Interval> const & intervals);

    template<typename T, typename B>
    void setEventsAtTimes(std::vector<T> times, std::vector<B> events) {
        for (size_t i = 0; i < times.size(); ++i) {
            _setEventAtTimeInternal(TimeFrameIndex(times[i]), events[i]);
        }
        _cacheOptimizationPointers();
        notifyObservers();
    }

    template<typename T>
    void createIntervalsFromBool(std::vector<T> const & bool_vector) {
        // Clear existing storage and rebuild
        auto * owning = _storage.tryGetMutableOwning();
        if (owning) {
            owning->clear();
        }

        bool in_interval = false;
        int start = 0;
        for (size_t i = 0; i < bool_vector.size(); ++i) {
            if (bool_vector[i] && !in_interval) {
                start = static_cast<int>(i);
                in_interval = true;
            } else if (!bool_vector[i] && in_interval) {
                if (owning) {
                    owning->addInterval(Interval{start, static_cast<int64_t>(i - 1)}, EntityId{0});
                }
                in_interval = false;
            }
        }
        if (in_interval && owning) {
            owning->addInterval(Interval{start, static_cast<int64_t>(bool_vector.size() - 1)}, EntityId{0});
        }

        _cacheOptimizationPointers();
        notifyObservers();
    }

    // ========== Getters ==========

    [[nodiscard]] size_t size() const { return _storage.size(); };


    // ========== Range-Mode Interval Queries ==========

    /**
     * @brief Controls how intervals at range boundaries are handled
     */
    enum class RangeMode {
        CONTAINED,  ///< Only intervals fully contained within range
        OVERLAPPING,///< Any interval that overlaps with range
        CLIP        ///< Clip intervals at range boundaries
    };

    /**
     * @brief Get intervals in a time range with TimeFrame conversion
     * 
     * @deprecated For OVERLAPPING mode, prefer viewInRange() or viewIntervalsInRange() 
     *             which provide lazy evaluation.
     * 
     * @tparam mode RangeMode::CONTAINED (default), OVERLAPPING, or CLIP
     * @param start_time Start time index in source_timeframe
     * @param stop_time Stop time index in source_timeframe
     * @param source_timeframe TimeFrame the indices are expressed in
     * @return Lazy view of Interval objects (or vector for CLIP mode)
     * @see viewIntervalsInRange for preferred alternative
     */
    template<RangeMode mode = RangeMode::CONTAINED>
    auto getIntervalsInRange(
            TimeFrameIndex start_time,
            TimeFrameIndex stop_time,
            TimeFrame const & source_timeframe) const {
        if (&source_timeframe == _time_frame.get()) {
            return _getIntervalsInRange<mode>(start_time.getValue(), stop_time.getValue());
        }

        // If either timeframe is null, fall back to original behavior
        if (!_time_frame.get()) {
            return _getIntervalsInRange<mode>(start_time.getValue(), stop_time.getValue());
        }

        // Use helper function for time frame conversion
        auto [target_start_index, target_stop_index] = convertTimeFrameRange(start_time,
                                                                             stop_time,
                                                                             source_timeframe,
                                                                             *_time_frame);
        return _getIntervalsInRange<mode>(target_start_index.getValue(), target_stop_index.getValue());
    }

    // ========== Time Frame ==========

    /**
     * @brief Set the time frame
     * 
     * @param time_frame The time frame to set
     */
    void setTimeFrame(std::shared_ptr<TimeFrame> time_frame) { _time_frame = time_frame; }

    /**
     * @brief Get the time frame
     * 
     * @return The time frame (may be nullptr)
     */
    [[nodiscard]] std::shared_ptr<TimeFrame> getTimeFrame() const { return _time_frame; }

    // ===== Identity =====
    void setIdentityContext(std::string const & data_key, EntityRegistry * registry) {
        _identity_data_key = data_key;
        _identity_registry = registry;
    }
    void rebuildAllEntityIds();

    // ========== Entity Lookup Methods ==========

    /**
     * @brief Find the interval data associated with a specific EntityId.
     * 
     * This method provides reverse lookup from EntityId to the actual interval data,
     * supporting group-based visualization workflows.
     * 
     * @param entity_id The EntityId to look up
     * @return Optional containing the interval data if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<Interval> getIntervalByEntityId(EntityId entity_id) const;

    /**
     * @brief Get all intervals that match the given EntityIds.
     * 
     * This method is optimized for batch lookup of multiple EntityIds,
     * useful for group-based operations.
     * 
     * @param entity_ids Vector of EntityIds to look up
     * @return Vector of pairs containing {EntityId, Interval} for found entities
     */
    [[nodiscard]] std::vector<std::pair<EntityId, Interval>> getIntervalsByEntityIds(std::vector<EntityId> const & entity_ids) const;

    // ========== Storage Type Queries ==========

    /**
     * @brief Check if this series uses view storage (doesn't own data)
     * @return true if storage is a view, false otherwise
     */
    [[nodiscard]] bool isView() const { return _storage.isView(); }

    /**
     * @brief Check if this series uses lazy storage
     * @return true if storage is lazy-evaluated, false otherwise
     */
    [[nodiscard]] bool isLazy() const { return _storage.isLazy(); }

    /**
     * @brief Get the storage type of this series
     * @return The storage type enum value
     */
    [[nodiscard]] DigitalIntervalStorageType getStorageType() const { return _storage.getStorageType(); }

    // ========== Factory Methods ==========

    /**
     * @brief Create a view of this series filtered by time range
     * 
     * Returns a new DigitalIntervalSeries that references this series' data
     * without copying. The view includes intervals overlapping with [start, end].
     * 
     * @param start Start time (inclusive)
     * @param end End time (inclusive)
     * @return Shared pointer to new view-based series
     */
    [[nodiscard]] static std::shared_ptr<DigitalIntervalSeries> createView(
            std::shared_ptr<DigitalIntervalSeries const> source,
            int64_t start,
            int64_t end);

    /**
     * @brief Create a view of this series filtered by EntityIds
     * 
     * @param source Source series to create view from
     * @param entity_ids Set of EntityIds to include in the view
     * @return Shared pointer to new view-based series
     */
    [[nodiscard]] static std::shared_ptr<DigitalIntervalSeries> createView(
            std::shared_ptr<DigitalIntervalSeries const> source,
            std::unordered_set<EntityId> const & entity_ids);

    /**
     * @brief Materialize this series into a new owning series
     * 
     * If this series uses view or lazy storage, creates a new series
     * with owning storage containing copies of all data.
     * 
     * @return Shared pointer to new owning series
     */
    [[nodiscard]] std::shared_ptr<DigitalIntervalSeries> materialize() const;

    /**
     * @brief Create a lazy-evaluated series from a transform view
     * 
     * Creates a new DigitalIntervalSeries backed by lazy storage that computes
     * elements on-demand from the provided view. Useful for transform pipelines
     * where you want to defer computation until elements are accessed.
     * 
     * The view must yield objects that are convertible to IntervalWithId, or
     * have .interval and .entity_id members, or be a pair/tuple of (Interval, EntityId).
     * 
     * @tparam ViewType Random-access range type yielding IntervalWithId-like objects
     * @param view The transform view (will be moved)
     * @param num_elements Number of elements in the view
     * @param time_frame Optional time frame for the new series
     * @return New series backed by lazy storage
     * 
     * @note The resulting series is read-only. Call materialize() to convert
     *       to an owning series if you need to modify the data.
     * 
     * @example
     * @code
     * auto source = std::make_shared<DigitalIntervalSeries>(...);
     * 
     * // Create a lazy transform that shifts all intervals by 100
     * auto shifted_view = source->view() 
     *     | std::views::transform([](IntervalWithId const& iwid) {
     *           return IntervalWithId(
     *               Interval{iwid.interval.start + 100, iwid.interval.end + 100},
     *               iwid.entity_id);
     *       });
     * 
     * auto lazy_series = DigitalIntervalSeries::createFromView(
     *     shifted_view, source->size(), source->getTimeFrame());
     * @endcode
     */
    template<typename ViewType>
    [[nodiscard]] static std::shared_ptr<DigitalIntervalSeries> createFromView(
            ViewType view,
            size_t num_elements,
            std::shared_ptr<TimeFrame> time_frame = nullptr);

    /**
     * @brief Get the storage cache for fast-path iteration
     * 
     * Returns a cache structure with pointers to contiguous data if available.
     * 
     * @return DigitalIntervalStorageCache with valid pointers if contiguous
     */
    [[nodiscard]] DigitalIntervalStorageCache getStorageCache() const { return _storage.tryGetCache(); }

private:
    DigitalIntervalStorageWrapper _storage;
    DigitalIntervalStorageCache _cached_storage;// Fast-path cache
    std::shared_ptr<TimeFrame> _time_frame{nullptr};

    // Cache management
    void _cacheOptimizationPointers();

    void _addEventInternal(Interval new_interval);// Core mutation logic
    void _setEventAtTimeInternal(TimeFrameIndex time, bool event);
    void _removeEventAtTimeInternal(TimeFrameIndex time);

    /**
     * @brief Get intervals in a time range with configurable boundary handling
     * 
     * @deprecated For OVERLAPPING mode, prefer viewInRange() or viewIntervalsInRange() 
     *             which provide lazy evaluation with TimeFrame support.
     * 
     * @tparam mode RangeMode::CONTAINED (default), OVERLAPPING, or CLIP
     * @param start_time Start time value
     * @param stop_time Stop time value
     * @return Lazy view of Interval objects (or vector for CLIP mode)
     * @see viewIntervalsInRange for TimeFrame-aware alternative
     */
    template<RangeMode mode = RangeMode::CONTAINED>
    auto _getIntervalsInRange(
            int64_t start_time,
            int64_t stop_time) const {

        if constexpr (mode == RangeMode::CONTAINED) {
            // Direct storage access like DigitalEventSeries - returns by value
            return std::views::iota(size_t{0}, _storage.size()) | std::views::filter([this, start_time, stop_time](size_t idx) {
                       Interval const interval = _storage.getInterval(idx);
                       return interval.start >= start_time && interval.end <= stop_time;
                   }) |
                   std::views::transform([this](size_t idx) {
                       return _storage.getInterval(idx);
                   });
        } else if constexpr (mode == RangeMode::OVERLAPPING) {
            return std::views::iota(size_t{0}, _storage.size()) | std::views::filter([this, start_time, stop_time](size_t idx) {
                       Interval const interval = _storage.getInterval(idx);
                       return interval.start <= stop_time && interval.end >= start_time;
                   }) |
                   std::views::transform([this](size_t idx) {
                       return _storage.getInterval(idx);
                   });
        } else if constexpr (mode == RangeMode::CLIP) {
            // For CLIP mode, we return a vector since we need to modify intervals
            return _getIntervalsAsVectorClipped(start_time, stop_time);
        } else {
            return std::views::empty<Interval>;
        }
    }

    // Helper method to handle clipping intervals at range boundaries
    std::vector<Interval> _getIntervalsAsVectorClipped(
            int64_t start_time,
            int64_t stop_time) const {

        std::vector<Interval> result;

        for (size_t i = 0; i < _storage.size(); ++i) {
            Interval const interval = _storage.getInterval(i);

            // Skip if not overlapping
            if (interval.end < start_time || interval.start > stop_time)
                continue;

            // Create a new clipped interval based on original interval values
            // but clipped at the transformed boundaries
            int64_t clipped_start = interval.start;
            if (clipped_start < start_time) {
                // Binary search or interpolation to find the original value
                // that transforms to start_time would be ideal here
                // For now, use a simple approach:
                while (clipped_start < start_time && clipped_start < interval.end)
                    clipped_start++;
            }

            int64_t clipped_end = interval.end;
            if (clipped_end > stop_time) {
                while (clipped_end > interval.start && clipped_end > stop_time)
                    clipped_end--;
            }

            result.push_back(Interval{clipped_start, clipped_end});
        }

        return result;
    }
    // Identity
    std::string _identity_data_key;
    EntityRegistry * _identity_registry{nullptr};

    /**
     * @brief Get time values from TimeFrameIndex range
     * 
     * @param start_index Start time index
     * @param stop_index Stop time index
     * @return std::pair<int64_t, int64_t> Start and stop time values
     */
    [[nodiscard]] std::pair<int64_t, int64_t> _getTimeRangeFromIndices(
            TimeFrameIndex start_index,
            TimeFrameIndex stop_index) const;

    // ========== Private Range Query Helpers ==========

    /**
     * @brief Convert time range from source time frame to internal time values
     * 
     * @param start_index Start time index in source_time_frame
     * @param stop_index Stop time index in source_time_frame
     * @param source_time_frame The time frame the indices are expressed in
     * @return Pair of (start_time, stop_time) as int64_t values for internal use
     */
    [[nodiscard]] std::pair<int64_t, int64_t> _getConvertedTimeRange(
            TimeFrameIndex start_index,
            TimeFrameIndex stop_index,
            TimeFrame const & source_time_frame) const {
        // Fast path: same time frame or no conversion needed
        if (&source_time_frame == _time_frame.get() || !_time_frame.get()) {
            return {start_index.getValue(), stop_index.getValue()};
        }
        // Convert to our time frame
        auto [target_start, target_stop] = convertTimeFrameRange(
                start_index, stop_index, source_time_frame, *_time_frame);
        return {target_start.getValue(), target_stop.getValue()};
    }

    /**
     * @brief Convert a single time index from source time frame to internal time value
     * 
     * @param time_index Time index in source_time_frame
     * @param source_time_frame The time frame the index is expressed in
     * @return int64_t time value for internal use
     */
    [[nodiscard]] int64_t _getConvertedTime(
            TimeFrameIndex time_index,
            TimeFrame const & source_time_frame) const {
        if (&source_time_frame == _time_frame.get() || !_time_frame.get()) {
            return time_index.getValue();
        }
        // Convert using the same logic as range conversion
        auto [target, _] = convertTimeFrameRange(
                time_index, time_index, source_time_frame, *_time_frame);
        return target.getValue();
    }

    /**
     * @brief Internal implementation of hasIntervalAtTime without TimeFrame conversion
     * 
     * @param time The time value to check (already in internal coordinates)
     * @return true if any interval contains the time
     */
    [[nodiscard]] bool _hasIntervalAtTime(int64_t time) const {
        for (size_t i = 0; i < _storage.size(); ++i) {
            Interval const & interval = _storage.getInterval(i);
            if (interval.start <= time && time <= interval.end) {
                return true;
            }
        }
        return false;
    }
};

// =============================================================================
// Template Implementation
// =============================================================================

template<typename ViewType>
std::shared_ptr<DigitalIntervalSeries> DigitalIntervalSeries::createFromView(
        ViewType view,
        size_t num_elements,
        std::shared_ptr<TimeFrame> time_frame) {
    auto result = std::make_shared<DigitalIntervalSeries>();
    result->_storage = DigitalIntervalStorageWrapper{
            LazyDigitalIntervalStorage<ViewType>{std::move(view), num_elements}};
    result->_time_frame = std::move(time_frame);
    result->_cacheOptimizationPointers();
    // Note: _data vector left empty - storage wrapper handles all access
    return result;
}

int find_closest_preceding_event(DigitalIntervalSeries * digital_series, TimeFrameIndex time);

#endif// DIGITAL_INTERVAL_SERIES_HPP
