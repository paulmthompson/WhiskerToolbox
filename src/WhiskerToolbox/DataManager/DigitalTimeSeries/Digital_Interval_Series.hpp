#ifndef DIGITAL_INTERVAL_SERIES_HPP
#define DIGITAL_INTERVAL_SERIES_HPP

#include "Observer/Observer_Data.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <utility>

/**
 * @brief Digital IntervalSeries class
 *
 * A digital interval series is a series of intervals where each interval is defined by a start and end time.
 * (Compare to DigitalEventSeries which is a series of events at specific times)
 *
 * Use digital events where you wish to specify a beginning and end time for each event.
 *
 *
 */
class DigitalIntervalSeries : public ObserverData {
public:
    DigitalIntervalSeries() = default;
    DigitalIntervalSeries(std::vector<std::pair<float, float>> digital_vector);

    void setData(std::vector<std::pair<float, float>> digital_vector)
    {
        _data = digital_vector;
        _sortData();
        notifyObservers();
    };

    void addEvent(float start, float end)
    {

        if (start > end) {
            std::cout << "Start time is greater than end time" << std::endl;
            return;
        }

        auto start_overlap = _findOverlapWithStart(start, end);
        auto end_overlap = _findOverlapWithEnd(start, end);

        if (start_overlap != _data.end() && end_overlap != _data.end()) {
            start = std::min(start, start_overlap->first);
            end = std::max(end, end_overlap->second);
            _data.erase(start_overlap, end_overlap + 1);
        } else if (start_overlap != _data.end()) {
            start = std::min(start, start_overlap->first);
            _data.erase(start_overlap);
        } else if (end_overlap != _data.end()) {
            end = std::max(end, end_overlap->second);
            _data.erase(end_overlap);
        }

        _data.emplace_back(start, end);
        _sortData();
        notifyObservers();
    }
    std::vector<std::pair<float, float>> const& getDigitalIntervalSeries() const;

    bool isEventAtTime(int time) const;
    void setEventAtTime(int time, bool event);

    void createIntervalsFromBool(std::vector<uint8_t> const& bool_vector);

    size_t size() {return _data.size();};

private:
    std::vector<std::pair<float, float>> _data {};

    void _sortData();

    auto _findOverlapWithStart(float start, float end) -> decltype(_data.begin()) {
        for (auto it = _data.begin(); it != _data.end(); ++it) {
            if (end >= it->first - 1 && start <= it->first) {
                return it;
            }
        }
        return _data.end();
    }

    auto _findOverlapWithEnd(float start, float end) -> decltype(_data.begin()) {
        for (auto it = _data.begin(); it != _data.end(); ++it) {
            if (start <= it->second + 1 && end >= it->second) {
                return it;
            }
        }
        return _data.end();
    }

};

std::vector<std::pair<float, float>> load_digital_series_from_csv(std::string const& filename);

int find_closest_preceding_event(DigitalIntervalSeries * digital_series, int time);

#endif // DIGITAL_INTERVAL_SERIES_HPP
