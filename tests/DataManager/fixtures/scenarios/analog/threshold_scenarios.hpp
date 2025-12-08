#ifndef THRESHOLD_SCENARIOS_HPP
#define THRESHOLD_SCENARIOS_HPP

#include "fixtures/builders/AnalogTimeSeriesBuilder.hpp"
#include <memory>

/**
 * @brief Threshold-related test scenarios for AnalogTimeSeries
 * 
 * This namespace contains pre-configured test data for threshold detection
 * algorithms. These scenarios are extracted from existing test fixtures
 * to enable reuse across v1 and v2 transform tests.
 */
namespace analog_scenarios {

/**
 * @brief Signal with positive threshold crossings and no lockout
 * 
 * Data: {0.5, 1.5, 0.8, 2.5, 1.2}
 * Times: {100, 200, 300, 400, 500}
 * 
 * With threshold=1.0, positive direction:
 *   - Event at t=200 (crosses from 0.5 to 1.5)
 *   - Event at t=400 (crosses from 0.8 to 2.5)
 *   - Event at t=500 (crosses from 2.5 to 1.2, still above threshold)
 */
inline std::shared_ptr<AnalogTimeSeries> positive_threshold_no_lockout() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 0.8f, 2.5f, 1.2f})
        .atTimes({100, 200, 300, 400, 500})
        .build();
}

/**
 * @brief Signal with positive threshold crossings and lockout period
 * 
 * Data: {0.5, 1.5, 1.8, 0.5, 2.5, 2.2}
 * Times: {100, 200, 300, 400, 500, 600}
 * 
 * With threshold=1.0, positive direction, lockout=150:
 *   - Event at t=200 (crosses from 0.5 to 1.5)
 *   - No event at t=300 (within lockout period)
 *   - Event at t=500 (crosses from 0.5 to 2.5, outside lockout)
 *   - No event at t=600 (within lockout period)
 */
inline std::shared_ptr<AnalogTimeSeries> positive_threshold_with_lockout() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 1.8f, 0.5f, 2.5f, 2.2f})
        .atTimes({100, 200, 300, 400, 500, 600})
        .build();
}

/**
 * @brief Signal with negative threshold crossings and no lockout
 * 
 * Data: {0.5, -1.5, -0.8, -2.5, -1.2}
 * Times: {100, 200, 300, 400, 500}
 * 
 * With threshold=-1.0, negative direction:
 *   - Event at t=200 (crosses from 0.5 to -1.5)
 *   - Event at t=400 (crosses from -0.8 to -2.5)
 *   - Event at t=500 (crosses from -2.5 to -1.2, still below threshold)
 */
inline std::shared_ptr<AnalogTimeSeries> negative_threshold_no_lockout() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, -1.5f, -0.8f, -2.5f, -1.2f})
        .atTimes({100, 200, 300, 400, 500})
        .build();
}

/**
 * @brief Signal with negative threshold crossings and lockout period
 * 
 * Data: {0.0, -1.5, -1.2, 0.0, -2.0, -0.5}
 * Times: {100, 200, 300, 400, 500, 600}
 * 
 * With threshold=-1.0, negative direction, lockout=150:
 *   - Event at t=200 (crosses from 0.0 to -1.5)
 *   - No event at t=300 (within lockout period)
 *   - Event at t=500 (crosses from 0.0 to -2.0, outside lockout)
 *   - No event at t=600 (within lockout period)
 */
inline std::shared_ptr<AnalogTimeSeries> negative_threshold_with_lockout() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.0f, -1.5f, -1.2f, 0.0f, -2.0f, -0.5f})
        .atTimes({100, 200, 300, 400, 500, 600})
        .build();
}

/**
 * @brief Signal with absolute value threshold crossings and no lockout
 * 
 * Data: {0.5, -1.5, 0.8, 2.5, -1.2, 0.9}
 * Times: {100, 200, 300, 400, 500, 600}
 * 
 * With threshold=1.0, absolute direction:
 *   - Event at t=200 (|0.5| to |-1.5| = 1.5)
 *   - Event at t=400 (|0.8| to |2.5| = 2.5)
 *   - Event at t=500 (|2.5| to |-1.2| = 1.2)
 */
