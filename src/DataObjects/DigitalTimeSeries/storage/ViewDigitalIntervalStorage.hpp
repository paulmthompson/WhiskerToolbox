#ifndef VIEW_DIGITAL_INTERVAL_STORAGE_HPP
#define VIEW_DIGITAL_INTERVAL_STORAGE_HPP

#include "DigitalIntervalStorageBase.hpp"
#include "DigitalIntervalStorageCache.hpp"

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/interval_data.hpp"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class OwningDigitalIntervalStorage;

// =============================================================================
// View Storage (References Source via Indices)
// =============================================================================

/**
 * @brief View-based digital interval storage that references another storage
 * 
 * Holds a shared_ptr to a source OwningDigitalIntervalStorage and a vector of indices
 * into that source. Enables zero-copy filtered views.
 */
class ViewDigitalIntervalStorage : public DigitalIntervalStorageBase<ViewDigitalIntervalStorage> {
public:
    /**
     * @brief Construct a view referencing source storage
     * 
     * @param source Shared pointer to source storage
     */
    explicit ViewDigitalIntervalStorage(std::shared_ptr<OwningDigitalIntervalStorage const> source);

    /**
     * @brief Set the indices this view includes
     */
    void setIndices(std::vector<size_t> indices);

    /**
     * @brief Create view of all intervals
     */
    void setAllIndices();

    /**
     * @brief Filter by overlapping time range [start, end].
     *
     * Delegates to the source owning storage when
     * `source()->assumeDisjointIntervals()` is true (O(log n) on source size), otherwise
     * uses @ref filterByOverlappingRangeLinear().
     *
     * @see filterByOverlappingRangeLinear()
     * @see OwningDigitalIntervalStorage::getOverlappingRangeImpl()
     * @see LazyDigitalIntervalStorage::getOverlappingRangeImpl()
     */
    void filterByOverlappingRange(TimeFrameIndex start, TimeFrameIndex end);

    /**
     * @brief Filter by overlapping time range [start, end] using linear scan.
     *
     * O(n) over source intervals; correct when intervals may overlap. Same overlap test
     * as @ref OwningDigitalIntervalStorage::getOverlappingRangeImpl() in overlapping mode
     * and @ref LazyDigitalIntervalStorage::getOverlappingRangeImpl().
     *
     * @see filterByOverlappingRange()
     * @see OwningDigitalIntervalStorage::getOverlappingRangeImpl()
     */
    void filterByOverlappingRangeLinear(TimeFrameIndex start, TimeFrameIndex end);

    /**
     * @brief Filter by contained time range [start, end]
     */
    void filterByContainedRange(TimeFrameIndex start, TimeFrameIndex end);

    /**
     * @brief Filter by EntityId set
     */
    void filterByEntityIds(std::unordered_set<EntityId> const & ids);

    /**
     * @brief Get the source storage
     */
    [[nodiscard]] std::shared_ptr<OwningDigitalIntervalStorage const> source() const;

    /**
     * @brief Get the indices vector
     */
    [[nodiscard]] std::vector<size_t> const & indices() const {
        return _indices;
    }

    // ========== CRTP Implementation ==========

    [[nodiscard]] size_t sizeImpl() const { return _indices.size(); }

    [[nodiscard]] TimeFrameInterval const & getIntervalImpl(size_t idx) const;

    [[nodiscard]] EntityId getEntityIdImpl(size_t idx) const;

    [[nodiscard]] std::optional<size_t> findByIntervalImpl(TimeFrameInterval const & interval) const;

    [[nodiscard]] std::optional<size_t> findByEntityIdImpl(EntityId id) const;

    [[nodiscard]] bool hasIntervalAtTimeImpl(TimeFrameIndex time) const;

    /**
     * @brief Get index range of view-local indices overlapping [start, end].
     *
     * **Disjoint fast path** (`source()->assumeDisjointIntervals() == true`): O(log n)
     * binary search over view indices (requires disjoint source intervals).
     *
     * **Overlapping fallback**: O(n) linear scan over view indices.
     *
     * @see filterByOverlappingRange()
     * @see OwningDigitalIntervalStorage::getOverlappingRangeImpl()
     * @see LazyDigitalIntervalStorage::getOverlappingRangeImpl()
     */
    [[nodiscard]] std::pair<size_t, size_t> getOverlappingRangeImpl(TimeFrameIndex start, TimeFrameIndex end) const;

    [[nodiscard]] std::pair<size_t, size_t> getContainedRangeImpl(TimeFrameIndex start, TimeFrameIndex end) const;

    [[nodiscard]] DigitalIntervalStorageType getStorageTypeImpl() const {
        return DigitalIntervalStorageType::View;
    }

    /**
     * @brief Return cache if view is contiguous
     */
    [[nodiscard]] DigitalIntervalStorageCache tryGetCacheImpl() const;

private:
    void _rebuildLocalIndices();

    std::shared_ptr<OwningDigitalIntervalStorage const> _source;
    std::vector<size_t> _indices;
    std::unordered_map<EntityId, size_t> _local_entity_id_to_index;
};


#endif// VIEW_DIGITAL_INTERVAL_STORAGE_HPP