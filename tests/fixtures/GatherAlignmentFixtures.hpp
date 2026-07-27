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

}// namespace Neuralyzer::Test::GatherFixtures

#endif// GATHER_ALIGNMENT_FIXTURES_HPP
