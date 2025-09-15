#include "Digital_Interval_Series.hpp"
#include "Entity/EntityRegistry.hpp"

#include <algorithm>
#include <utility>  
#include <vector>

// ========== Constructors ==========

DigitalIntervalSeries::DigitalIntervalSeries(std::vector<Interval> digital_vector) {
    _data = std::move(digital_vector);
    _sortData();
}

DigitalIntervalSeries::DigitalIntervalSeries(std::vector<std::pair<float, float>> const & digital_vector) {
    std::vector<Interval> intervals;
    intervals.reserve(digital_vector.size());
    for (auto & interval: digital_vector) {
        intervals.emplace_back(Interval{static_cast<int64_t>(interval.first), static_cast<int64_t>(interval.second)});
    }
    _data = std::move(intervals);
    _sortData();
}

// ========== Getters ==========

std::vector<Interval> const & DigitalIntervalSeries::getDigitalIntervalSeries() const {
    return _data;
}

bool DigitalIntervalSeries::isEventAtTime(TimeFrameIndex const time) const {

    auto Contained = [time](auto const & event) {
        return is_contained(event, time.getValue());
    };

    if (std::ranges::any_of(_data, Contained)) return true;

    return false;
}

void DigitalIntervalSeries::addEvent(Interval new_interval) {
    _addEvent(new_interval);

    notifyObservers();
}

void DigitalIntervalSeries::_addEvent(Interval new_interval) {
    auto it = _data.begin();
    while (it != _data.end()) {
        if (is_overlapping(*it, new_interval) || is_contiguous(*it, new_interval)) {
            new_interval.start = std::min(new_interval.start, it->start);
            new_interval.end = std::max(new_interval.end, it->end);
            it = _data.erase(it);
        } else if (is_contained(new_interval, *it)) {
            // The new interval is completely contained within an existing interval, so we do nothing.
            return;
        } else {
            ++it;
        }
    }
    _data.push_back(new_interval);
    _sortData();
}

void DigitalIntervalSeries::setEventAtTime(TimeFrameIndex time, bool const event) {
    _setEventAtTime(time, event);
    notifyObservers();
}

bool DigitalIntervalSeries::removeInterval(Interval const & interval) {
    auto it = std::find(_data.begin(), _data.end(), interval);
    if (it != _data.end()) {
        _data.erase(it);
        notifyObservers();
        return true;
    }
    return false;
}

size_t DigitalIntervalSeries::removeIntervals(std::vector<Interval> const & intervals) {
    size_t removed_count = 0;
    
    for (auto const & interval : intervals) {
        auto it = std::find(_data.begin(), _data.end(), interval);
        if (it != _data.end()) {
            _data.erase(it);
            removed_count++;
        }
    }
    
    if (removed_count > 0) {
        _sortData();  // Re-sort after removals
        notifyObservers();
    }
    
    return removed_count;
}

void DigitalIntervalSeries::_setEventAtTime(TimeFrameIndex time, bool const event) {
    if (!event) {
        _removeEventAtTime(time);
    } else {
        _addEvent(Interval{time.getValue(), time.getValue()});
    }
}

void DigitalIntervalSeries::_removeEventAtTime(TimeFrameIndex const time) {
    for (auto it = _data.begin(); it != _data.end(); ++it) {
        if (is_contained(*it, time.getValue())) {
            if (time.getValue() == it->start && time.getValue() == it->end) {
                _data.erase(it);
            } else if (time.getValue() == it->start) {
                it->start = time.getValue() + 1;
            } else if (time.getValue() == it->end) {
                it->end = time.getValue() - 1;
            } else {
                auto preceding_event = Interval{it->start, time.getValue() - 1};
                auto following_event = Interval{time.getValue() + 1, it->end};
                _data.erase(it);
                _data.push_back(preceding_event);
                _data.push_back(following_event);

                _sortData();
            }
            return;
        }
    }
}

void DigitalIntervalSeries::_sortData() {
    std::sort(_data.begin(), _data.end());
}

void DigitalIntervalSeries::rebuildAllEntityIds() {
    if (!_identity_registry) {
        _entity_ids.assign(_data.size(), 0);
        return;
    }
    _entity_ids.clear();
    _entity_ids.reserve(_data.size());
    for (size_t i = 0; i < _data.size(); ++i) {
        // Use start as the discrete time index representative, and i as stable local index
        _entity_ids.push_back(
            _identity_registry->ensureId(_identity_data_key, EntityKind::IntervalEntity, TimeFrameIndex{_data[i].start}, static_cast<int>(i))
        );
    }
}

// ========== Entity Lookup Methods ==========

std::optional<Interval> DigitalIntervalSeries::getIntervalByEntityId(EntityId entity_id) const {
    if (!_identity_registry) {
        return std::nullopt;
    }
    
    auto descriptor = _identity_registry->get(entity_id);
    if (!descriptor || descriptor->kind != EntityKind::IntervalEntity || descriptor->data_key != _identity_data_key) {
        return std::nullopt;
    }
    
    int local_index = descriptor->local_index;
    
    if (local_index < 0 || static_cast<size_t>(local_index) >= _data.size()) {
        return std::nullopt;
    }
    
    return _data[static_cast<size_t>(local_index)];
}

std::optional<int> DigitalIntervalSeries::getIndexByEntityId(EntityId entity_id) const {
    if (!_identity_registry) {
        return std::nullopt;
    }
    
    auto descriptor = _identity_registry->get(entity_id);
    if (!descriptor || descriptor->kind != EntityKind::IntervalEntity || descriptor->data_key != _identity_data_key) {
        return std::nullopt;
    }
    
    int local_index = descriptor->local_index;
    
    if (local_index < 0 || static_cast<size_t>(local_index) >= _data.size()) {
        return std::nullopt;
    }
    
    return local_index;
}

std::vector<std::pair<EntityId, Interval>> DigitalIntervalSeries::getIntervalsByEntityIds(std::vector<EntityId> const & entity_ids) const {
    std::vector<std::pair<EntityId, Interval>> result;
    result.reserve(entity_ids.size());
    
    for (EntityId entity_id : entity_ids) {
        auto interval = getIntervalByEntityId(entity_id);
        if (interval) {
            result.emplace_back(entity_id, *interval);
        }
    }
    
    return result;
}

std::vector<std::pair<EntityId, int>> DigitalIntervalSeries::getIndexInfoByEntityIds(std::vector<EntityId> const & entity_ids) const {
    std::vector<std::pair<EntityId, int>> result;
    result.reserve(entity_ids.size());
    
    for (EntityId entity_id : entity_ids) {
        auto index = getIndexByEntityId(entity_id);
        if (index) {
            result.emplace_back(entity_id, *index);
        }
    }
    
    return result;
}

int find_closest_preceding_event(DigitalIntervalSeries * digital_series, TimeFrameIndex time) {
    auto const & events = digital_series->getDigitalIntervalSeries();

    // Check if sorted
    for (size_t i = 1; i < events.size(); ++i) {
        if (events[i].start < events[i - 1].start) {
            throw std::runtime_error("DigitalIntervalSeries is not sorted");
        }
    }
    int closest_index = -1;
    for (size_t i = 0; i < events.size(); ++i) {
        if (events[i].start <= time.getValue()) {
            closest_index = static_cast<int>(i);
            if (time.getValue() <= events[i].end) {
                return static_cast<int>(i);
            }
        } else {
            break;
        }
    }
    return closest_index;
}

