/**
 * @file GatherAlignmentFixtures.hpp
 * @brief Shared test fixtures for GatherResult alignment and cross-timeframe tests
 */

#ifndef GATHER_ALIGNMENT_FIXTURES_HPP
#define GATHER_ALIGNMENT_FIXTURES_HPP

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

namespace Neuralyzer::Test::GatherFixtures {

/**
 * @brief Create a TimeFrame mapping indices to absolute time
 *
 * For example, 500 Hz events mapped to a 30 kHz spike time base use
 * `samples_per_index = 60` (30000 / 500).
 */
[[nodiscard]] inline std::shared_ptr<TimeFrame> createTimeFrameForRate(int indices,
                                                                       int samples_per_index) {
    std::vector<int> times;
    times.reserve(static_cast<std::size_t>(indices));
    for (int i = 0; i < indices; ++i) {
        times.push_back(i * samples_per_index);
    }
    return std::make_shared<TimeFrame>(times);
}

/**
 * @brief Create an identity TimeFrame where index equals absolute time
 */
[[nodiscard]] inline std::shared_ptr<TimeFrame> createIdentityTimeFrame(int size) {
    std::vector<int> times;
    times.reserve(static_cast<std::size_t>(size));
    for (int i = 0; i < size; ++i) {
        times.push_back(i);
    }
    return std::make_shared<TimeFrame>(times);
}

/**
 * @brief Create an identity TimeFrame large enough for the supplied maximum time.
 */
[[nodiscard]] inline std::shared_ptr<TimeFrame> createIdentityTimeFrameForMax(int64_t max_time) {
    auto const size = static_cast<int>(std::max<int64_t>(max_time + 10'000, 10'000));
    std::vector<int> times(static_cast<std::size_t>(size));
    std::iota(times.begin(), times.end(), 0);
    return std::make_shared<TimeFrame>(times);
}

/**
 * @brief Create a DigitalEventSeries with events at specified index times
 */
[[nodiscard]] inline std::shared_ptr<DigitalEventSeries>
createEventSeries(std::vector<int64_t> const & times) {
    auto series = std::make_shared<DigitalEventSeries>();
    auto const max_time = times.empty() ? int64_t{0} : *std::max_element(times.begin(), times.end());
    series->setTimeFrame(createIdentityTimeFrameForMax(max_time));
    for (auto t: times) {
        series->addEvent(TimeFrameIndex(t));
    }
    return series;
}

/**
 * @brief Create a DigitalIntervalSeries from start/end pairs
 */
[[nodiscard]] inline std::shared_ptr<DigitalIntervalSeries>
createIntervalSeries(std::vector<std::pair<int64_t, int64_t>> const & intervals) {
    std::vector<Interval> interval_vec;
    interval_vec.reserve(intervals.size());
    int64_t max_time = 0;
    for (auto const & [start, end]: intervals) {
        interval_vec.push_back(Interval{start, end});
        max_time = std::max(max_time, end);
    }
    auto series = std::make_shared<DigitalIntervalSeries>(std::move(interval_vec));
    series->setTimeFrame(createIdentityTimeFrameForMax(max_time));
    return series;
}

/**
 * @brief Standard 30 kHz spike / 500 Hz alignment event rate ratio
 */
inline constexpr int kSpikeSamplesPerEventIndex = 60;

/**
 * @brief Create overlapping gather windows around each event.
 *
 * @pre @p events must not be null.
 * @post The returned windows preserve one row per input event and carry a
 *       TimeFrame whose interval indices resolve to absolute window bounds.
 */
[[nodiscard]] inline std::shared_ptr<DigitalIntervalSeries> createWindowsAroundEvents(
        std::shared_ptr<DigitalEventSeries> const & events,
        int64_t pre_window,
        int64_t post_window) {
    auto const time_frame = events->getTimeFrame();
    std::vector<Interval> absolute_windows;
    absolute_windows.reserve(events->size());

    for (auto const & event: events->view()) {
        auto const event_index = event.time();
        auto const event_time = time_frame ? static_cast<int64_t>(time_frame->getTimeAtIndex(event_index))
                                           : event_index.getValue();
        absolute_windows.push_back(Interval{
                event_time - pre_window,
                event_time + post_window});
    }

    std::vector<int> boundary_times;
    boundary_times.reserve(absolute_windows.size() * 2);
    for (auto const & window: absolute_windows) {
        boundary_times.push_back(static_cast<int>(window.start));
        boundary_times.push_back(static_cast<int>(window.end));
    }

    std::ranges::sort(boundary_times);
    auto const last_unique = std::ranges::unique(boundary_times);
    boundary_times.erase(last_unique.begin(), last_unique.end());

    std::vector<Interval> windows;
    windows.reserve(absolute_windows.size());
    for (auto const & window: absolute_windows) {
        auto const start = std::ranges::lower_bound(boundary_times, static_cast<int>(window.start));
        auto const end = std::ranges::lower_bound(boundary_times, static_cast<int>(window.end));
        windows.push_back(Interval{
                std::distance(boundary_times.begin(), start),
                std::distance(boundary_times.begin(), end)});
    }

    return DigitalIntervalSeries::createOverlapping(
            std::move(windows),
            std::make_shared<TimeFrame>(boundary_times));
}

enum class TestIntervalAlignmentPoint {
    Start,
    End,
    Center,
};

/**
 * @brief Create one alignment event per interval at the requested point.
 *
 * @pre @p intervals must not be null.
 * @post The returned event series uses the interval series TimeFrame.
 */
[[nodiscard]] inline std::shared_ptr<DigitalEventSeries> createAlignmentEventsForIntervals(
        std::shared_ptr<DigitalIntervalSeries> const & intervals,
        TestIntervalAlignmentPoint point) {
    auto events = std::make_shared<DigitalEventSeries>();
    events->setTimeFrame(intervals->getTimeFrame());

    for (auto const & interval: intervals->view()) {
        int64_t alignment_time = interval.interval.start;
        switch (point) {
            case TestIntervalAlignmentPoint::Start:
                alignment_time = interval.interval.start;
                break;
            case TestIntervalAlignmentPoint::End:
                alignment_time = interval.interval.end;
                break;
            case TestIntervalAlignmentPoint::Center:
                alignment_time = (interval.interval.start + interval.interval.end) / 2;
                break;
        }
        events->addEvent(TimeFrameIndex(alignment_time));
    }

    return events;
}

}// namespace Neuralyzer::Test::GatherFixtures

#endif// GATHER_ALIGNMENT_FIXTURES_HPP
