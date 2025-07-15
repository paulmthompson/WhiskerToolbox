#ifndef DIGITAL_INTERVAL_SERIES_HPP
#define DIGITAL_INTERVAL_SERIES_HPP

#include "Observer/Observer_Data.hpp"
#include "interval_data.hpp"
#include "TimeFrame.hpp"

#include <cstdint>
#include <iostream>
#include <ranges>
#include <utility>
#include <vector>

template<typename T>
inline constexpr bool always_false_v = false;


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

    // ========== Constructors ==========
    /**
     * @brief Default constructor
     * 
     * This constructor creates an empty DigitalIntervalSeries
     */
    DigitalIntervalSeries() = default;

    /**
     * @brief Constructor for DigitalIntervalSeries from a vector of intervals
     * 
     * @param digital_vector Vector of intervals
     */
    explicit DigitalIntervalSeries(std::vector<Interval> digital_vector);

    explicit DigitalIntervalSeries(std::vector<std::pair<float, float>> const & digital_vector);

    // ========== Setters ==========

    void addEvent(Interval new_interval);

    void addEvent(TimeFrameIndex start, TimeFrameIndex end) {

        if (start > end) {
            std::cout << "Start time is greater than end time" << std::endl;
            return;
        }

        addEvent(Interval{start.getValue(), end.getValue()});
    }

    void setEventAtTime(TimeFrameIndex time, bool event);

    /**
     * @brief Remove an interval from the series
     * 
     * @param interval The interval to remove
     * @return True if the interval was found and removed, false otherwise
     */
    bool removeInterval(Interval const & interval);

    /**
     * @brief Remove multiple intervals from the series
     * 
     * @param intervals The intervals to remove
     * @return The number of intervals that were successfully removed
     */
    size_t removeIntervals(std::vector<Interval> const & intervals);

    template<typename T, typename B>
    void setEventsAtTimes(std::vector<T> times, std::vector<B> events) {
        for (int64_t i = 0; i < times.size(); ++i) {
            _setEventAtTime(TimeFrameIndex(times[i]), events[i]);
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

    // ========== Getters ==========

    [[nodiscard]] std::vector<Interval> const & getDigitalIntervalSeries() const;

    [[nodiscard]] bool isEventAtTime(TimeFrameIndex time) const;

    [[nodiscard]] size_t size() const { return _data.size(); };



    
    // Defines how to handle intervals that overlap with range boundaries
    enum class RangeMode {
        CONTAINED,  // Only intervals fully contained within range
        OVERLAPPING,// Any interval that overlaps with range
        CLIP        // Clip intervals at range boundaries
    };

    template<RangeMode mode = RangeMode::CONTAINED>
    auto getIntervalsInRange(
            int64_t start_time,
            int64_t stop_time) const {

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
            return std::views::empty<Interval>;
        }
    }

    template<RangeMode mode = RangeMode::CONTAINED>
    auto getIntervalsInRange(
            TimeFrameIndex start_time,
            TimeFrameIndex stop_time,
            TimeFrame const * source_timeframe,
            TimeFrame const * target_timeframe
        ) const {
             if (source_timeframe == target_timeframe) {
                 return getIntervalsInRange<mode>(start_time.getValue(), stop_time.getValue());
            }

            // If either timeframe is null, fall back to original behavior
            if (!source_timeframe || !target_timeframe) {
                return getIntervalsInRange<mode>(start_time.getValue(), stop_time.getValue());
            }

            // Convert the time index from source timeframe to target timeframe
            // 1. Get the time value from the source timeframe
            auto start_time_value = source_timeframe->getTimeAtIndex(start_time);
            auto stop_time_value = source_timeframe->getTimeAtIndex(stop_time);

            // 2. Convert that time value to an index in the target timeframe
            auto target_start_index = target_timeframe->getIndexAtTime(static_cast<float>(start_time_value));
            auto target_stop_index = target_timeframe->getIndexAtTime(static_cast<float>(stop_time_value));

            return getIntervalsInRange<mode>(target_start_index.getValue(), target_stop_index.getValue());
        };

private:
    std::vector<Interval> _data{};

    void _addEvent(Interval new_interval);
    void _setEventAtTime(TimeFrameIndex time, bool event);
    void _removeEventAtTime(TimeFrameIndex time);

    void _sortData();

    // Helper method to handle clipping intervals at range boundaries
    std::vector<Interval> _getIntervalsAsVectorClipped(
            int64_t start_time,
            int64_t stop_time) const {

        std::vector<Interval> result;

        for (auto const & interval: _data) {

            // Skip if not overlapping
            if (interval.end < start_time || interval.start > stop_time)
                continue;

            // Create a new clipped interval based on original interval values
            // but clipped at the transformed boundaries
            int64_t clipped_start = interval.start;
            if (clipped_start < start_time) {
                // Binary search or interpolation to find the original value
                // that transforms to start_time would be ideal here
                // For now, use a simple approach:
                while (clipped_start < start_time && clipped_start < interval.end)
                    clipped_start++;
            }

            int64_t clipped_end = interval.end;
            if (clipped_end > stop_time) {
                while (clipped_end > interval.start && clipped_end > stop_time)
                    clipped_end--;
            }

            result.push_back(Interval{clipped_start, clipped_end});
        }

        return result;
    }
};

int find_closest_preceding_event(DigitalIntervalSeries * digital_series, TimeFrameIndex time);

#endif// DIGITAL_INTERVAL_SERIES_HPP
