#ifndef DIGITAL_INTERVAL_SERIES_HPP
#define DIGITAL_INTERVAL_SERIES_HPP

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
 * @brief Digital IntervalSeries class
 *
 * A digital interval series is a series of intervals where each interval is defined by a start and end time.
 * (Compare to DigitalEventSeries which is a series of events at specific times)
 *
 * Use digital events where you wish to specify a beginning and end time for each event.
 *
 *
 */
class DigitalIntervalSeries : public ObserverData {
public:
    // ========== Constructors ==========
    /**
     * @brief Default constructor
     * 
     * This constructor creates an empty DigitalIntervalSeries
     */
    DigitalIntervalSeries() = default;

    /**
     * @brief Constructor for DigitalIntervalSeries from a vector of intervals
     * 
     * @param digital_vector Vector of intervals
     */
    explicit DigitalIntervalSeries(std::vector<Interval> digital_vector);

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
        return std::views::iota(size_t{0}, size())
             | std::views::transform([this](size_t idx) {
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
     * @brief Get a view of (TimeFrameIndex, IntervalWithId) pairs for iteration
     * 
     * Provides a consistent interface matching other time series types.
     * Returns `std::pair<TimeFrameIndex, IntervalWithId>` for backward compatibility
     * with existing code that uses `.first`, `.second`, and structured bindings.
     * 
     * Note: The TimeFrameIndex in the pair is the interval's start time,
     * which is the canonical time point for this element.
     * 
     * Usage: `for(auto [time, interval] : series.elements()) { ... }`
     * 
     * @return A lazy range view of (TimeFrameIndex, IntervalWithId) pairs
     * @see elementsView() for concept-compliant iteration with IntervalWithId
     */
    [[nodiscard]] auto elements() const {
        return std::views::iota(size_t{0}, size())
             | std::views::transform([this](size_t idx) {
                   // Fast path: use cached pointers if valid
                   if (_cached_storage.isValid()) {
                       auto interval = _cached_storage.getInterval(idx);
                       return std::make_pair(
                               TimeFrameIndex(interval.start),
                               IntervalWithId(interval, _cached_storage.getEntityId(idx)));
                   }
                   // Slow path: virtual dispatch through wrapper
                   auto interval = _storage.getInterval(idx);
                   return std::make_pair(
                           TimeFrameIndex(interval.start),
                           IntervalWithId(interval, _storage.getEntityId(idx)));
               });
    }

    /**
     * @brief Get a view of IntervalWithId objects (concept-compliant)
     * 
     * Enables iterating over the series as a sequence of IntervalWithId objects.
     * Each element satisfies the TimeSeriesElement and EntityElement concepts,
     * enabling use with generic time series algorithms.
     * 
     * Use this method when you need concept-compliant elements for generic algorithms.
     * Use elements() when you need backward-compatible pair iteration.
     * 
     * Usage:
     * ```cpp
     * for (auto interval : series.elementsView()) {
     *     auto t = interval.time();     // TimeFrameIndex (start time)
     *     auto id = interval.id();      // EntityId
     *     auto v = interval.value();    // Interval const&
     * }
     * ```
     * 
     * @return A lazy range view of IntervalWithId objects
     * @see IntervalWithId
     * @see TimeSeriesConcepts.hpp for concept definitions
     */
    [[nodiscard]] auto elementsView() const {
        return view();  // view() already returns IntervalWithId objects
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
        _invalidateLegacyCache();
        notifyObservers();
    }

    template<typename T>
    void createIntervalsFromBool(std::vector<T> const & bool_vector) {
        // Clear existing storage and rebuild
        auto* owning = _storage.tryGetMutableOwning();
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
        _invalidateLegacyCache();
        notifyObservers();
    }

    // ========== Getters ==========

    [[nodiscard]] std::vector<Interval> const & getDigitalIntervalSeries() const;

    [[nodiscard]] bool isEventAtTime(TimeFrameIndex time) const;

    [[nodiscard]] size_t size() const { return _storage.size(); };


    // Defines how to handle intervals that overlap with range boundaries
    enum class RangeMode {
        CONTAINED,  // Only intervals fully contained within range
        OVERLAPPING,// Any interval that overlaps with range
        CLIP        // Clip intervals at range boundaries
    };

    template<RangeMode mode = RangeMode::CONTAINED>
    auto getIntervalsInRange(
            int64_t start_time,
            int64_t stop_time) const {

        if constexpr (mode == RangeMode::CONTAINED) {
            // Direct storage access like DigitalEventSeries - returns by value
            return std::views::iota(size_t{0}, _storage.size())
                   | std::views::filter([this, start_time, stop_time](size_t idx) {
                         Interval const interval = _storage.getInterval(idx);
                         return interval.start >= start_time && interval.end <= stop_time;
                     })
                   | std::views::transform([this](size_t idx) {
                         return _storage.getInterval(idx);
                     });
        } else if constexpr (mode == RangeMode::OVERLAPPING) {
            return std::views::iota(size_t{0}, _storage.size())
                   | std::views::filter([this, start_time, stop_time](size_t idx) {
                         Interval const interval = _storage.getInterval(idx);
                         return interval.start <= stop_time && interval.end >= start_time;
                     })
                   | std::views::transform([this](size_t idx) {
                         return _storage.getInterval(idx);
                     });
        } else if constexpr (mode == RangeMode::CLIP) {
            // For CLIP mode, we return a vector since we need to modify intervals
            return _getIntervalsAsVectorClipped(start_time, stop_time);
        } else {
            return std::views::empty<Interval>;
        }
    }

    template<RangeMode mode = RangeMode::CONTAINED>
    auto getIntervalsInRange(
            TimeFrameIndex start_time,
            TimeFrameIndex stop_time,
            TimeFrame const & source_timeframe) const {
        if (&source_timeframe == _time_frame.get()) {
            return getIntervalsInRange<mode>(start_time.getValue(), stop_time.getValue());
        }

        // If either timeframe is null, fall back to original behavior
        if (!_time_frame.get()) {
            return getIntervalsInRange<mode>(start_time.getValue(), stop_time.getValue());
        }

        // Use helper function for time frame conversion
        auto [target_start_index, target_stop_index] = convertTimeFrameRange(start_time,
                                                                             stop_time,
                                                                             source_timeframe,
                                                                             *_time_frame);
        return getIntervalsInRange<mode>(target_start_index.getValue(), target_stop_index.getValue());
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
    [[nodiscard]] std::vector<EntityId> const & getEntityIds() const;

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
     * @brief Find the index for a specific EntityId.
     * 
     * Returns the index in the interval vector associated with the given EntityId.
     * 
     * @param entity_id The EntityId to look up
     * @return Optional containing the index if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<int> getIndexByEntityId(EntityId entity_id) const;

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

    /**
     * @brief Get index information for multiple EntityIds.
     * 
     * Returns both the EntityId and its corresponding index in the data vector
     * for batch operations.
     * 
     * @param entity_ids Vector of EntityIds to look up
     * @return Vector of tuples containing {EntityId, index} for found entities
     */
    [[nodiscard]] std::vector<std::pair<EntityId, int>> getIndexInfoByEntityIds(std::vector<EntityId> const & entity_ids) const;

    // ========== Intervals with EntityIDs ==========

    /**
     * @brief Get intervals in range with their EntityIDs using TimeFrameIndex
     * 
     * @param start_time Start time index for the range
     * @param stop_time Stop time index for the range
     * @return std::vector<IntervalWithId> Vector of intervals with their EntityIDs
     */
    [[nodiscard]] std::vector<IntervalWithId> getIntervalsWithIdsInRange(TimeFrameIndex start_time, TimeFrameIndex stop_time) const;

    /**
     * @brief Get intervals in range with their EntityIDs using time frame conversion
     * 
     * @param start_index Start time index in source timeframe
     * @param stop_index Stop time index in source timeframe
     * @param source_time_frame Source timeframe for the indices
     * @return std::vector<IntervalWithId> Vector of intervals with their EntityIDs
     */
    [[nodiscard]] std::vector<IntervalWithId> getIntervalsWithIdsInRange(TimeFrameIndex start_index,
                                                                         TimeFrameIndex stop_index,
                                                                         TimeFrame const & source_time_frame) const;

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
    DigitalIntervalStorageCache _cached_storage;  // Fast-path cache
    std::shared_ptr<TimeFrame> _time_frame{nullptr};
    
    // Lazy-built cache for legacy API (getDigitalIntervalSeries)
    mutable std::vector<Interval> _legacy_data_cache;
    mutable bool _legacy_data_cache_valid{false};
    mutable std::vector<EntityId> _legacy_entity_id_cache;
    mutable bool _legacy_entity_id_cache_valid{false};
    void _invalidateLegacyCache() { 
        _legacy_data_cache_valid = false; 
        _legacy_entity_id_cache_valid = false;
    }
    void _rebuildLegacyCacheIfNeeded() const;
    void _rebuildEntityIdCacheIfNeeded() const;
    
    // Cache management
    void _cacheOptimizationPointers();

    void _addEventInternal(Interval new_interval);  // Core mutation logic
    void _setEventAtTimeInternal(TimeFrameIndex time, bool event);
    void _removeEventAtTimeInternal(TimeFrameIndex time);

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
