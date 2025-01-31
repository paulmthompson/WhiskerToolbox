#ifndef DIGITAL_INTERVAL_SERIES_HPP
#define DIGITAL_INTERVAL_SERIES_HPP

#include "Observer/Observer_Data.hpp"
#include "interval_data.hpp"

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
    DigitalIntervalSeries(std::vector<Interval> digital_vector);

    void setData(std::vector<Interval> digital_vector)
    {
        _data = digital_vector;
        _sortData();
        notifyObservers();
    };

    void setData(std::vector<std::pair<float,float>> digital_vector)
    {
        std::vector<Interval> intervals;
        for (auto const& interval : digital_vector) {
            intervals.push_back(Interval{static_cast<int64_t>(interval.first), static_cast<int64_t>(interval.second)});
        }
        setData(intervals);
    }

    void addEvent(Interval new_interval);

    template <typename T>
    void addEvent(T start, T end)
    {

        if (start > end) {
            std::cout << "Start time is greater than end time" << std::endl;
            return;
        }

        addEvent(Interval{static_cast<int64_t>(start), static_cast<int64_t>(end)});
    }
    std::vector<Interval> const& getDigitalIntervalSeries() const;

    bool isEventAtTime(int time) const;
    void setEventAtTime(int time, bool event);
    void removeEventAtTime(int time);

    template <typename T>
    void setEventsAtTimes(std::vector<T> times, std::vector<bool> events)
    {
        for (int i = 0; i < times.size(); ++i) {
            _setEventAtTime(times[i], events[i]);
        }
        notifyObservers();
    }

    void createIntervalsFromBool(std::vector<uint8_t> const& bool_vector);

    size_t size() {return _data.size();};

private:
    std::vector<Interval> _data {};

    void _addEvent(Interval new_interval);
    void _setEventAtTime(int time, bool event);
    void _removeEventAtTime(int time);

    void _sortData();

};

std::vector<Interval> load_digital_series_from_csv(std::string const& filename, char delimiter = ' ');

void save_intervals(std::vector<Interval> const & intervals,
                    std::string const block_output);

int find_closest_preceding_event(DigitalIntervalSeries * digital_series, int time);

#endif // DIGITAL_INTERVAL_SERIES_HPP
