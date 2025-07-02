#ifndef DIGITAL_INTERVAL_SERIES_HPP
#define DIGITAL_INTERVAL_SERIES_HPP

#include "Observer/Observer_Data.hpp"
#include "interval_data.hpp"

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

    template<typename T>
    void addEvent(T start, T end) {

        if (start > end) {
            std::cout << "Start time is greater than end time" << std::endl;
            return;
        }

        addEvent(Interval{static_cast<int64_t>(start), static_cast<int64_t>(end)});
    }

    void setEventAtTime(int time, bool event);

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

    // ========== Getters ==========

    [[nodiscard]] std::vector<Interval> const & getDigitalIntervalSeries() const;

    [[nodiscard]] bool isEventAtTime(int time) const;

    [[nodiscard]] size_t size() const { return _data.size(); };

    
    // Defines how to handle intervals that overlap with range boundaries
    enum class RangeMode {
        CONTAINED,  // Only intervals fully contained within range
        OVERLAPPING,// Any interval that overlaps with range
        CLIP        // Clip intervals at range boundaries
    };

    template<RangeMode mode = RangeMode::CONTAINED, typename TransformFunc = std::identity>
    auto getIntervalsInRange(
            int64_t start_time,
            int64_t stop_time,
            TransformFunc && time_transform = {}) const {

        if constexpr (mode == RangeMode::CONTAINED) {
            return _data | std::views::filter([start_time, stop_time, time_transform](Interval const & interval) {
                       auto transformed_start = time_transform(interval.start);
                       auto transformed_end = time_transform(interval.end);
                       return transformed_start >= start_time && transformed_end <= stop_time;
                   });
        } else if constexpr (mode == RangeMode::OVERLAPPING) {
            return _data | std::views::filter([start_time, stop_time, time_transform](Interval const & interval) {
                       auto transformed_start = time_transform(interval.start);
                       auto transformed_end = time_transform(interval.end);
                       return transformed_start <= stop_time && transformed_end >= start_time;
                   });
        } else if constexpr (mode == RangeMode::CLIP) {
            // For CLIP mode, we return a vector since we need to modify intervals
            return _getIntervalsAsVectorClipped(start_time, stop_time, time_transform);
        } else {
            static_assert(always_false_v<TransformFunc>, "Unhandled IntervalQueryMode");
            return std::views::empty<Interval>;
        }
    }

    // Get vector of intervals in range (for backward compatibility)
    template<RangeMode mode = RangeMode::CONTAINED>
    [[nodiscard]] std::vector<Interval> getIntervalsAsVector(int64_t start_time, int64_t stop_time) const {
        if constexpr (mode == RangeMode::CLIP) {
            return _getIntervalsAsVectorClipped(start_time, stop_time);
        }

        auto range = getIntervalsInRange<mode>(start_time, stop_time);
        return {std::ranges::begin(range), std::ranges::end(range)};
    }

private:
    std::vector<Interval> _data{};

    void _addEvent(Interval new_interval);
    void _setEventAtTime(int time, bool event);
    void _removeEventAtTime(int time);

    void _sortData();

    // Helper method to handle clipping intervals at range boundaries
    template<typename TransformFunc = std::identity>
    std::vector<Interval> _getIntervalsAsVectorClipped(
            int64_t start_time,
            int64_t stop_time,
            TransformFunc && time_transform = {}) const {

        std::vector<Interval> result;

        for (auto const & interval: _data) {

            auto transformed_start = time_transform(interval.start);
            auto transformed_end = time_transform(interval.end);

            // Skip if not overlapping
            if (transformed_end < start_time || transformed_start > stop_time)
                continue;

            // Create a new clipped interval based on original interval values
            // but clipped at the transformed boundaries
            int64_t clipped_start = interval.start;
            if (transformed_start < start_time) {
                // Binary search or interpolation to find the original value
                // that transforms to start_time would be ideal here
                // For now, use a simple approach:
                while (time_transform(clipped_start) < start_time && clipped_start < interval.end)
                    clipped_start++;
            }

            int64_t clipped_end = interval.end;
            if (transformed_end > stop_time) {
                while (time_transform(clipped_end) > stop_time && clipped_end > interval.start)
                    clipped_end--;
            }

            result.push_back(Interval{clipped_start, clipped_end});
        }

        return result;
    }
};

int find_closest_preceding_event(DigitalIntervalSeries * digital_series, int time);

#endif// DIGITAL_INTERVAL_SERIES_HPP
