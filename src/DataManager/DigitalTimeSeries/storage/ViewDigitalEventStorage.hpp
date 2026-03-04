#ifndef VIEW_DIGITAL_EVENT_STORAGE_HPP
#define VIEW_DIGITAL_EVENT_STORAGE_HPP

#include "DigitalEventStorageBase.hpp"
#include "OwningDigitalEventStorage.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>
#include <vector>
#include <optional>
#include <utility>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <numeric>

/**
 * @brief View-based digital event storage that references another storage
 *
 * Holds a shared_ptr to a source OwningDigitalEventStorage and a vector of indices
 * into that source. Enables zero-copy filtered views.
 */
class ViewDigitalEventStorage : public DigitalEventStorageBase<ViewDigitalEventStorage> {
public:
    /**
     * @brief Construct a view referencing source storage
     *
     * @param source Shared pointer to source storage
     */
    explicit ViewDigitalEventStorage(std::shared_ptr<OwningDigitalEventStorage const> source)
        : _source(std::move(source)) {}

    /**
     * @brief Set the indices this view includes
     */
    void setIndices(std::vector<size_t> indices) {
        _indices = std::move(indices);
        _rebuildLocalIndices();
    }

    /**
     * @brief Create view of all events
     */
    void setAllIndices() {
        _indices.resize(_source->size());
        std::iota(_indices.begin(), _indices.end(), 0);
        _rebuildLocalIndices();
    }

    /**
     * @brief Filter by time range [start, end] inclusive
     */
    void filterByTimeRange(TimeFrameIndex start, TimeFrameIndex end) {
        auto [src_start, src_end] = _source->getTimeRange(start, end);

        _indices.clear();
        _indices.reserve(src_end - src_start);

        for (size_t i = src_start; i < src_end; ++i) {
            _indices.push_back(i);
        }

        _rebuildLocalIndices();
    }

    /**
     * @brief Filter by EntityId set
     */
    void filterByEntityIds(std::unordered_set<EntityId> const& ids) {
        _indices.clear();

        for (size_t i = 0; i < _source->size(); ++i) {
            if (ids.contains(_source->getEntityId(i))) {
                _indices.push_back(i);
            }
        }

        _rebuildLocalIndices();
    }

    /**
     * @brief Get the source storage
     */
    [[nodiscard]] std::shared_ptr<OwningDigitalEventStorage const> source() const {
        return _source;
    }

    /**
     * @brief Get the indices vector
     */
    [[nodiscard]] std::vector<size_t> const& indices() const {
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
     * @brief Return cache if view is contiguous
     */
        [[nodiscard]] DigitalEventStorageCache tryGetCacheImpl() const;

private:
    void _rebuildLocalIndices() {
        _local_entity_id_to_index.clear();
        for (size_t i = 0; i < _indices.size(); ++i) {
            EntityId const id = _source->getEntityId(_indices[i]);
            _local_entity_id_to_index[id] = i;
        }
    }

    std::shared_ptr<OwningDigitalEventStorage const> _source;
    std::vector<size_t> _indices;
    std::unordered_map<EntityId, size_t> _local_entity_id_to_index;
};

#endif // VIEW_DIGITAL_EVENT_STORAGE_HPP
