#ifndef OWNING_DIGITAL_EVENT_STORAGE_HPP
#define OWNING_DIGITAL_EVENT_STORAGE_HPP

#include "DigitalEventStorageBase.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <vector>
#include <optional>
#include <utility>
#include <span>
#include <unordered_map>

/**
 * @brief Owning digital event storage using Structure of Arrays layout
 *
 * Stores event data in parallel vectors for cache-friendly access:
 * - _events[i] - TimeFrameIndex for event i (sorted)
 * - _entity_ids[i] - EntityId for event i
 *
 * Maintains acceleration structures:
 * - Events are always sorted by time
 * - O(log n) lookup by time using binary search
 * - O(1) lookup by EntityId using hash map
 */
class OwningDigitalEventStorage : public DigitalEventStorageBase<OwningDigitalEventStorage> {
public:
    OwningDigitalEventStorage() = default;

    /**
     * @brief Construct from existing event vector (will sort)
     */
    explicit OwningDigitalEventStorage(std::vector<TimeFrameIndex> events) {
        _events = std::move(events);
        _sortEvents();
        // Entity IDs will be empty until rebuilt
        _entity_ids.resize(_events.size(), EntityId{0});
    }

    /**
     * @brief Construct from existing event and entity ID vectors
     * @note Both vectors must be the same size
     */
    OwningDigitalEventStorage(std::vector<TimeFrameIndex> events, std::vector<EntityId> entity_ids) {
        if (events.size() != entity_ids.size()) {
            throw std::invalid_argument("Events and entity_ids must have same size");
        }
        _events = std::move(events);
        _entity_ids = std::move(entity_ids);
        _sortEventsWithEntityIds();
        _rebuildEntityIdIndex();
    }

    // ========== Modification ==========

    /**
     * @brief Add an event at the specified time
     *
     * If an event already exists at this exact time, it is not added (returns false).
     *
     * @param time The event time
     * @param entity_id The EntityId for this event
     * @return true if added, false if duplicate time
     */
        bool addEvent(TimeFrameIndex time, EntityId entity_id);

    /**
     * @brief Remove an event at the specified time
     * @return true if removed, false if not found
     */
        bool removeEvent(TimeFrameIndex time);

    /**
     * @brief Remove an event by EntityId
     * @return true if removed, false if not found
     */
        bool removeByEntityId(EntityId id);

    /**
     * @brief Clear all events
     */
        void clear();

    /**
     * @brief Reserve capacity for expected number of events
     */
        void reserve(size_t capacity);

    /**
     * @brief Set all entity IDs (must match event count)
     */
            void setEntityIds(std::vector<EntityId> ids);

    // ========== CRTP Implementation ==========

    [[nodiscard]] size_t sizeImpl() const { return _events.size(); }

    [[nodiscard]] TimeFrameIndex getEventImpl(size_t idx) const { return _events[idx]; }

    [[nodiscard]] EntityId getEntityIdImpl(size_t idx) const {
        return idx < _entity_ids.size() ? _entity_ids[idx] : EntityId{0};
    }

    [[nodiscard]] std::optional<size_t> findByTimeImpl(TimeFrameIndex time) const {
        auto it = std::ranges::lower_bound(_events, time);
        if (it != _events.end() && *it == time) {
            return static_cast<size_t>(std::distance(_events.begin(), it));
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<size_t> findByEntityIdImpl(EntityId id) const {
        auto it = _entity_id_to_index.find(id);
        return it != _entity_id_to_index.end() ? std::optional{it->second} : std::nullopt;
    }

    [[nodiscard]] std::pair<size_t, size_t> getTimeRangeImpl(TimeFrameIndex start, TimeFrameIndex end) const {
        auto it_start = std::ranges::lower_bound(_events, start);
        auto it_end = std::ranges::upper_bound(_events, end);

        return {
            static_cast<size_t>(std::distance(_events.begin(), it_start)),
            static_cast<size_t>(std::distance(_events.begin(), it_end))
        };
    }

    [[nodiscard]] DigitalEventStorageType getStorageTypeImpl() const {
        return DigitalEventStorageType::Owning;
    }

    /**
     * @brief Get cache with pointers to contiguous data
     *
     * OwningDigitalEventStorage stores data contiguously in SoA layout,
     * so it always returns a valid cache for fast-path iteration.
     */
    [[nodiscard]] DigitalEventStorageCache tryGetCacheImpl() const {
        return DigitalEventStorageCache{
            _events.data(),
            _entity_ids.data(),
            _events.size(),
            true  // is_contiguous
        };
    }

    // ========== Direct Array Access ==========

    [[nodiscard]] std::vector<TimeFrameIndex> const& events() const { return _events; }
    [[nodiscard]] std::vector<EntityId> const& entityIds() const { return _entity_ids; }

    [[nodiscard]] std::span<TimeFrameIndex const> eventsSpan() const { return _events; }
    [[nodiscard]] std::span<EntityId const> entityIdsSpan() const { return _entity_ids; }

private:
        void _sortEvents();

        void _sortEventsWithEntityIds();

        void _rebuildEntityIdIndex();

    std::vector<TimeFrameIndex> _events;
    std::vector<EntityId> _entity_ids;
    std::unordered_map<EntityId, size_t> _entity_id_to_index;
};

#endif // OWNING_DIGITAL_EVENT_STORAGE_HPP
