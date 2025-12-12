#ifndef DIGITAL_EVENT_SCENARIOS_HPP
#define DIGITAL_EVENT_SCENARIOS_HPP

#include "../builders/DigitalEventBuilder.hpp"

/**
 * @brief Pre-configured scenarios for digital event testing
 * 
 * Provides common test patterns used across MVP matrix tests,
 * rendering tests, and display option tests.
 */
namespace digital_event_scenarios {

/**
 * @brief Few events scenario - good for basic visualization testing
 */
inline std::vector<EventData> few_events() {
    return DigitalEventBuilder()
        .withRandomEvents(10, 100.0f, 42)
        .build();
}

/**
 * @brief Many events scenario - tests alpha reduction logic
 */
inline std::vector<EventData> many_events() {
    return DigitalEventBuilder()
        .withRandomEvents(150, 100.0f, 42)
        .build();
}

/**
 * @brief Very dense events - tests line thickness reduction
 */
inline std::vector<EventData> very_dense_events() {
    return DigitalEventBuilder()
        .withRandomEvents(250, 100.0f, 42)
        .build();
}

/**
 * @brief Dense events for rendering optimization tests
 */
inline std::vector<EventData> dense_events_for_rendering() {
    return DigitalEventBuilder()
        .withRandomEvents(500, 1000.0f, 123)
        .build();
}

/**
 * @brief Regular periodic events
 */
inline std::vector<EventData> regular_periodic_events() {
    return DigitalEventBuilder()
        .withRegularEvents(0.0f, 1000.0f, 50.0f)
        .build();
}

/**
 * @brief Sparse events with wide spacing
 */
inline std::vector<EventData> sparse_events() {
    return DigitalEventBuilder()
        .withRegularEvents(0.0f, 10000.0f, 1000.0f)
        .build();
}

/**
 * @brief Custom event times for specific test cases
 */
inline std::vector<EventData> custom_events(std::vector<float> const& times) {
    return DigitalEventBuilder()
        .withTimes(times)
        .build();
}

/**
 * @brief Display options for stacked mode with allocation
 */
inline NewDigitalEventSeriesDisplayOptions stacked_options(float center, float height) {
    return DigitalEventDisplayOptionsBuilder()
        .withStackedMode()
        .withAllocation(center, height)
        .withMarginFactor(0.95f)
        .build();
}

/**
 * @brief Display options for full canvas mode
 */
inline NewDigitalEventSeriesDisplayOptions full_canvas_options() {
    return DigitalEventDisplayOptionsBuilder()
        .withFullCanvasMode()
        .withMarginFactor(0.95f)
        .build();
}

/**
 * @brief Display options with intrinsic properties applied
 */
inline NewDigitalEventSeriesDisplayOptions options_with_intrinsics(std::vector<EventData> const& events) {
    return DigitalEventDisplayOptionsBuilder()
        .withStackedMode()
        .withIntrinsicProperties(events)
        .build();
}

} // namespace digital_event_scenarios

#endif // DIGITAL_EVENT_SCENARIOS_HPP
