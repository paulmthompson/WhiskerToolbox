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
     * @brief Filter by overlapping time range [start, end]
     */
    void filterByOverlappingRange(int64_t start, int64_t end);

    /**
     * @brief Filter by contained time range [start, end]
     */
    void filterByContainedRange(int64_t start, int64_t end);

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

    [[nodiscard]] Interval const & getIntervalImpl(size_t idx) const;

    [[nodiscard]] EntityId getEntityIdImpl(size_t idx) const;

    [[nodiscard]] std::optional<size_t> findByIntervalImpl(Interval const & interval) const;

    [[nodiscard]] std::optional<size_t> findByEntityIdImpl(EntityId id) const;

    [[nodiscard]] bool hasIntervalAtTimeImpl(int64_t time) const;

    [[nodiscard]] std::pair<size_t, size_t> getOverlappingRangeImpl(int64_t start, int64_t end) const;

    [[nodiscard]] std::pair<size_t, size_t> getContainedRangeImpl(int64_t start, int64_t end) const;

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