inline std::shared_ptr<AnalogTimeSeries> absolute_threshold_no_lockout() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, -1.5f, 0.8f, 2.5f, -1.2f, 0.9f})
        .atTimes({100, 200, 300, 400, 500, 600})
        .build();
}

/**
 * @brief Signal with absolute value threshold crossings and lockout period
 * 
 * Data: {0.5, 1.5, -1.2, 0.5, -2.0, 0.8}
 * Times: {100, 200, 300, 400, 500, 600}
 * 
 * With threshold=1.0, absolute direction, lockout=150:
 *   - Event at t=200 (|0.5| to |1.5| = 1.5)
 *   - No event at t=300 (within lockout period)
 *   - Event at t=500 (|0.5| to |-2.0| = 2.0, outside lockout)
 *   - No event at t=600 (within lockout period)
 */
inline std::shared_ptr<AnalogTimeSeries> absolute_threshold_with_lockout() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, -1.2f, 0.5f, -2.0f, 0.8f})
        .atTimes({100, 200, 300, 400, 500, 600})
        .build();
}

/**
 * @brief Signal that never crosses the threshold
 * 
 * Data: {0.5, 1.5, 0.8, 2.5, 1.2}
 * Times: {100, 200, 300, 400, 500}
 * 
 * With threshold=10.0:
 *   - No events expected (all values below threshold)
 */
inline std::shared_ptr<AnalogTimeSeries> no_events_high_threshold() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 0.8f, 2.5f, 1.2f})
        .atTimes({100, 200, 300, 400, 500})
        .build();
}

/**
 * @brief Signal where all samples cross the threshold
 * 
 * Data: {0.5, 1.5, 0.8, 2.5, 1.2}
 * Times: {100, 200, 300, 400, 500}
 * 
 * With threshold=0.1, positive direction:
 *   - Events at all time points (all values above threshold)
 */
inline std::shared_ptr<AnalogTimeSeries> all_events_low_threshold() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 0.8f, 2.5f, 1.2f})
        .atTimes({100, 200, 300, 400, 500})
        .build();
}

/**
 * @brief Empty signal (no data)
 * 
 * Data: {}
 * Times: {}
 * 
 * Expected: No events, regardless of threshold
 */
inline std::shared_ptr<AnalogTimeSeries> empty_signal() {
    return AnalogTimeSeriesBuilder()
        .withValues({})
        .atTimes({})
        .build();
}

/**
 * @brief Signal with lockout time larger than series duration
 * 
 * Data: {1.5, 2.5, 3.5}
 * Times: {100, 200, 300}
 * 
 * With threshold=1.0, lockout=1000:
 *   - Event at t=100 (first crossing)
 *   - No more events (all within lockout period)
 */
inline std::shared_ptr<AnalogTimeSeries> lockout_larger_than_duration() {
    return AnalogTimeSeriesBuilder()
        .withValues({1.5f, 2.5f, 3.5f})
        .atTimes({100, 200, 300})
        .build();
}

/**
 * @brief Signal with values exactly at threshold
 * 
 * Data: {0.5, 1.0, 1.5}
 * Times: {100, 200, 300}
 * 
 * With threshold=1.0:
 *   - Behavior depends on implementation (>= vs >)
 *   - Tests boundary condition handling
 */
inline std::shared_ptr<AnalogTimeSeries> events_at_threshold() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.0f, 1.5f})
        .atTimes({100, 200, 300})
        .build();
}

/**
 * @brief Signal with timestamps starting from zero
 * 
 * Data: {1.5, 0.5, 2.5}
 * Times: {0, 10, 20}
 * 
 * With threshold=1.0:
 *   - Tests zero-based time handling
 */
inline std::shared_ptr<AnalogTimeSeries> zero_based_timestamps() {
    return AnalogTimeSeriesBuilder()
        .withValues({1.5f, 0.5f, 2.5f})
        .atTimes({0, 10, 20})
        .build();
}

} // namespace analog_scenarios

#endif // THRESHOLD_SCENARIOS_HPP
