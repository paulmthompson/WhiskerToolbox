#ifndef OWNING_DIGITAL_INTERVAL_STORAGE_HPP
#define OWNING_DIGITAL_INTERVAL_STORAGE_HPP

#include "DigitalIntervalStorageBase.hpp"
#include "DigitalIntervalStorageCache.hpp"

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include <optional>
#include <span>
#include <vector>

// =============================================================================
// Owning Storage (SoA Layout)
// =============================================================================

/**
 * @brief Owning digital interval storage using Structure of Arrays layout
 * 
 * Stores interval data in parallel vectors for cache-friendly access:
 * - _intervals[i] - Interval for entry i (sorted by start time)
 * - _entity_ids[i] - EntityId for entry i
 * 
 * Maintains acceleration structures:
 * - Intervals are always sorted by start time
 * - O(log n) lookup by start time using binary search
 * - O(1) lookup by EntityId using hash map
 */
class OwningDigitalIntervalStorage : public DigitalIntervalStorageBase<OwningDigitalIntervalStorage> {
public:
    OwningDigitalIntervalStorage() = default;

    /**
     * @brief Construct from existing interval vector (will sort)
     */
    explicit OwningDigitalIntervalStorage(std::vector<Interval> intervals);

    /**
     * @brief Construct from existing interval and entity ID vectors
     * @note Both vectors must be the same size
     */
    OwningDigitalIntervalStorage(std::vector<Interval> intervals, std::vector<EntityId> entity_ids);

    // ========== Modification ==========

    /**
     * @brief Add an interval
     * 
     * If an interval with the same start/end already exists, it is not added (returns false).
     * 
     * @param interval The interval to add
     * @param entity_id The EntityId for this interval
     * @return true if added, false if duplicate
     */
    bool addInterval(Interval const & interval, EntityId entity_id);

    /**
     * @brief Remove an interval by exact match
     * @return true if removed, false if not found
     */
    bool removeInterval(Interval const & interval);

    /**
     * @brief Remove an interval by EntityId
     * @return true if removed, false if not found
     */
    bool removeByEntityId(EntityId id);

    /**
     * @brief Clear all intervals
     */
    void clear();

    /**
     * @brief Reserve capacity for expected number of intervals
     */
    void reserve(size_t capacity);

    /**
     * @brief Set all entity IDs (must match interval count)
     */
    void setEntityIds(std::vector<EntityId> ids);

    /**
     * @brief Set entity ID at a specific index
     */
    void setEntityId(size_t idx, EntityId id);

    /**
     * @brief Set interval at a specific index (does not re-sort)
     */
    void setInterval(size_t idx, Interval interval);

    /**
     * @brief Remove interval at a specific index
     */
    void removeAt(size_t idx);

    /**
     * @brief Sort intervals and entity IDs together
     */
    void sort();

    // ========== CRTP Implementation ==========

    [[nodiscard]] size_t sizeImpl() const { return _intervals.size(); }

    [[nodiscard]] Interval const & getIntervalImpl(size_t idx) const { return _intervals[idx]; }

    [[nodiscard]] EntityId getEntityIdImpl(size_t idx) const {
        return idx < _entity_ids.size() ? _entity_ids[idx] : EntityId{0};
    }

    [[nodiscard]] std::optional<size_t> findByIntervalImpl(Interval const & interval) const;

    [[nodiscard]] std::optional<size_t> findByEntityIdImpl(EntityId id) const;

    [[nodiscard]] bool hasIntervalAtTimeImpl(int64_t time) const;

    [[nodiscard]] std::pair<size_t, size_t> getOverlappingRangeImpl(int64_t start, int64_t end) const;

    [[nodiscard]] std::pair<size_t, size_t> getContainedRangeImpl(int64_t start, int64_t end) const;

    [[nodiscard]] DigitalIntervalStorageType getStorageTypeImpl() const {
        return DigitalIntervalStorageType::Owning;
    }

    /**
     * @brief Get cache with pointers to contiguous data
     * 
     * OwningDigitalIntervalStorage stores data contiguously in SoA layout,
     * so it always returns a valid cache for fast-path iteration.
     */
    [[nodiscard]] DigitalIntervalStorageCache tryGetCacheImpl() const {
        return DigitalIntervalStorageCache{
                _intervals.data(),
                _entity_ids.data(),
                _intervals.size(),
                true// is_contiguous
        };
    }

    // ========== Direct Array Access ==========

    [[nodiscard]] std::vector<Interval> const & intervals() const { return _intervals; }
    [[nodiscard]] std::vector<EntityId> const & entityIds() const { return _entity_ids; }

    [[nodiscard]] std::span<Interval const> intervalsSpan() const { return _intervals; }
    [[nodiscard]] std::span<EntityId const> entityIdsSpan() const { return _entity_ids; }

private:
    void _sortIntervals();

    void _sortIntervalsWithEntityIds();

    void _rebuildEntityIdIndex() {
        _entity_id_to_index.clear();
        for (size_t i = 0; i < _entity_ids.size(); ++i) {
            _entity_id_to_index[_entity_ids[i]] = i;
        }
    }

    std::vector<Interval> _intervals;
    std::vector<EntityId> _entity_ids;
    std::unordered_map<EntityId, size_t> _entity_id_to_index;
};


#endif// OWNING_DIGITAL_INTERVAL_STORAGE_HPP