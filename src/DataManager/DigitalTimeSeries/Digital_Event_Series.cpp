#include "Digital_Event_Series.hpp"

#include "TimeFrame/TimeFrame.hpp"
#include "Entity/EntityRegistry.hpp"

#include <algorithm> // std::sort

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

    if (std::find(_data.begin(), _data.end(), event_time) != _data.end()) {
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
    auto it = std::find(_data.begin(), _data.end(), event_time);
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
    std::sort(_data.begin(), _data.end());
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

