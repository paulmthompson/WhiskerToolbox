#ifndef BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
#define BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP

#include "DigitalEventStorage.hpp"
#include "DigitalTimeSeries/EventWithId.hpp"
#include "Entity/EntityTypes.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <compare>
#include <memory>
#include <ranges>
#include <unordered_set>
#include <utility>
#include <vector>

class EntityRegistry;


/**
 * @brief Digital Event Series class
 *
 * A digital event series is a series of events where each event is defined by a time.
 * (Compare to DigitalIntervalSeries which is a series of intervals with start and end times)
 *
 * Use digital events where you wish to specify a time for each event.
 *
 * Now backed by flexible storage abstraction supporting:
 * - Owning storage (default): owns data in SoA layout
 * - View storage: zero-copy filtered view of another series
 * - Lazy storage: on-demand computation from transforms
 */
class DigitalEventSeries : public ObserverData {
public:
    DigitalEventSeries();
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
        return std::views::iota(size_t{0}, size())
             | std::views::transform([this](size_t idx) {
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


    [[nodiscard]] std::vector<TimeFrameIndex> const & getEventSeries() const;

    void addEvent(TimeFrameIndex event_time);

    bool removeEvent(TimeFrameIndex event_time);

    [[nodiscard]] size_t size() const { return _storage.size(); }

    void clear();

    [[nodiscard]] auto getEventsInRange(TimeFrameIndex start_time, TimeFrameIndex stop_time) const {
        // Use storage's getTimeRange for efficient range lookup
        auto [start_idx, end_idx] = _storage.getTimeRange(start_time, stop_time);

        return std::views::iota(start_idx, end_idx) | std::views::transform([this](size_t idx) {
                   return _storage.getEvent(idx);
               });
    }

    [[nodiscard]] auto getEventsInRange(TimeFrameIndex start_index,
                                        TimeFrameIndex stop_index,
                                        TimeFrame const & source_time_frame) const {
        if (&source_time_frame == _time_frame.get()) {
            return getEventsInRange(start_index, stop_index);
        }

        // If either timeframe is null, fall back to original behavior
        if (!_time_frame.get()) {
            return getEventsInRange(start_index, stop_index);
        }

        // Use helper function for time frame conversion
        auto [target_start_index, target_stop_index] = convertTimeFrameRange(start_index, stop_index, source_time_frame, *_time_frame);
        return getEventsInRange(target_start_index, target_stop_index);
    };

    std::vector<TimeFrameIndex> getEventsAsVector(TimeFrameIndex start_time, TimeFrameIndex stop_time) const {
        auto [start_idx, end_idx] = _storage.getTimeRange(start_time, stop_time);

        std::vector<TimeFrameIndex> result;
        if (end_idx > start_idx) {
            result.reserve(end_idx - start_idx);

            for (size_t i = start_idx; i < end_idx; ++i) {
                result.push_back(_storage.getEvent(i));
            }
        }
        return result;
    }

    // ========== Events with EntityIDs ==========

    /**
     * @brief Get events in range with their EntityIDs using TimeFrameIndex
     * 
     * @param start_time Start time index for the range
     * @param stop_time Stop time index for the range
     * @return std::vector<EventWithId> Vector of events with their EntityIDs
     */
    [[nodiscard]] std::vector<EventWithId> getEventsWithIdsInRange(TimeFrameIndex start_time,
                                                                   TimeFrameIndex stop_time) const;

    /**
     * @brief Get events in range with their EntityIDs using time frame conversion
     * 
     * @param start_index Start time index in source timeframe
     * @param stop_index Stop time index in source timeframe
     * @param source_time_frame Source timeframe for the indices
     * @return std::vector<EventWithId> Vector of events with their EntityIDs
     */
    [[nodiscard]] std::vector<EventWithId> getEventsWithIdsInRange(TimeFrameIndex start_index,
                                                                   TimeFrameIndex stop_index,
                                                                   TimeFrame const & source_time_frame) const;

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

    // ===== Identity =====
    void setIdentityContext(std::string const & data_key, EntityRegistry * registry) {
        _identity_data_key = data_key;
        _identity_registry = registry;
    }
    void rebuildAllEntityIds();
    [[nodiscard]] std::vector<EntityId> const & getEntityIds() const;

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

    // Cache management
    void _cacheOptimizationPointers();

    // Legacy interface support
    mutable std::vector<TimeFrameIndex> _legacy_event_vector;
    mutable bool _legacy_vector_valid{false};
    mutable std::vector<EntityId> _legacy_entity_id_vector;
    mutable bool _legacy_entity_id_valid{false};

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
