#ifndef UNIFORM_INTERVAL_TEST_TIME_FRAME_HPP
#define UNIFORM_INTERVAL_TEST_TIME_FRAME_HPP

/**
 * @file UniformIntervalTestTimeFrame.hpp
 * @brief Shared identity TimeFrame helpers for digital time-series tests
 *
 * Provides a uniform mapping TimeFrameIndex @f$i@f$ → ClockTicks @f$i@f$ so tests
 * can assert on @c view() results in clock-tick space while constructing data in
 * index space.
 */

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/ClockTicks.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "fixtures/builders/TimeFrameBuilder.hpp"

#include <cstddef>
#include <memory>
#include <unordered_map>

namespace uniform_interval_test {

/// Default frame count for tests that only use modest index ranges.
inline constexpr std::size_t k_default_frame_count = 10'000;

/**
 * @brief Identity uniform TimeFrame with @p frame_count entries.
 *
 * Frame @f$i@f$ maps to clock tick @f$i@f$. Results are cached per @p frame_count
 * for the lifetime of the test process.
 *
 * @param frame_count Number of frames (must be > 0)
 */
[[nodiscard]] inline std::shared_ptr<TimeFrame> uniformIntervalTestTimeFrame(
        std::size_t frame_count = k_default_frame_count) {
    static std::unordered_map<std::size_t, std::shared_ptr<TimeFrame>> cache;

    auto const it = cache.find(frame_count);
    if (it != cache.end()) {
        return it->second;
    }

    auto frame = TimeFrameBuilder().withCount(frame_count).build();
    cache.emplace(frame_count, frame);
    return frame;
}

/// Attach a uniform identity TimeFrame so @c DigitalIntervalSeries::view() is valid.
inline void assignUniformIntervalTestTimeFrame(
        DigitalIntervalSeries & series,
        std::size_t frame_count = k_default_frame_count) {
    series.setTimeFrame(uniformIntervalTestTimeFrame(frame_count));
}

inline void assignUniformIntervalTestTimeFrame(
        std::shared_ptr<DigitalIntervalSeries> const & series,
        std::size_t frame_count = k_default_frame_count) {
    if (series) {
        assignUniformIntervalTestTimeFrame(*series, frame_count);
    }
}

/// Attach a uniform identity TimeFrame so @c DigitalEventSeries::view() is valid.
inline void assignUniformIntervalTestTimeFrame(
        DigitalEventSeries & series,
        std::size_t frame_count = k_default_frame_count) {
    series.setTimeFrame(uniformIntervalTestTimeFrame(frame_count));
}

inline void assignUniformIntervalTestTimeFrame(
        std::shared_ptr<DigitalEventSeries> const & series,
        std::size_t frame_count = k_default_frame_count) {
    if (series) {
        assignUniformIntervalTestTimeFrame(*series, frame_count);
    }
}

/**
 * @brief Build a series from index-space intervals with TimeFrame already assigned.
 */
[[nodiscard]] inline std::shared_ptr<DigitalIntervalSeries> makeIntervalSeries(
        std::vector<TimeFrameInterval> intervals,
        std::size_t frame_count = k_default_frame_count) {
    auto series = std::make_shared<DigitalIntervalSeries>(std::move(intervals));
    assignUniformIntervalTestTimeFrame(*series, frame_count);
    return series;
}

/**
 * @brief Convert a stored index to the matching clock tick under an identity TimeFrame.
 */
[[nodiscard]] inline ClockTicks tick(TimeFrameIndex index) {
    return ClockTicks{index.getValue()};
}

/**
 * @brief Map a stored index to clock ticks using an arbitrary TimeFrame.
 */
[[nodiscard]] inline ClockTicks clockAt(TimeFrame const & time_frame, TimeFrameIndex index) {
    return time_frame.getTimeAtIndex(index);
}

}// namespace uniform_interval_test

#endif// UNIFORM_INTERVAL_TEST_TIME_FRAME_HPP
