#ifndef DIGITAL_TIME_SERIES_BUILDER_HPP
#define DIGITAL_TIME_SERIES_BUILDER_HPP

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <vector>
#include <memory>

/**
 * @brief Lightweight builder for DigitalEventSeries objects
 * 
 * Provides fluent API for constructing DigitalEventSeries test data.
 * 
 * @example Simple event series
 * @code
 * auto events = DigitalEventSeriesBuilder()
 *     .withEvents({0, 10, 20, 30})
 *     .build();
 * @endcode
 */
class DigitalEventSeriesBuilder {
public:
    DigitalEventSeriesBuilder() = default;

    /**
     * @brief Specify event times explicitly
     * @param times Vector of event time indices
     */
    DigitalEventSeriesBuilder& withEvents(std::vector<int> times) {
        m_event_times.clear();
        for (int t : times) {
            m_event_times.emplace_back(t);
        }
        return *this;
    }

    /**
     * @brief Add a single event
     * @param time Event time index
     */
    DigitalEventSeriesBuilder& addEvent(int time) {
        m_event_times.emplace_back(time);
        return *this;
    }

    /**
     * @brief Create evenly-spaced events
     * @param start Starting time
     * @param end Ending time (inclusive)
     * @param interval Interval between events
     */
    DigitalEventSeriesBuilder& withInterval(int start, int end, int interval) {
        m_event_times.clear();
        for (int t = start; t <= end; t += interval) {
            m_event_times.emplace_back(t);
        }
        return *this;
    }

    /**
     * @brief Build the DigitalEventSeries
     * @return Shared pointer to constructed DigitalEventSeries
     */
    std::shared_ptr<DigitalEventSeries> build() const {
        return std::make_shared<DigitalEventSeries>(m_event_times);
    }

private:
    std::vector<TimeFrameIndex> m_event_times;
};

/**
 * @brief Lightweight builder for DigitalIntervalSeries objects
 * 
 * Provides fluent API for constructing DigitalIntervalSeries test data.
 * 
 * @example Simple interval series
 * @code
 * auto intervals = DigitalIntervalSeriesBuilder()
 *     .withInterval(0, 10)
 *     .withInterval(20, 30)
 *     .build();
 * @endcode
 */
class DigitalIntervalSeriesBuilder {
public:
    DigitalIntervalSeriesBuilder() = default;

    /**
     * @brief Add an interval
     * @param start Interval start time
     * @param end Interval end time
     */
    DigitalIntervalSeriesBuilder& withInterval(int start, int end) {
        m_intervals.emplace_back(start, end);
        return *this;
    }

    /**
     * @brief Add an interval (TimeFrameIndex version)
     * @param start Interval start time
     * @param end Interval end time
     */
    DigitalIntervalSeriesBuilder& withInterval(TimeFrameIndex start, TimeFrameIndex end) {
        m_intervals.emplace_back(start, end);
        return *this;
    }

    /**
     * @brief Add multiple intervals
     * @param intervals Vector of intervals
     */
    DigitalIntervalSeriesBuilder& withIntervals(const std::vector<Interval>& intervals) {
        m_intervals.insert(m_intervals.end(), intervals.begin(), intervals.end());
        return *this;
    }

    /**
     * @brief Create evenly-spaced non-overlapping intervals
     * @param start Starting time
     * @param end Ending time
     * @param interval_duration Duration of each interval
     * @param gap Gap between intervals
     */
    DigitalIntervalSeriesBuilder& withPattern(int start, int end, int interval_duration, int gap) {
        m_intervals.clear();
        int current = start;
        while (current + interval_duration <= end) {
            m_intervals.emplace_back(current, current + interval_duration);
            current += interval_duration + gap;
        }
        return *this;
    }

    /**
     * @brief Build the DigitalIntervalSeries
     * @return Shared pointer to constructed DigitalIntervalSeries
     */
    std::shared_ptr<DigitalIntervalSeries> build() const {
        return std::make_shared<DigitalIntervalSeries>(m_intervals);
    }

private:
    std::vector<Interval> m_intervals;
};

#endif // DIGITAL_TIME_SERIES_BUILDER_HPP
