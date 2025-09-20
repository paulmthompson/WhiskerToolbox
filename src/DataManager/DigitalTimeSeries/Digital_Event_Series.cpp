#include "Digital_Event_Series.hpp"

#include "Entity/EntityRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <algorithm>// std::sort

DigitalEventSeries::DigitalEventSeries(std::vector<float> event_vector) {
    setData(std::move(event_vector));
}

void DigitalEventSeries::setData(std::vector<float> event_vector) {
    _data = std::move(event_vector);
    _sortEvents();
    notifyObservers();
    // Rebuild IDs if identity context present
    if (_identity_registry) {
        rebuildAllEntityIds();
    }
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
        _entity_ids.assign(_data.size(), 0);
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

std::vector<EventWithId> DigitalEventSeries::getEventsWithIdsInRange(float start_time, float stop_time) const {
    std::vector<EventWithId> result;
    //result.reserve(_data.size());// Reserve space for potential worst case

    for (size_t i = 0; i < _data.size(); ++i) {
        if (_data[i] >= start_time && _data[i] <= stop_time) {
            EntityId const entity_id = (i < _entity_ids.size()) ? _entity_ids[i] : 0;
            result.emplace_back(_data[i], entity_id);
        }
    }
    return result;
}

std::vector<EventWithId> DigitalEventSeries::getEventsWithIdsInRange(TimeFrameIndex start_time, TimeFrameIndex stop_time) const {
    auto [start_time_value, stop_time_value] = _getTimeRangeFromIndices(start_time, stop_time);
    return getEventsWithIdsInRange(start_time_value, stop_time_value);
}

std::vector<EventWithId> DigitalEventSeries::getEventsWithIdsInRange(TimeFrameIndex start_index,
                                                                     TimeFrameIndex stop_index,
                                                                     TimeFrame const * source_time_frame,
                                                                     TimeFrame const * event_time_frame) const {
    if (source_time_frame == event_time_frame) {
        return getEventsWithIdsInRange(start_index, stop_index);
    }

    // If either timeframe is null, fall back to original behavior
    if (!source_time_frame || !event_time_frame) {
        return getEventsWithIdsInRange(start_index, stop_index);
    }

    auto [target_start_index, target_stop_index] = _convertTimeFrameRange(start_index, stop_index, source_time_frame, event_time_frame);
    return getEventsWithIdsInRange(target_start_index, target_stop_index);
}

// ========== Helper Functions for Time Frame Conversion ==========

std::pair<TimeFrameIndex, TimeFrameIndex> DigitalEventSeries::_convertTimeFrameRange(
        TimeFrameIndex const start_index,
        TimeFrameIndex const stop_index,
        TimeFrame const * const source_time_frame,
        TimeFrame const * const target_time_frame) {

    // Get the time values from the source timeframe
    auto start_time_value = source_time_frame->getTimeAtIndex(start_index);
    auto stop_time_value = source_time_frame->getTimeAtIndex(stop_index);

    // Convert to indices in the target timeframe
    auto target_start_index = target_time_frame->getIndexAtTime(static_cast<float>(start_time_value), false);
    auto target_stop_index = target_time_frame->getIndexAtTime(static_cast<float>(stop_time_value));

    return {target_start_index, target_stop_index};
}

std::pair<float, float> DigitalEventSeries::_getTimeRangeFromIndices(
        TimeFrameIndex start_index,
        TimeFrameIndex stop_index) const {

    if (_time_frame) {
        auto start_time_value = _time_frame->getTimeAtIndex(start_index);
        auto stop_time_value = _time_frame->getTimeAtIndex(stop_index);
        return {static_cast<float>(start_time_value), static_cast<float>(stop_time_value)};
    } else {
        // Fallback to using indices as time values if no timeframe
        return {static_cast<float>(start_index.getValue()), static_cast<float>(stop_index.getValue())};
    }
}
