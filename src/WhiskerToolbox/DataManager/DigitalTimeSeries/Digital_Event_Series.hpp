#ifndef BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
#define BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP

#include "Observer/Observer_Data.hpp"
#include "TimeFrame.hpp"

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

    [[nodiscard]] auto getEventsInRange(float start_time, float stop_time) const {
        return _data | std::views::filter([start_time, stop_time](float time) {
                   return time >= start_time && time <= stop_time;
               });
    }

    template<typename TransformFunc>
    auto getEventsInRange(float start_time, float stop_time, TransformFunc time_transform) const {
        return _data | std::views::filter([start_time, stop_time, time_transform](float time) {
                   auto transformed_time = time_transform(time);
                   return transformed_time >= start_time && transformed_time <= stop_time;
               });
    }

    [[nodiscard]] auto getEventsInRange(TimeFrameIndex start_index,
                                        TimeFrameIndex stop_index,
                                        TimeFrame const * source_time_frame,
                                        TimeFrame const * destination_time_frame) const {

        auto start_time_idx = getTimeIndexForSeries(start_index, source_time_frame, destination_time_frame);
        auto end_time_idx = getTimeIndexForSeries(stop_index, source_time_frame, destination_time_frame);
        return getEventsInRange(start_time_idx, end_time_idx);
    };

    std::vector<float> getEventsAsVector(float start_time, float stop_time) const {
        std::vector<float> result;
        for (float time : _data) {
            if (time >= start_time && time <= stop_time) {
                result.push_back(time);
            }
        }
        return result;
    }

    template<typename TransformFunc>
    std::vector<float> getEventsAsVector(float start_time, float stop_time, TransformFunc time_transform) const {
        std::vector<float> result;
        for (float time : _data) {
            auto transformed_time = time_transform(time);
            if (transformed_time >= start_time && transformed_time <= stop_time) {
                result.push_back(time);
            }
        }
        return result;
    }

private:
    std::vector<float> _data{};

    // Sort the events in ascending order
    void _sortEvents();
};

#endif//BEHAVIORTOOLBOX_DIGITAL_EVENT_SERIES_HPP
