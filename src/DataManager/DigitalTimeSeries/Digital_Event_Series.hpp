#ifndef BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
#define BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP

/**
 * @file Digital_Event_Series.hpp
 * @brief DigitalEventSeries class for managing time-stamped discrete events
 * 
 * This file provides the DigitalEventSeries class, which represents a sorted collection
 * of discrete events, where each event is defined by a TimeFrameIndex (an index into
 * an accompanying TimeFrame) and an EntityId (a unique identifier for the event).
 * 
 * ## Storage Backends
 * 
 * DigitalEventSeries supports three storage backends via a type-erased wrapper:
 * 
 * ### Owning Storage (DigitalEventStorageType::Owning)
 * - **Default storage type** for newly created series
 * - Owns event data in Structure-of-Arrays (SoA) layout for cache efficiency
 * - Supports all mutation operations (addEvent, removeEvent, clear)
 * - Events are always maintained in sorted order by time
 * - O(log n) lookups by time, O(1) lookups by EntityId via hash map
 * 
 * ### View Storage (DigitalEventStorageType::View)
 * - **Zero-copy filtered view** of another series
 * - Created via createView() factory methods
 * - References source data via index vector; no data copying
 * - Supports filtering by time range or EntityId set
 * - **Read-only**: mutation operations throw std::runtime_error
 * - Returns valid cache if view indices are contiguous
 * 
 * ### Lazy Storage (DigitalEventStorageType::Lazy)
 * - **On-demand computation** from transform views
 * - Created via createFromView() template method
 * - Stores a C++20 ranges view that computes EventWithId on access
 * - Useful for transform pipelines without materializing intermediate results
 * - **Read-only**: mutation operations throw std::runtime_error
 * - Always returns invalid cache (forces virtual dispatch)
 * 
 * ## TimeFrame Integration
 * 
 * Each event's time is stored as a TimeFrameIndex, which is an index into the
 * series' associated TimeFrame. This enables:
 * - Different data sources to use different sampling rates
 * - Automatic time conversion when querying across timeframes
 * - Efficient range queries using binary search on indices
 * 
 * ## Entity System Integration
 * 
 * Each event can have an associated EntityId for tracking across the application.
 * When setIdentityContext() is called with an EntityRegistry, new events are
 * automatically assigned unique EntityIds. This enables:
 * - Linking events to analysis results
 * - Group-based selection and filtering
 * - Cross-data-type entity tracking
 * 
 * @see DigitalEventStorage.hpp for storage implementation details
 * @see EventWithId for the element type returned by iterators
 * @see TimeFrame for time base management
 * @see EntityRegistry for entity ID management
 */

#include "DigitalTimeSeries/EventWithId.hpp"
#include "Entity/EntityTypes.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "storage/DigitalEventStorage.hpp"

#include <compare>
#include <memory>
#include <ranges>
#include <unordered_set>
#include <utility>
#include <vector>

class EntityRegistry;


/**
 * @brief A sorted collection of discrete time events with entity tracking
 * 
 * DigitalEventSeries stores events as (TimeFrameIndex, EntityId) pairs, maintaining
 * events in sorted order by time. Each event represents a discrete occurrence at a
 * specific time point (compare to DigitalIntervalSeries for time ranges).
 * 
 * ## Primary Interface
 * 
 * - **view()**: Returns a lazy range of EventWithId objects for iteration
 * - **viewInRange()**: Returns events in a time range (requires TimeFrame for conversion)
 * - **addEvent()/removeEvent()**: Modify events (owning storage only)
 * - **createView()/createFromView()**: Create view/lazy backed series
 * - **materialize()**: Convert any storage type to owning storage
 * 
 * ## Storage Backends
 * 
 * The class supports three storage backends (Owning, View, Lazy) that provide
 * different performance tradeoffs. See @ref Digital_Event_Series.hpp for details.
 * 
 * ## Example Usage
 * 
 * ```cpp
 * // Create and populate a series
 * DigitalEventSeries series;
 * series.setTimeFrame(my_time_frame);
 * series.addEvent(TimeFrameIndex{100});
 * series.addEvent(TimeFrameIndex{200});
 * 
 * // Iterate all events
 * for (auto event : series.view()) {
 *     std::cout << "Event at " << event.time().getValue() 
 *               << " with id " << event.id().getValue() << "\n";
 * }
 * 
 * // Query range with timeframe conversion
 * for (auto event : series.viewInRange(start, end, other_timeframe)) {
 *     process(event);
 * }
 * 
 * // Create filtered view
 * auto filtered = DigitalEventSeries::createView(
 *     std::make_shared<DigitalEventSeries>(series),
 *     TimeFrameIndex{50}, TimeFrameIndex{150});
 * ```
 * 
 * @note Events are always sorted by time. Duplicate times are rejected.
 * @note View and Lazy storage are read-only; mutations throw std::runtime_error.
 * 
 * @see EventWithId for element accessors (time(), id(), value())
 * @see DigitalIntervalSeries for time interval data
 */
