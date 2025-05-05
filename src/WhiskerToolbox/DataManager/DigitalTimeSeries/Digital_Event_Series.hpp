#ifndef BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
#define BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP

#include "Observer/Observer_Data.hpp"


#include <concepts>
#include <ranges>
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
    explicit DigitalEventSeries(std::vector<float> event_vector);

    void setData(std::vector<float> event_vector);
    [[nodiscard]] std::vector<float> const & getEventSeries() const;

    void addEvent(float event_time);

    bool removeEvent(float event_time);

    [[nodiscard]] size_t size() const { return _data.size(); }

    void clear() {
        _data.clear();
        notifyObservers();
    }

    template<typename TransformFunc = std::identity>
    auto getEventsInRange(float start_time, float stop_time,
                          TransformFunc && time_transform = {}) const {
        return _data | std::views::filter([start_time, stop_time, time_transform](float time) {
                   auto transformed_time = time_transform(time);
                   return transformed_time >= start_time && transformed_time <= stop_time;
               });
    }

    [[nodiscard]] auto getEventsInRange(float start_time, float stop_time) const {
        return getEventsInRange(start_time, stop_time, std::identity{});
    }

    template<typename TransformFunc = std::identity>
    std::vector<float> getEventsAsVector(float start_time, float stop_time,
                                         TransformFunc && time_transform = {}) const {
        auto range = getEventsInRange(start_time, stop_time, time_transform);
        return {std::ranges::begin(range), std::ranges::end(range)};
    }

private:
    std::vector<float> _data{};

    // Sort the events in ascending order
    void _sortEvents();
};

#endif//BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
