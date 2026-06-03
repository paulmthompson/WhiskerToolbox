#ifndef ANALOG_EVENT_THRESHOLD_TEST_HELPERS_HPP
#define ANALOG_EVENT_THRESHOLD_TEST_HELPERS_HPP

#include "fixtures/builders/AnalogTimeSeriesBuilder.hpp"
#include "fixtures/vectors/analog/analog_event_threshold_vectors.hpp"
#include "transforms/AnalogTimeSeries/Analog_Event_Threshold/analog_event_threshold.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>

#include <memory>
#include <span>
#include <vector>

/**
 * @file analog_event_threshold_test_helpers.hpp
 * @brief Shared builders and assertions for AnalogEventThreshold v1/v2 tests
 */

namespace analog_event_threshold_test {

using analog_event_threshold_vectors::Case;
using analog_event_threshold_vectors::Direction;

inline std::shared_ptr<AnalogTimeSeries> buildAnalogTimeSeries(Case const& tc) {
    return AnalogTimeSeriesBuilder().withValues(tc.values).atTimes(tc.times).build();
}

inline ThresholdParams toThresholdParams(Case const& tc) {
    ThresholdParams params;
    params.thresholdValue = static_cast<double>(tc.threshold);
    params.lockoutTime = static_cast<double>(tc.lockout);
    switch (tc.direction) {
    case Direction::positive:
        params.direction = ThresholdParams::ThresholdDirection::POSITIVE;
        break;
    case Direction::negative:
        params.direction = ThresholdParams::ThresholdDirection::NEGATIVE;
        break;
    case Direction::absolute:
        params.direction = ThresholdParams::ThresholdDirection::ABSOLUTE;
        break;
    }
    return params;
}

inline void requireEventTimes(DigitalEventSeries const& events, std::span<int const> expected_times) {
    std::vector<TimeFrameIndex> expected;
    expected.reserve(expected_times.size());
    for (int t : expected_times) {
        expected.emplace_back(t);
    }
    REQUIRE_THAT(events.view() | std::views::transform([](auto const& e) { return e.time(); }),
                 Catch::Matchers::RangeEquals(expected));
}

} // namespace analog_event_threshold_test

#endif // ANALOG_EVENT_THRESHOLD_TEST_HELPERS_HPP