class DigitalEventSeries : public ObserverData {
public:
    /**
     * @brief Default constructor creating an empty series with owning storage
     */
    DigitalEventSeries();

    /**
     * @brief Construct from a vector of event times
     * 
     * Events are sorted and de-duplicated during construction.
     * 
     * @param event_vector Vector of event times (will be sorted/deduplicated)
     */
    explicit DigitalEventSeries(std::vector<TimeFrameIndex> event_vector);

    // =============================================================
    // ==================== RANGES INTERFACE =======================
    // =============================================================

    /**
     * @brief Get a std::ranges compatible view of the series.
     * 
     * Returns a random-access view that synthesizes EventWithId objects on demand.
     * Uses cached pointers for fast-path iteration when storage is contiguous.
     * Allows iterating over EventWithId objects directly.
     */
    [[nodiscard]] auto view() const {
        return std::views::iota(size_t{0}, size()) | std::views::transform([this](size_t idx) {
                   // Fast path: use cached pointers if valid
                   if (_cached_storage.isValid()) {
                       return EventWithId(
                               _cached_storage.getEvent(idx),
                               _cached_storage.getEntityId(idx));
                   }
                   // Slow path: virtual dispatch through wrapper
                   return EventWithId(
                           _storage.getEvent(idx),
                           _storage.getEntityId(idx));
               });
    }

    /**
     * @brief Add a new event at the specified time
     * 
     * Maintains sorted order. Duplicate times are rejected (no-op).
     * 
     * @param event_time The time index for the new event
     * @throws std::runtime_error if storage is View or Lazy (read-only)
     */
    void addEvent(TimeFrameIndex event_time);

    /**
     * @brief Remove an event at the specified time
     * 
     * @param event_time The time index of the event to remove
     * @return true if an event was removed, false if no event existed at that time
     * @throws std::runtime_error if storage is View or Lazy (read-only)
     */
    bool removeEvent(TimeFrameIndex event_time);

    /**
     * @brief Get the number of events in the series
     * @return Number of events
     */
    [[nodiscard]] size_t size() const { return _storage.size(); }

    /**
     * @brief Remove all events from the series
     * @throws std::runtime_error if storage is View or Lazy (read-only)
     */
    void clear();

    // ========== Range Queries (Public - Require TimeFrame) ==========

    /**
     * @brief Get events in a time range as a lazy view of EventWithId objects
     * 
     * This is the primary range query interface. It requires a source TimeFrame
     * parameter to enable proper time conversion when the query time base differs
     * from the series' internal time base.
     * 
     * @param start_index Start time index (inclusive)
     * @param stop_index Stop time index (inclusive)
     * @param source_time_frame The time frame that start_index/stop_index are expressed in
     * @return Lazy view of EventWithId objects in the range
     * 
     * @note If source_time_frame matches the series' time frame, no conversion occurs
     * @note If the series has no time frame set, indices are used directly
     */
    [[nodiscard]] auto viewInRange(TimeFrameIndex start_index,
                                   TimeFrameIndex stop_index,
                                   TimeFrame const & source_time_frame) const {
        auto [start_idx, end_idx] = _getTimeRangeIndices(start_index, stop_index, source_time_frame);
        return std::views::iota(start_idx, end_idx) | std::views::transform([this](size_t idx) {
                   return EventWithId(_storage.getEvent(idx), _storage.getEntityId(idx));
               });
    }

