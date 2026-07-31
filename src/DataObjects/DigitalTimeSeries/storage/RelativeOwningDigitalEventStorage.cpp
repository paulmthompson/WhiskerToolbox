/**
 * @file RelativeOwningDigitalEventStorage.cpp
 * @brief Implementation of relative owning digital event storage backend.
 */

#include "RelativeOwningDigitalEventStorage.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>

RelativeOwningDigitalEventStorage::RelativeOwningDigitalEventStorage(std::vector<ClockTicks> events)
    : _events(std::move(events)) {
    _sortEvents();
    _entity_ids.resize(_events.size(), EntityId{0});
}

RelativeOwningDigitalEventStorage::RelativeOwningDigitalEventStorage(std::vector<ClockTicks> events,
                                                                     std::vector<EntityId> entity_ids)
    : _events(std::move(events)),
      _entity_ids(std::move(entity_ids)) {
    if (_events.size() != _entity_ids.size()) {
        throw std::invalid_argument("Events and entity_ids must have same size");
    }
    _sortEventsWithEntityIds();
    _rebuildEntityIdIndex();
}

TimeFrameIndex RelativeOwningDigitalEventStorage::getEventImpl(size_t /*idx*/) {
    throw std::runtime_error(
            "RelativeOwningDigitalEventStorage does not store TimeFrameIndex events; use getRelativeEvent()");
}

std::optional<size_t> RelativeOwningDigitalEventStorage::findByTimeImpl(TimeFrameIndex /*time*/) {
    throw std::runtime_error(
            "RelativeOwningDigitalEventStorage does not support findByTime(); use findByRelativeTime()");
}

std::optional<size_t> RelativeOwningDigitalEventStorage::findByEntityIdImpl(EntityId id) const {
    auto it = _entity_id_to_index.find(id);
    return it != _entity_id_to_index.end() ? std::optional{it->second} : std::nullopt;
}

std::pair<size_t, size_t> RelativeOwningDigitalEventStorage::getTimeRangeImpl(TimeFrameIndex /*start*/,
                                                                              TimeFrameIndex /*end*/) {
    throw std::runtime_error(
            "RelativeOwningDigitalEventStorage does not support getTimeRange(); use getRelativeTimeRange()");
}

std::optional<size_t> RelativeOwningDigitalEventStorage::findByRelativeTimeImpl(ClockTicks time) const {
    auto it = std::ranges::lower_bound(_events, time);
    if (it != _events.end() && *it == time) {
        return static_cast<size_t>(std::distance(_events.begin(), it));
    }
    return std::nullopt;
}

std::pair<size_t, size_t> RelativeOwningDigitalEventStorage::getRelativeTimeRangeImpl(ClockTicks start,
                                                                                      ClockTicks end) const {
    auto it_start = std::ranges::lower_bound(_events, start);
    auto it_end = std::ranges::upper_bound(_events, end);

    return {static_cast<size_t>(std::distance(_events.begin(), it_start)),
            static_cast<size_t>(std::distance(_events.begin(), it_end))};
}

void RelativeOwningDigitalEventStorage::_sortEvents() {
    std::ranges::sort(_events);
    auto [first, last] = std::ranges::unique(_events);
    _events.erase(first, last);
}

void RelativeOwningDigitalEventStorage::_sortEventsWithEntityIds() {
    std::vector<size_t> indices(_events.size());
    std::iota(indices.begin(), indices.end(), 0);

    std::ranges::sort(indices, [this](size_t a, size_t b) { return _events[a] < _events[b]; });

    std::vector<ClockTicks> sorted_events;
    sorted_events.reserve(_events.size());
    std::vector<EntityId> sorted_ids;
    sorted_ids.reserve(_entity_ids.size());

    for (size_t const idx: indices) {
        if (!sorted_events.empty() && sorted_events.back() == _events[idx]) {
            continue;
        }
        sorted_events.push_back(_events[idx]);
        sorted_ids.push_back(_entity_ids[idx]);
    }

    _events = std::move(sorted_events);
    _entity_ids = std::move(sorted_ids);
}
