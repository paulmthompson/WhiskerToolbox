#ifndef DIGITAL_INTERVAL_SERIES_HPP
#define DIGITAL_INTERVAL_SERIES_HPP

#include "Observer/Observer_Data.hpp"
#include "interval_data.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

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

    // Defines how to handle intervals that overlap with range boundaries
    enum class RangeMode {
        CONTAINED,  // Only intervals fully contained within range
        OVERLAPPING,// Any interval that overlaps with range
        CLIP        // Clip intervals at range boundaries
    };

    void setData(std::vector<Interval> digital_vector) {
        _data = digital_vector;
        _sortData();
        notifyObservers();
    };

    void setData(std::vector<std::pair<float, float>> digital_vector) {
        std::vector<Interval> intervals;
        for (auto const & interval: digital_vector) {
            intervals.push_back(Interval{static_cast<int64_t>(interval.first), static_cast<int64_t>(interval.second)});
        }
        setData(intervals);
    }

    void addEvent(Interval new_interval);

    template<typename T>
    void addEvent(T start, T end) {

        if (start > end) {
            std::cout << "Start time is greater than end time" << std::endl;
            return;
        }

        addEvent(Interval{static_cast<int64_t>(start), static_cast<int64_t>(end)});
    }
    std::vector<Interval> const & getDigitalIntervalSeries() const;

    bool isEventAtTime(int const time) const;
    void setEventAtTime(int time, bool const event);
    void removeEventAtTime(int time);

    template<typename T, typename B>
    void setEventsAtTimes(std::vector<T> times, std::vector<B> events) {
        for (int i = 0; i < times.size(); ++i) {
            _setEventAtTime(times[i], events[i]);
        }
        notifyObservers();
    }

    template<typename T>
    void createIntervalsFromBool(std::vector<T> const & bool_vector) {
        bool in_interval = false;
        int start = 0;
        for (int i = 0; i < bool_vector.size(); ++i) {
            if (bool_vector[i] && !in_interval) {
                start = i;
                in_interval = true;
            } else if (!bool_vector[i] && in_interval) {
                _data.push_back(Interval{start, i - 1});
                in_interval = false;
            }
        }
        if (in_interval) {
            _data.push_back(Interval{start, static_cast<int64_t>(bool_vector.size() - 1)});
        }

        _sortData();
        notifyObservers();
    }

    size_t size() const { return _data.size(); };

    template<RangeMode mode = RangeMode::CONTAINED>
    auto getIntervalsInRange(int64_t start_time, int64_t stop_time) const {

        if constexpr (mode == RangeMode::CONTAINED) {
            return _data | std::views::filter([start_time, stop_time](Interval const & interval) {
                       return interval.start >= start_time && interval.end <= stop_time;
                   });
        } else if constexpr (mode == RangeMode::OVERLAPPING) {
            return _data | std::views::filter([start_time, stop_time](Interval const & interval) {
                       return interval.start <= stop_time && interval.end >= start_time;
                   });
        } else if constexpr (mode == RangeMode::CLIP) {
            // For CLIP mode, we return a vector since we need to modify intervals
            return _getIntervalsAsVectorClipped(start_time, stop_time);
        } else {

            static_assert(true, "Unhandled IntervalQueryMode");
            // Return an empty view or handle error appropriately
            return std::views::empty<Interval>;
        }
    }

    // Get vector of intervals in range (for backward compatibility)
    template<RangeMode mode = RangeMode::CONTAINED>
    std::vector<Interval> getIntervalsAsVector(int64_t start_time, int64_t stop_time) const {
        if constexpr (mode == RangeMode::CLIP) {
            return _getIntervalsAsVectorClipped(start_time, stop_time);
        }

        auto range = getIntervalsInRange<mode>(start_time, stop_time);
        return {std::ranges::begin(range), std::ranges::end(range)};
    }

private:
    std::vector<Interval> _data{};

    void _addEvent(Interval new_interval);
    void _setEventAtTime(int time, bool const event);
    void _removeEventAtTime(int const time);

    void _sortData();

    // Helper method to handle clipping intervals at range boundaries
    std::vector<Interval> _getIntervalsAsVectorClipped(int64_t start_time, int64_t stop_time) const {
        std::vector<Interval> result;

        for (auto const & interval: _data) {
            // Skip if not overlapping
            if (interval.end < start_time || interval.start > stop_time)
                continue;

            // Create a new clipped interval
            Interval clipped{
                    std::max(interval.start, start_time),
                    std::min(interval.end, stop_time)};

            result.push_back(clipped);
        }

        return result;
    }
};

std::vector<Interval> load_digital_series_from_csv(std::string const & filename, char delimiter = ' ');

void save_intervals(std::vector<Interval> const & intervals,
                    std::string const block_output);

int find_closest_preceding_event(DigitalIntervalSeries * digital_series, int time);

#endif// DIGITAL_INTERVAL_SERIES_HPP
