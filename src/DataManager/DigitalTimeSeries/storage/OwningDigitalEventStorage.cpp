/**
 * @file OwningDigitalEventStorage.cpp
 * @brief Implementation of owning digital event storage backend.
 */

#include "OwningDigitalEventStorage.hpp"

#include <algorithm>
#include <numeric>

OwningDigitalEventStorage::OwningDigitalEventStorage(std::vector<TimeFrameIndex> events)
    : _events(std::move(events)) {
    _sortEvents();
    _entity_ids.resize(_events.size(), EntityId{0});
}

OwningDigitalEventStorage::OwningDigitalEventStorage(std::vector<TimeFrameIndex> events,
                                                     std::vector<EntityId> entity_ids)
    : _events(std::move(events)),
      _entity_ids(std::move(entity_ids)) {
    if (_events.size() != _entity_ids.size()) {
        throw std::invalid_argument("Events and entity_ids must have same size");
    }
    _sortEventsWithEntityIds();
    _rebuildEntityIdIndex();
}

bool OwningDigitalEventStorage::addEvent(TimeFrameIndex time, EntityId entity_id) {
    auto it = std::ranges::lower_bound(_events, time);
    if (it != _events.end() && *it == time) {
        return false;
    }

    auto const idx = static_cast<size_t>(std::distance(_events.begin(), it));
    _events.insert(it, time);
    _entity_ids.insert(_entity_ids.begin() + static_cast<std::ptrdiff_t>(idx), entity_id);

    _entity_id_to_index[entity_id] = idx;
    for (size_t i = idx + 1; i < _entity_ids.size(); ++i) {
        _entity_id_to_index[_entity_ids[i]] = i;
    }

    return true;
}

bool OwningDigitalEventStorage::removeEvent(TimeFrameIndex time) {
    auto it = std::ranges::lower_bound(_events, time);
    if (it == _events.end() || *it != time) {
        return false;
    }

    auto const idx = static_cast<size_t>(std::distance(_events.begin(), it));

    if (idx < _entity_ids.size()) {
        _entity_id_to_index.erase(_entity_ids[idx]);
    }

    _events.erase(it);
    _entity_ids.erase(_entity_ids.begin() + static_cast<std::ptrdiff_t>(idx));

    for (size_t i = idx; i < _entity_ids.size(); ++i) {
        _entity_id_to_index[_entity_ids[i]] = i;
    }

    return true;
}

bool OwningDigitalEventStorage::removeByEntityId(EntityId id) {
    auto it = _entity_id_to_index.find(id);
    if (it == _entity_id_to_index.end()) {
        return false;
    }

    size_t const idx = it->second;
    _entity_id_to_index.erase(it);

    _events.erase(_events.begin() + static_cast<std::ptrdiff_t>(idx));
    _entity_ids.erase(_entity_ids.begin() + static_cast<std::ptrdiff_t>(idx));

    for (size_t i = idx; i < _entity_ids.size(); ++i) {
        _entity_id_to_index[_entity_ids[i]] = i;
    }

    return true;
}

void OwningDigitalEventStorage::clear() {
    _events.clear();
    _entity_ids.clear();
    _entity_id_to_index.clear();
}

void OwningDigitalEventStorage::reserve(size_t capacity) {
    _events.reserve(capacity);
    _entity_ids.reserve(capacity);
}

void OwningDigitalEventStorage::setEntityIds(std::vector<EntityId> ids) {
    if (ids.size() != _events.size()) {
        throw std::invalid_argument("EntityId count must match event count");
    }
    _entity_ids = std::move(ids);
    _rebuildEntityIdIndex();
}

std::optional<size_t> OwningDigitalEventStorage::findByTimeImpl(TimeFrameIndex time) const {
    auto it = std::ranges::lower_bound(_events, time);
    if (it != _events.end() && *it == time) {
        return static_cast<size_t>(std::distance(_events.begin(), it));
    }
    return std::nullopt;
}

std::optional<size_t> OwningDigitalEventStorage::findByEntityIdImpl(EntityId id) const {
    auto it = _entity_id_to_index.find(id);
    return it != _entity_id_to_index.end() ? std::optional{it->second} : std::nullopt;
}

std::pair<size_t, size_t> OwningDigitalEventStorage::getTimeRangeImpl(TimeFrameIndex start,
                                                                      TimeFrameIndex end) const {
    auto it_start = std::ranges::lower_bound(_events, start);
    auto it_end = std::ranges::upper_bound(_events, end);

    return {static_cast<size_t>(std::distance(_events.begin(), it_start)),
            static_cast<size_t>(std::distance(_events.begin(), it_end))};
}

void OwningDigitalEventStorage::_sortEvents() {
    std::ranges::sort(_events);
}

void OwningDigitalEventStorage::_sortEventsWithEntityIds() {
    std::vector<size_t> indices(_events.size());
    std::iota(indices.begin(), indices.end(), 0);

    std::ranges::sort(indices, [this](size_t a, size_t b) { return _events[a] < _events[b]; });

    std::vector<TimeFrameIndex> sorted_events;
    sorted_events.reserve(_events.size());
    std::vector<EntityId> sorted_ids;
    sorted_ids.reserve(_entity_ids.size());

    for (size_t idx: indices) {
        sorted_events.push_back(_events[idx]);
        sorted_ids.push_back(_entity_ids[idx]);
    }

    _events = std::move(sorted_events);
    _entity_ids = std::move(sorted_ids);
}