    /**
     * @brief Get just the TimeFrameIndex values in a range as a lazy view
     * 
     * @param start_index Start time index (inclusive)
     * @param stop_index Stop time index (inclusive)
     * @param source_time_frame The time frame that start_index/stop_index are expressed in
     * @return Lazy view of TimeFrameIndex values in the range
     */
    [[nodiscard]] auto viewTimesInRange(TimeFrameIndex start_index,
                                        TimeFrameIndex stop_index,
                                        TimeFrame const & source_time_frame) const {
        auto [start_idx, end_idx] = _getTimeRangeIndices(start_index, stop_index, source_time_frame);
        return std::views::iota(start_idx, end_idx) | std::views::transform([this](size_t idx) {
                   return _storage.getEvent(idx);
               });
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
     * @return std::shared_ptr<TimeFrame> The current time frame
     */
    [[nodiscard]] std::shared_ptr<TimeFrame> getTimeFrame() const { return _time_frame; }

    // ========== Identity ==========

    /**
     * @brief Set the identity context for automatic EntityId assignment
     * 
     * When set, new events added via addEvent() will automatically be assigned
     * unique EntityIds from the registry.
     * 
     * @param data_key The key identifying this data in the registry
     * @param registry Pointer to the EntityRegistry (must outlive this series)
     */
    void setIdentityContext(std::string const & data_key, EntityRegistry * registry) {
        _identity_data_key = data_key;
        _identity_registry = registry;
    }

    /**
     * @brief Rebuild all EntityIds for existing events
     * 
     * Assigns new unique EntityIds to all events using the current identity context.
     * Requires setIdentityContext() to have been called first.
     */
    void rebuildAllEntityIds();

    // ========== Storage Type Queries ==========

    /**
     * @brief Check if storage is a view (doesn't own data)
     */
    [[nodiscard]] bool isView() const { return _storage.isView(); }

    /**
     * @brief Check if storage is lazy-evaluated
     */
    [[nodiscard]] bool isLazy() const { return _storage.isLazy(); }

    /**
     * @brief Get the underlying storage type
     */
    [[nodiscard]] DigitalEventStorageType getStorageType() const { return _storage.getStorageType(); }

    // ========== View and Lazy Factory Methods ==========

    /**
     * @brief Create a view-based series filtering by time range
     * 
     * @param source Source series to create view from
     * @param start Start time (inclusive)
     * @param end End time (inclusive)
     * @return New series backed by view storage
     */
    static std::shared_ptr<DigitalEventSeries> createView(
            std::shared_ptr<DigitalEventSeries const> source,
            TimeFrameIndex start,
            TimeFrameIndex end);

    /**
     * @brief Create a view-based series filtering by EntityIds
     * 
     * @param source Source series to create view from
     * @param entity_ids Set of EntityIds to include
     * @return New series backed by view storage
     */
    static std::shared_ptr<DigitalEventSeries> createView(
            std::shared_ptr<DigitalEventSeries const> source,
            std::unordered_set<EntityId> const & entity_ids);

    /**
     * @brief Create a lazy-evaluated series from a transform view
     * 
     * @tparam ViewType Random-access range type yielding EventWithId-like objects
     * @param view The transform view
     * @param num_elements Number of elements in the view
     * @param time_frame Optional time frame for the new series
     * @return New series backed by lazy storage
     */
    template<typename ViewType>
    static std::shared_ptr<DigitalEventSeries> createFromView(
            ViewType view,
            size_t num_elements,
            std::shared_ptr<TimeFrame> time_frame = nullptr);

    /**
     * @brief Materialize a lazy or view series into owning storage
     * 
     * Copies all events from the current storage into a new series with
     * owning storage. Useful when lazy evaluation is no longer desirable.
     * 
     * @return New series with owning storage containing all events
     */
    [[nodiscard]] std::shared_ptr<DigitalEventSeries> materialize() const;

private:
    DigitalEventStorageWrapper _storage;
    DigitalEventStorageCache _cached_storage;// Fast-path cache
    std::shared_ptr<TimeFrame> _time_frame{nullptr};

    // ========== Private Range Query Helpers ==========

    /**
     * @brief Convert time range from source time frame to storage indices
     * 
     * @param start_index Start time index in source_time_frame
     * @param stop_index Stop time index in source_time_frame
     * @param source_time_frame The time frame the indices are expressed in
     * @return Pair of (start_storage_idx, end_storage_idx) for use with storage
     */
    [[nodiscard]] std::pair<size_t, size_t> _getTimeRangeIndices(
            TimeFrameIndex start_index,
            TimeFrameIndex stop_index,
            TimeFrame const & source_time_frame) const {
        // Fast path: same time frame or no conversion needed
        if (&source_time_frame == _time_frame.get() || !_time_frame.get()) {
            return _storage.getTimeRange(start_index, stop_index);
        }
        // Convert to our time frame
        auto [target_start, target_stop] = convertTimeFrameRange(
                start_index, stop_index, source_time_frame, *_time_frame);
        return _storage.getTimeRange(target_start, target_stop);
    }

    // Cache management
    void _cacheOptimizationPointers();

    // Identity
    std::string _identity_data_key;
    EntityRegistry * _identity_registry{nullptr};
};

// =============================================================================
// Template Implementation
// =============================================================================

template<typename ViewType>
std::shared_ptr<DigitalEventSeries> DigitalEventSeries::createFromView(
        ViewType view,
        size_t num_elements,
        std::shared_ptr<TimeFrame> time_frame) {
    auto result = std::make_shared<DigitalEventSeries>();
    result->_storage = DigitalEventStorageWrapper{
            LazyDigitalEventStorage<ViewType>{std::move(view), num_elements}};
    result->_time_frame = std::move(time_frame);
    result->_cacheOptimizationPointers();
    return result;
}

#endif//BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
