/**
 * @file RelativeOwningDigitalEventStorage.hpp
 * @brief Immutable owning digital event storage for relative ClockTicks.
 */

#ifndef RELATIVE_OWNING_DIGITAL_EVENT_STORAGE_HPP
#define RELATIVE_OWNING_DIGITAL_EVENT_STORAGE_HPP

#include "DigitalEventStorageBase.hpp"
#include "DigitalEventStorageCache.hpp"

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/ClockTicks.hpp"

#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

// =============================================================================
// Relative Owning Storage (Immutable SoA Layout)
// =============================================================================

/**
 * @brief Immutable owning storage for trial-relative digital event times.
 *
 * Stores event data in parallel vectors for cache-friendly access:
 * - _events[i]     - relative ClockTicks for event i (sorted)
 * - _entity_ids[i] - EntityId for event i
 *
 * Constructed once from caller-provided vectors; no mutation API is provided.
 * Does not require an associated TimeFrame.
 */
class RelativeOwningDigitalEventStorage : public DigitalEventStorageBase<RelativeOwningDigitalEventStorage> {
public:
    /**
     * @brief Construct from existing relative event vector (will sort and deduplicate).
     * @param events Relative event times (sorted and deduplicated in place).
     */
    explicit RelativeOwningDigitalEventStorage(std::vector<ClockTicks> events);

    /**
     * @brief Construct from existing relative event and entity ID vectors.
     * @param events Relative event times (sorted and deduplicated in place).
     * @param entity_ids Entity IDs aligned with events before sorting.
     * @pre events.size() == entity_ids.size()
     */
    RelativeOwningDigitalEventStorage(std::vector<ClockTicks> events, std::vector<EntityId> entity_ids);

    // ========== CRTP Implementation ==========

    [[nodiscard]] size_t sizeImpl() const { return _events.size(); }

    [[nodiscard]] static TimeFrameIndex getEventImpl(size_t idx) ;

    [[nodiscard]] EntityId getEntityIdImpl(size_t idx) const {
        return idx < _entity_ids.size() ? _entity_ids[idx] : EntityId{0};
    }

    [[nodiscard]] static std::optional<size_t> findByTimeImpl(TimeFrameIndex time) ;

    [[nodiscard]] std::optional<size_t> findByEntityIdImpl(EntityId id) const;

    [[nodiscard]] static std::pair<size_t, size_t> getTimeRangeImpl(TimeFrameIndex start, TimeFrameIndex end) ;

    [[nodiscard]] DigitalEventTimeDomain getTimeDomainImpl() const {
        return DigitalEventTimeDomain::RelativeClockTicks;
    }

    [[nodiscard]] ClockTicks getRelativeEventImpl(size_t idx) const { return _events[idx]; }

    [[nodiscard]] std::optional<size_t> findByRelativeTimeImpl(ClockTicks time) const;

    [[nodiscard]] std::pair<size_t, size_t> getRelativeTimeRangeImpl(ClockTicks start, ClockTicks end) const;

    [[nodiscard]] DigitalEventStorageType getStorageTypeImpl() const {
        return DigitalEventStorageType::RelativeOwning;
    }

    /**
     * @brief Get cache with pointers to contiguous relative event data.
     */
    [[nodiscard]] DigitalEventStorageCache tryGetCacheImpl() const {
        return DigitalEventStorageCache{
                DigitalEventTimeDomain::RelativeClockTicks,
                nullptr,
                _events.data(),
                _entity_ids.data(),
                _events.size(),
                true};
    }

    // ========== Direct Array Access ==========

    [[nodiscard]] std::vector<ClockTicks> const & events() const { return _events; }
    [[nodiscard]] std::vector<EntityId> const & entityIds() const { return _entity_ids; }

    [[nodiscard]] std::span<ClockTicks const> eventsSpan() const { return _events; }
    [[nodiscard]] std::span<EntityId const> entityIdsSpan() const { return _entity_ids; }

private:
    void _sortEvents();

    void _sortEventsWithEntityIds();

    void _rebuildEntityIdIndex() {
        _entity_id_to_index.clear();
        for (size_t i = 0; i < _entity_ids.size(); ++i) {
            _entity_id_to_index[_entity_ids[i]] = i;
        }
    }

    std::vector<ClockTicks> _events;
    std::vector<EntityId> _entity_ids;
    std::unordered_map<EntityId, size_t> _entity_id_to_index;
};

#endif// RELATIVE_OWNING_DIGITAL_EVENT_STORAGE_HPP
