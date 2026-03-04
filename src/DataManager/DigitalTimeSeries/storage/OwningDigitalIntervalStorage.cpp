
#include "OwningDigitalIntervalStorage.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>

OwningDigitalIntervalStorage::OwningDigitalIntervalStorage(std::vector<Interval> intervals) {
    _intervals = std::move(intervals);
    _sortIntervals();
    // Entity IDs will be empty until rebuilt
    _entity_ids.resize(_intervals.size(), EntityId{0});
}

OwningDigitalIntervalStorage::OwningDigitalIntervalStorage(std::vector<Interval> intervals, std::vector<EntityId> entity_ids) {
    if (intervals.size() != entity_ids.size()) {
        throw std::invalid_argument("Intervals and entity_ids must have same size");
    }
    _intervals = std::move(intervals);
    _entity_ids = std::move(entity_ids);
    _sortIntervalsWithEntityIds();
    _rebuildEntityIdIndex();
}

bool OwningDigitalIntervalStorage::addInterval(Interval const & interval, EntityId entity_id) {
    // Find insertion position by start time
    auto it = std::ranges::lower_bound(_intervals, interval.start, {},
                                       [](Interval const & i) { return i.start; });

    // Check for exact duplicate
    while (it != _intervals.end() && it->start == interval.start) {
        if (it->end == interval.end) {
            return false;// Duplicate
        }
        ++it;
    }

    // Insert at sorted position
    it = std::ranges::lower_bound(_intervals, interval.start, {},
                                  [](Interval const & i) { return i.start; });
    auto const idx = static_cast<size_t>(std::distance(_intervals.begin(), it));
    _intervals.insert(it, interval);
    _entity_ids.insert(_entity_ids.begin() + static_cast<std::ptrdiff_t>(idx), entity_id);

    // Update index
    _entity_id_to_index[entity_id] = idx;
    // Update indices for moved elements
    for (size_t i = idx + 1; i < _entity_ids.size(); ++i) {
        _entity_id_to_index[_entity_ids[i]] = i;
    }

    return true;
}

bool OwningDigitalIntervalStorage::removeInterval(Interval const & interval) {
    auto opt_idx = findByIntervalImpl(interval);
    if (!opt_idx) {
        return false;
    }

    size_t const idx = *opt_idx;

    // Remove from entity ID index
    if (idx < _entity_ids.size()) {
        _entity_id_to_index.erase(_entity_ids[idx]);
    }

    _intervals.erase(_intervals.begin() + static_cast<std::ptrdiff_t>(idx));
    _entity_ids.erase(_entity_ids.begin() + static_cast<std::ptrdiff_t>(idx));

    // Update indices for moved elements
    for (size_t i = idx; i < _entity_ids.size(); ++i) {
        _entity_id_to_index[_entity_ids[i]] = i;
    }

    return true;
}

bool OwningDigitalIntervalStorage::removeByEntityId(EntityId id) {
    auto it = _entity_id_to_index.find(id);
    if (it == _entity_id_to_index.end()) {
        return false;
    }

    size_t const idx = it->second;
    _entity_id_to_index.erase(it);

    _intervals.erase(_intervals.begin() + static_cast<std::ptrdiff_t>(idx));
    _entity_ids.erase(_entity_ids.begin() + static_cast<std::ptrdiff_t>(idx));

    // Update indices for moved elements
    for (size_t i = idx; i < _entity_ids.size(); ++i) {
        _entity_id_to_index[_entity_ids[i]] = i;
    }

    return true;
}

void OwningDigitalIntervalStorage::clear() {
    _intervals.clear();
    _entity_ids.clear();
    _entity_id_to_index.clear();
}

/**
     * @brief Reserve capacity for expected number of intervals
     */
void OwningDigitalIntervalStorage::reserve(size_t capacity) {
    _intervals.reserve(capacity);
    _entity_ids.reserve(capacity);
}

/**
     * @brief Set all entity IDs (must match interval count)
     */
void OwningDigitalIntervalStorage::setEntityIds(std::vector<EntityId> ids) {
    if (ids.size() != _intervals.size()) {
        throw std::invalid_argument("EntityId count must match interval count");
    }
    _entity_ids = std::move(ids);
    _rebuildEntityIdIndex();
}

void OwningDigitalIntervalStorage::setEntityId(size_t idx, EntityId id) {
    if (idx >= _entity_ids.size()) {
        throw std::out_of_range("Index out of range");
    }
    // Remove old mapping
    _entity_id_to_index.erase(_entity_ids[idx]);
    _entity_ids[idx] = id;
    _entity_id_to_index[id] = idx;
}

/**
     * @brief Set interval at a specific index (does not re-sort)
     */
void OwningDigitalIntervalStorage::setInterval(size_t idx, Interval interval) {
    if (idx >= _intervals.size()) {
        throw std::out_of_range("Index out of range");
    }
    _intervals[idx] = interval;
}

/**
     * @brief Remove interval at a specific index
     */
