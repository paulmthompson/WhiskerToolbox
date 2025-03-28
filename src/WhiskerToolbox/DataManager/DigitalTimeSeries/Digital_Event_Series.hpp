#ifndef BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
#define BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP

#include "Observer/Observer_Data.hpp"

#include <algorithm>
#include <ranges>
#include <string>
#include <vector>

/**
 * @brief Digital Event Series class
 *
 * A digital event series is a series of events where each event is defined by a time.
 * (Compare to DigitalIntervalSeries which is a series of intervals with start and end times)
 *
 * Use digital events where you wish to specify a time for each event.
 *
 *
 */
class DigitalEventSeries : public ObserverData {
public:
    DigitalEventSeries() = default;
    DigitalEventSeries(std::vector<float> event_vector);

    void setData(std::vector<float> event_vector);
    std::vector<float> const & getEventSeries() const;

    // Add a single event to the series
    void addEvent(float const event_time);

    // Remove an event at a specific time (exact match)
    bool removeEvent(float const event_time);

    // Get the number of events in the series
    size_t size() const { return _data.size(); }

    // Clear all events
    void clear() {
        _data.clear();
        notifyObservers();
    }

    // Range-based access to events within a time range (C++20)
    auto getEventsInRange(float start_time, float stop_time) const {
        return _data | std::views::filter([start_time, stop_time](float time) {
                   return time >= start_time && time <= stop_time;
               });
    }

    // Get vector of events in range (for backward compatibility)
    std::vector<float> getEventsAsVector(float start_time, float stop_time) const {
        auto range = getEventsInRange(start_time, stop_time);
        return {std::ranges::begin(range), std::ranges::end(range)};
    }

private:
    std::vector<float> _data{};

    // Sort the events in ascending order
    void _sortEvents();
};

namespace DigitalEventSeriesLoader {
DigitalEventSeries loadFromCSV(std::string const & filename);
}// namespace DigitalEventSeriesLoader


#endif//BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
