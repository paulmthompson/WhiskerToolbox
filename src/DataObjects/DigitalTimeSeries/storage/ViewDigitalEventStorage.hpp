/**
 * @file ViewDigitalEventStorage.hpp
 * @brief View-based digital event storage backend.
 */

#ifndef VIEW_DIGITAL_EVENT_STORAGE_HPP
#define VIEW_DIGITAL_EVENT_STORAGE_HPP

#include "DigitalEventStorageBase.hpp"
#include "DigitalEventStorageCache.hpp"

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class OwningDigitalEventStorage;

// =============================================================================
// View Storage (References Source via Indices)
// =============================================================================

/**
 * @brief View-based digital event storage that references another storage.
 *
 * Holds a shared_ptr to a source OwningDigitalEventStorage and a vector of indices
 * into that source. Enables zero-copy filtered views.
 */
class ViewDigitalEventStorage : public DigitalEventStorageBase<ViewDigitalEventStorage> {
public:
    /**
     * @brief Construct a view referencing source storage.
     *
     * @param source Shared pointer to source storage.
     */
    explicit ViewDigitalEventStorage(std::shared_ptr<OwningDigitalEventStorage const> source);

    /**
     * @brief Set the indices this view includes.
     */
    void setIndices(std::vector<size_t> indices);

    /**
     * @brief Create view of all events.
     */
    void setAllIndices();

    /**
     * @brief Filter by time range [start, end] inclusive.
     */
    void filterByTimeRange(TimeFrameIndex start, TimeFrameIndex end);

    /**
     * @brief Filter by EntityId set.
     */
    void filterByEntityIds(std::unordered_set<EntityId> const & ids);

    /**
     * @brief Get the source storage.
     */
    [[nodiscard]] std::shared_ptr<OwningDigitalEventStorage const> source() const;

    /**
     * @brief Get the indices vector.
     */
    [[nodiscard]] std::vector<size_t> const & indices() const {
        return _indices;
    }

    // ========== CRTP Implementation ==========

    [[nodiscard]] size_t sizeImpl() const { return _indices.size(); }

    [[nodiscard]] TimeFrameIndex getEventImpl(size_t idx) const;

    [[nodiscard]] EntityId getEntityIdImpl(size_t idx) const;

    [[nodiscard]] std::optional<size_t> findByTimeImpl(TimeFrameIndex time) const;

    [[nodiscard]] std::optional<size_t> findByEntityIdImpl(EntityId id) const;

    [[nodiscard]] std::pair<size_t, size_t> getTimeRangeImpl(TimeFrameIndex start, TimeFrameIndex end) const;

    [[nodiscard]] DigitalEventStorageType getStorageTypeImpl() const {
        return DigitalEventStorageType::View;
    }

    /**
     * @brief Return cache if view is contiguous.
     */
    [[nodiscard]] DigitalEventStorageCache tryGetCacheImpl() const;

private:
    void _rebuildLocalIndices();

    std::shared_ptr<OwningDigitalEventStorage const> _source;
    std::vector<size_t> _indices;
    std::unordered_map<EntityId, size_t> _local_entity_id_to_index;
};

#endif// VIEW_DIGITAL_EVENT_STORAGE_HPP