void OwningDigitalIntervalStorage::removeAt(size_t idx) {
    if (idx >= _intervals.size()) {
        throw std::out_of_range("Index out of range");
    }

    // Remove from entity ID index
    if (idx < _entity_ids.size()) {
        _entity_id_to_index.erase(_entity_ids[idx]);
    }

    _intervals.erase(_intervals.begin() + static_cast<std::ptrdiff_t>(idx));
    _entity_ids.erase(_entity_ids.begin() + static_cast<std::ptrdiff_t>(idx));

    // Update indices for moved elements
    for (size_t i = idx; i < _entity_ids.size(); ++i) {
        _entity_id_to_index[_entity_ids[i]] = i;
    }
}

/**
     * @brief Sort intervals and entity IDs together
     */
void OwningDigitalIntervalStorage::sort() {
    _sortIntervalsWithEntityIds();
    _rebuildEntityIdIndex();
}

std::optional<size_t> OwningDigitalIntervalStorage::findByIntervalImpl(Interval const & interval) const {
    // Binary search by start time, then linear search for exact match
    auto it = std::ranges::lower_bound(_intervals, interval.start, {},
                                       [](Interval const & i) { return i.start; });

    while (it != _intervals.end() && it->start == interval.start) {
        if (it->end == interval.end) {
            return static_cast<size_t>(std::distance(_intervals.begin(), it));
        }
        ++it;
    }
    return std::nullopt;
}

std::optional<size_t> OwningDigitalIntervalStorage::findByEntityIdImpl(EntityId id) const {
    auto it = _entity_id_to_index.find(id);
    return it != _entity_id_to_index.end() ? std::optional{it->second} : std::nullopt;
}

bool OwningDigitalIntervalStorage::hasIntervalAtTimeImpl(int64_t time) const {
    // Binary search to find intervals that might contain this time
    // An interval contains time if interval.start <= time <= interval.end

    // Find first interval where start > time (all previous could contain time)
    auto it = std::ranges::upper_bound(_intervals, time, {},
                                       [](Interval const & i) { return i.start; });

    // Check intervals from beginning up to it
    for (auto check = _intervals.begin(); check != it; ++check) {
        if (check->end >= time) {
            return true;
        }
    }
    return false;
}

std::pair<size_t, size_t> OwningDigitalIntervalStorage::getOverlappingRangeImpl(int64_t start, int64_t end) const {
    if (_intervals.empty() || start > end) {
        return {0, 0};
    }

    // An interval overlaps [start, end] if interval.start <= end && interval.end >= start
    //
    // Key insight: Since intervals are disjoint (merged on insertion) and sorted by start,
    // they are ALSO sorted by end. This enables O(log n) binary search for both bounds.

    // 1. Find the STOP index (Upper Bound on Start Time)
    // We want the first interval that starts strictly AFTER our range ends.
    // Everything before this is a candidate.
    auto it_end = std::ranges::upper_bound(_intervals, end, {},
                                           [](Interval const & i) { return i.start; });

    // 2. Find the START index (Lower Bound on End Time)
    // We want the first interval that ends at or after our range starts.
    // Since disjoint intervals sorted by start are also sorted by end,
    // we can binary search this!
    auto it_start = std::ranges::lower_bound(_intervals.begin(), it_end, start, {},
                                             [](Interval const & i) { return i.end; });

    size_t start_idx = static_cast<size_t>(std::distance(_intervals.begin(), it_start));
    size_t end_idx = static_cast<size_t>(std::distance(_intervals.begin(), it_end));

    return {start_idx, end_idx};
}

std::pair<size_t, size_t> OwningDigitalIntervalStorage::getContainedRangeImpl(int64_t start, int64_t end) const {
    if (_intervals.empty() || start > end) {
        return {0, 0};
    }

    // An interval is contained in [start, end] if interval.start >= start && interval.end <= end
    // Find first interval where start >= start
    auto it_start = std::ranges::lower_bound(_intervals, start, {},
                                             [](Interval const & i) { return i.start; });

    if (it_start == _intervals.end()) {
        return {0, 0};
    }

    size_t start_idx = static_cast<size_t>(std::distance(_intervals.begin(), it_start));
    size_t end_idx = start_idx;

    // Find last interval where end <= end
    for (size_t i = start_idx; i < _intervals.size() && _intervals[i].start <= end; ++i) {
        if (_intervals[i].end <= end) {
            end_idx = i + 1;
        }
    }

    return {start_idx, end_idx};
}

void OwningDigitalIntervalStorage::_sortIntervals() {
    std::ranges::sort(_intervals, [](Interval const & a, Interval const & b) {
        return a.start < b.start;
    });
}

void OwningDigitalIntervalStorage::_sortIntervalsWithEntityIds() {
    // Create index array and sort by interval start time
    std::vector<size_t> indices(_intervals.size());
    std::iota(indices.begin(), indices.end(), 0);

    std::ranges::sort(indices, [this](size_t a, size_t b) {
        return _intervals[a].start < _intervals[b].start;
    });

    // Apply permutation to both arrays
    std::vector<Interval> sorted_intervals;
    sorted_intervals.reserve(_intervals.size());
    std::vector<EntityId> sorted_ids;
    sorted_ids.reserve(_entity_ids.size());

    for (size_t i = 0; i < indices.size(); ++i) {
        sorted_intervals.push_back(_intervals[indices[i]]);
        sorted_ids.push_back(_entity_ids[indices[i]]);
    }

    _intervals = std::move(sorted_intervals);
    _entity_ids = std::move(sorted_ids);
}