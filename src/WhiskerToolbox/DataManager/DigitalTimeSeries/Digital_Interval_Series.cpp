#include "Digital_Interval_Series.hpp"

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
    bool merged = false;

    for (auto it = _data.begin(); it != _data.end();) {
        if (is_overlapping(*it, new_interval) || is_contiguous(*it, new_interval)) {
            new_interval.start = std::min(new_interval.start, it->start);
            new_interval.end = std::max(new_interval.end, it->end);
            it = _data.erase(it);
            merged = true;
        } else if (is_contained(new_interval, *it)) {
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

int find_closest_preceding_event(DigitalIntervalSeries * digital_series, TimeFrameIndex time) {
    auto const & events = digital_series->getDigitalIntervalSeries();

    // Check if sorted
    for (int i = 1; i < events.size(); ++i) {
        if (events[i].start < events[i - 1].start) {
            throw std::runtime_error("DigitalIntervalSeries is not sorted");
        }
    }
    int closest_index = -1;
    for (int i = 0; i < events.size(); ++i) {
        if (events[i].start <= time.getValue()) {
            closest_index = i;
            if (time.getValue() <= events[i].end) {
                return i;
            }
        } else {
            break;
        }
    }
    return closest_index;
}

