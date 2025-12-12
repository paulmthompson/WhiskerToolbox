#ifndef DIGITAL_INTERVAL_SCENARIOS_HPP
#define DIGITAL_INTERVAL_SCENARIOS_HPP

#include "../builders/DigitalIntervalBuilder.hpp"

/**
 * @brief Pre-configured scenarios for digital interval testing
 * 
 * Provides common test patterns used across MVP matrix tests,
 * rendering tests, and display option tests.
 */
namespace digital_interval_scenarios {

/**
 * @brief Few intervals scenario - good for basic visualization testing
 */
inline std::vector<Interval> few_intervals() {
    return DigitalIntervalBuilder()
        .withRandomIntervals(5, 1000.0f, 50.0f, 200.0f, 42)
        .build();
}

/**
 * @brief Many intervals scenario - tests overlap and alpha reduction
 */
inline std::vector<Interval> many_intervals() {
    return DigitalIntervalBuilder()
        .withRandomIntervals(20, 1000.0f, 50.0f, 300.0f, 42)
        .build();
}

/**
 * @brief Non-overlapping intervals for clean testing
 */
inline std::vector<Interval> non_overlapping_intervals() {
    return DigitalIntervalBuilder()
        .withNonOverlappingIntervals(10, 10000.0f, 100.0f, 200.0f, 42)
        .build();
}

/**
 * @brief Regular periodic intervals
 */
inline std::vector<Interval> regular_periodic_intervals() {
    return DigitalIntervalBuilder()
        .withRegularIntervals(0.0f, 1000.0f, 200.0f, 100.0f)
        .build();
}

/**
 * @brief Dense overlapping intervals for rendering optimization tests
 */
inline std::vector<Interval> dense_overlapping_intervals() {
    return DigitalIntervalBuilder()
        .withRandomIntervals(50, 1000.0f, 100.0f, 400.0f, 123)
        .build();
}

/**
 * @brief Sparse intervals with wide spacing
 */
inline std::vector<Interval> sparse_intervals() {
    return DigitalIntervalBuilder()
        .withNonOverlappingIntervals(5, 10000.0f, 1000.0f, 200.0f, 42)
        .build();
}

/**
 * @brief Custom intervals for specific test cases
 */
inline std::vector<Interval> custom_intervals(std::vector<std::pair<long, long>> const& times) {
    DigitalIntervalBuilder builder;
    for (auto const& [start, end] : times) {
        builder.addInterval(start, end);
    }
    return builder.build();
}

/**
 * @brief Display options with intrinsic properties applied
 */
inline NewDigitalIntervalSeriesDisplayOptions options_with_intrinsics(std::vector<Interval> const& intervals) {
    return DigitalIntervalDisplayOptionsBuilder()
        .withIntrinsicProperties(intervals)
        .build();
}

} // namespace digital_interval_scenarios

#endif // DIGITAL_INTERVAL_SCENARIOS_HPP
