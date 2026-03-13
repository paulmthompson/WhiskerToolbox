/**
 * @file ViewDigitalEventStorage.cpp
 * @brief Implementation of view-based digital event storage backend.
 */

#include "ViewDigitalEventStorage.hpp"

#include "OwningDigitalEventStorage.hpp"

#include <algorithm>
#include <numeric>

ViewDigitalEventStorage::ViewDigitalEventStorage(std::shared_ptr<OwningDigitalEventStorage const> source)
    : _source(std::move(source)) {}

void ViewDigitalEventStorage::setIndices(std::vector<size_t> indices) {
    _indices = std::move(indices);
    _rebuildLocalIndices();
}

void ViewDigitalEventStorage::setAllIndices() {
    _indices.resize(_source->size());
    std::iota(_indices.begin(), _indices.end(), 0);
    _rebuildLocalIndices();
}

void ViewDigitalEventStorage::filterByTimeRange(TimeFrameIndex start, TimeFrameIndex end) {
    auto [src_start, src_end] = _source->getTimeRange(start, end);

    _indices.clear();
    _indices.reserve(src_end - src_start);

    for (size_t i = src_start; i < src_end; ++i) {
        _indices.push_back(i);
    }

    _rebuildLocalIndices();
}

void ViewDigitalEventStorage::filterByEntityIds(std::unordered_set<EntityId> const & ids) {
    _indices.clear();

    for (size_t i = 0; i < _source->size(); ++i) {
        if (ids.contains(_source->getEntityId(i))) {
            _indices.push_back(i);
        }
    }

    _rebuildLocalIndices();
}

std::shared_ptr<OwningDigitalEventStorage const> ViewDigitalEventStorage::source() const {
    return _source;
}

TimeFrameIndex ViewDigitalEventStorage::getEventImpl(size_t idx) const {
    return _source->getEvent(_indices[idx]);
}

EntityId ViewDigitalEventStorage::getEntityIdImpl(size_t idx) const {
    return _source->getEntityId(_indices[idx]);
}

std::optional<size_t> ViewDigitalEventStorage::findByTimeImpl(TimeFrameIndex time) const {
    auto it = std::ranges::lower_bound(_indices, time, {}, [this](size_t idx) { return _source->getEvent(idx); });

    if (it != _indices.end() && _source->getEvent(*it) == time) {
        return static_cast<size_t>(std::distance(_indices.begin(), it));
    }
    return std::nullopt;
}

std::optional<size_t> ViewDigitalEventStorage::findByEntityIdImpl(EntityId id) const {
    auto it = _local_entity_id_to_index.find(id);
    return it != _local_entity_id_to_index.end() ? std::optional{it->second} : std::nullopt;
}

std::pair<size_t, size_t> ViewDigitalEventStorage::getTimeRangeImpl(TimeFrameIndex start,
                                                                    TimeFrameIndex end) const {
    auto it_start = std::ranges::lower_bound(_indices, start, {}, [this](size_t idx) {
        return _source->getEvent(idx);
    });
    auto it_end = std::ranges::upper_bound(_indices, end, {}, [this](size_t idx) {
        return _source->getEvent(idx);
    });

    return {static_cast<size_t>(std::distance(_indices.begin(), it_start)),
            static_cast<size_t>(std::distance(_indices.begin(), it_end))};
}

DigitalEventStorageCache ViewDigitalEventStorage::tryGetCacheImpl() const {
    if (_indices.empty()) {
        return DigitalEventStorageCache{nullptr, nullptr, 0, true};
    }

    size_t const start_idx = _indices[0];
    bool is_contiguous = true;

    for (size_t i = 1; i < _indices.size(); ++i) {
        if (_indices[i] != start_idx + i) {
            is_contiguous = false;
            break;
        }
    }

    if (is_contiguous) {
        return DigitalEventStorageCache{
                _source->events().data() + start_idx,
                _source->entityIds().data() + start_idx,
                _indices.size(),
                true};
    }

    return DigitalEventStorageCache{};
}

void ViewDigitalEventStorage::_rebuildLocalIndices() {
    _local_entity_id_to_index.clear();
    for (size_t i = 0; i < _indices.size(); ++i) {
        EntityId const id = _source->getEntityId(_indices[i]);
        _local_entity_id_to_index[id] = i;
    }
}
