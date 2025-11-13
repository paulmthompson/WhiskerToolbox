#include "Digital_Event_Series.hpp"

#include "Entity/EntityRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>// std::sort

DigitalEventSeries::DigitalEventSeries(std::vector<float> event_vector) {
    _data = std::move(event_vector);
    _sortEvents();
}

std::vector<float> const & DigitalEventSeries::getEventSeries() const {
    return _data;
}

void DigitalEventSeries::addEvent(float const event_time) {

    if (std::ranges::find(_data, event_time) != _data.end()) {
        return;
    }

    _data.push_back(event_time);

    _sortEvents();

    notifyObservers();
    if (_identity_registry) {
        // Rebuild to ensure order alignment
        rebuildAllEntityIds();
    }
}

bool DigitalEventSeries::removeEvent(float const event_time) {
    auto it = std::ranges::find(_data, event_time);
    if (it != _data.end()) {
        _data.erase(it);
        notifyObservers();
        if (_identity_registry) {
            rebuildAllEntityIds();
        }
        return true;
    }
    return false;
}

void DigitalEventSeries::_sortEvents() {
    std::ranges::sort(_data);
}

void DigitalEventSeries::rebuildAllEntityIds() {
    if (!_identity_registry) {
        _entity_ids.assign(_data.size(), EntityId(0));
        return;
    }
    _entity_ids.clear();
    _entity_ids.reserve(_data.size());
    for (int i = 0; i < static_cast<int>(_data.size()); ++i) {
        // Use time index = i (since data is float times, but consistent order gives stable local index)
        _entity_ids.push_back(_identity_registry->ensureId(_identity_data_key, EntityKind::EventEntity, TimeFrameIndex{i}, i));
    }
}

// ========== Events with EntityIDs ==========

std::vector<EventWithId> DigitalEventSeries::getEventsWithIdsInRange(TimeFrameIndex start_time, TimeFrameIndex stop_time) const {
    std::vector<EventWithId> result;
    //result.reserve(_data.size());// Reserve space for potential worst case

    for (size_t i = 0; i < _data.size(); ++i) {
        if (_data[i] >= static_cast<float>(start_time.getValue()) && _data[i] <= static_cast<float>(stop_time.getValue())) {
            EntityId const entity_id = (i < _entity_ids.size()) ? _entity_ids[i] : EntityId(0);
            result.emplace_back(_data[i], entity_id);
        }
    }
    return result;
}

std::vector<EventWithId> DigitalEventSeries::getEventsWithIdsInRange(TimeFrameIndex start_index,
                                                                     TimeFrameIndex stop_index,
                                                                     TimeFrame const & source_time_frame) const {
    if (&source_time_frame == _time_frame.get()) {
        return getEventsWithIdsInRange(start_index, stop_index);
    }

    // If either timeframe is null, fall back to original behavior
    if (!_time_frame.get()) {
        return getEventsWithIdsInRange(start_index, stop_index);
    }

    auto [target_start_index, target_stop_index] = convertTimeFrameRange(start_index, stop_index, source_time_frame, *_time_frame);
    return getEventsWithIdsInRange(target_start_index, target_stop_index);
}
