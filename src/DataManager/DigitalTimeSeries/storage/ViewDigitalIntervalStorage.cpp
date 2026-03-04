
#include "ViewDigitalIntervalStorage.hpp"

#include "OwningDigitalIntervalStorage.hpp"

#include <algorithm>
#include <numeric>

ViewDigitalIntervalStorage::ViewDigitalIntervalStorage(std::shared_ptr<OwningDigitalIntervalStorage const> source)
    : _source(std::move(source)) {}


void ViewDigitalIntervalStorage::setIndices(std::vector<size_t> indices) {
    _indices = std::move(indices);
    _rebuildLocalIndices();
}

void ViewDigitalIntervalStorage::setAllIndices() {
    _indices.resize(_source->size());
    std::iota(_indices.begin(), _indices.end(), 0);
    _rebuildLocalIndices();
}

void ViewDigitalIntervalStorage::filterByOverlappingRange(int64_t start, int64_t end) {
    auto [src_start, src_end] = _source->getOverlappingRange(start, end);

    _indices.clear();
    _indices.reserve(src_end - src_start);

    for (size_t i = src_start; i < src_end; ++i) {
        _indices.push_back(i);
    }

    _rebuildLocalIndices();
}

void ViewDigitalIntervalStorage::filterByContainedRange(int64_t start, int64_t end) {
    auto [src_start, src_end] = _source->getContainedRange(start, end);

    _indices.clear();
    _indices.reserve(src_end - src_start);

    for (size_t i = src_start; i < src_end; ++i) {
        _indices.push_back(i);
    }

    _rebuildLocalIndices();
}

void ViewDigitalIntervalStorage::filterByEntityIds(std::unordered_set<EntityId> const & ids) {
    _indices.clear();

    for (size_t i = 0; i < _source->size(); ++i) {
        if (ids.contains(_source->getEntityId(i))) {
            _indices.push_back(i);
        }
    }

    _rebuildLocalIndices();
}

std::shared_ptr<OwningDigitalIntervalStorage const> ViewDigitalIntervalStorage::source() const {
    return _source;
}

Interval const & ViewDigitalIntervalStorage::getIntervalImpl(size_t idx) const {
    return _source->getInterval(_indices[idx]);
}

EntityId ViewDigitalIntervalStorage::getEntityIdImpl(size_t idx) const {
    return _source->getEntityId(_indices[idx]);
}

std::optional<size_t> ViewDigitalIntervalStorage::findByIntervalImpl(Interval const & interval) const {
    // Linear search through view indices
    for (size_t i = 0; i < _indices.size(); ++i) {
        Interval const & src_interval = _source->getInterval(_indices[i]);
        if (src_interval.start == interval.start && src_interval.end == interval.end) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<size_t> ViewDigitalIntervalStorage::findByEntityIdImpl(EntityId id) const {
    auto it = _local_entity_id_to_index.find(id);
    return it != _local_entity_id_to_index.end() ? std::optional{it->second} : std::nullopt;
}

bool ViewDigitalIntervalStorage::hasIntervalAtTimeImpl(int64_t time) const {
    for (size_t idx: _indices) {
        Interval const & interval = _source->getInterval(idx);
        if (interval.start <= time && time <= interval.end) {
            return true;
        }
    }
    return false;
}

std::pair<size_t, size_t> ViewDigitalIntervalStorage::getOverlappingRangeImpl(int64_t start, int64_t end) const {
    if (_indices.empty() || start > end) {
        return {0, 0};
    }

    // Views maintain sorted order from source, so we can use binary search.
    // Key insight: Since source intervals are disjoint and sorted by start,
    // and we preserve that order in our indices, we can binary search by
    // looking up the interval at each index.

    // Find first index where source interval ends at or after start
    auto it_start = std::ranges::lower_bound(_indices, start, {},
                                             [this](size_t idx) { return _source->getInterval(idx).end; });

    // Find first index where source interval starts strictly after end
    auto it_end = std::ranges::upper_bound(_indices, end, {},
                                           [this](size_t idx) { return _source->getInterval(idx).start; });

    size_t start_idx = static_cast<size_t>(std::distance(_indices.begin(), it_start));
    size_t end_idx = static_cast<size_t>(std::distance(_indices.begin(), it_end));

    return {start_idx, end_idx};
}

std::pair<size_t, size_t> ViewDigitalIntervalStorage::getContainedRangeImpl(int64_t start, int64_t end) const {
    if (_indices.empty() || start > end) {
        return {0, 0};
    }

    // Linear scan for view
    size_t start_idx = _indices.size();
    size_t end_idx = 0;

    for (size_t i = 0; i < _indices.size(); ++i) {
        Interval const & interval = _source->getInterval(_indices[i]);
        if (interval.start >= start && interval.end <= end) {
            start_idx = std::min(start_idx, i);
            end_idx = std::max(end_idx, i + 1);
        }
    }

    return start_idx <= end_idx ? std::pair{start_idx, end_idx} : std::pair<size_t, size_t>{0, 0};
}


DigitalIntervalStorageCache ViewDigitalIntervalStorage::tryGetCacheImpl() const {
    if (_indices.empty()) {
        return DigitalIntervalStorageCache{nullptr, nullptr, 0, true};
    }

    // Check if indices form a contiguous range
    size_t const start_idx = _indices[0];
    bool is_contiguous = true;

    for (size_t i = 1; i < _indices.size(); ++i) {
        if (_indices[i] != start_idx + i) {
            is_contiguous = false;
            break;
        }
    }

    if (is_contiguous) {
        return DigitalIntervalStorageCache{
                _source->intervals().data() + start_idx,
                _source->entityIds().data() + start_idx,
                _indices.size(),
                true};
    }

    return DigitalIntervalStorageCache{};// Invalid
}

void ViewDigitalIntervalStorage::_rebuildLocalIndices() {
    _local_entity_id_to_index.clear();
    for (size_t i = 0; i < _indices.size(); ++i) {
        EntityId const id = _source->getEntityId(_indices[i]);
        _local_entity_id_to_index[id] = i;
    }
}