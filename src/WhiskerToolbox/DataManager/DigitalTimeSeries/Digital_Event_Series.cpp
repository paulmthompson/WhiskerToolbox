#include "Digital_Event_Series.hpp"

#include <algorithm> // std::sort

DigitalEventSeries::DigitalEventSeries(std::vector<float> event_vector) {
    setData(std::move(event_vector));
}

void DigitalEventSeries::setData(std::vector<float> event_vector) {
    _data = std::move(event_vector);
    _sortEvents();
    notifyObservers();
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
}

bool DigitalEventSeries::removeEvent(float const event_time) {
    auto it = std::find(_data.begin(), _data.end(), event_time);
    if (it != _data.end()) {
        _data.erase(it);
        notifyObservers();
        return true;
    }
    return false;
}

void DigitalEventSeries::_sortEvents() {
    std::sort(_data.begin(), _data.end());
}
