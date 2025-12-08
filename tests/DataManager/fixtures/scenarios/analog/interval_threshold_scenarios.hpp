#ifndef INTERVAL_THRESHOLD_SCENARIOS_HPP
#define INTERVAL_THRESHOLD_SCENARIOS_HPP

#include "fixtures/builders/AnalogTimeSeriesBuilder.hpp"
#include <memory>

/**
 * @brief Interval threshold test scenarios for AnalogTimeSeries
 * 
 * This namespace contains pre-configured test data for interval threshold detection
 * algorithms. These scenarios support testing with various threshold types (positive,
 * negative, absolute), lockout times, and minimum duration constraints.
 * 
 * Scenarios are shared between v1 and v2 transform tests.
 */
namespace analog_scenarios {

/**
 * @brief Simple positive threshold case
 * 
 * Data: {0.5, 1.5, 2.0, 1.8, 0.8, 2.5, 1.2, 0.3}
 * Times: {100, 200, 300, 400, 500, 600, 700, 800}
 * 
 * With threshold=1.0, positive direction:
 *   - Interval [200-400]: values 1.5, 2.0, 1.8
 *   - Interval [600-700]: values 2.5, 1.2
 */
inline std::shared_ptr<AnalogTimeSeries> positive_simple() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 2.0f, 1.8f, 0.8f, 2.5f, 1.2f, 0.3f})
        .atTimes({100, 200, 300, 400, 500, 600, 700, 800})
        .build();
}

/**
 * @brief Negative threshold case
 * 
 * Data: {0.5, -1.5, -2.0, -1.8, 0.8, -2.5, -1.2, 0.3}
 * Times: {100, 200, 300, 400, 500, 600, 700, 800}
 * 
 * With threshold=-1.0, negative direction:
 *   - Interval [200-400]: values -1.5, -2.0, -1.8
 *   - Interval [600-700]: values -2.5, -1.2
 */
inline std::shared_ptr<AnalogTimeSeries> negative_threshold() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, -1.5f, -2.0f, -1.8f, 0.8f, -2.5f, -1.2f, 0.3f})
        .atTimes({100, 200, 300, 400, 500, 600, 700, 800})
        .build();
}

/**
 * @brief Absolute threshold case
 * 
 * Data: {0.5, 1.5, -2.0, 1.8, 0.8, -2.5, 1.2, 0.3}
 * Times: {100, 200, 300, 400, 500, 600, 700, 800}
 * 
 * With threshold=1.0, absolute direction:
 *   - Interval [200-400]: |values| = 1.5, 2.0, 1.8
 *   - Interval [600-700]: |values| = 2.5, 1.2
 */
inline std::shared_ptr<AnalogTimeSeries> absolute_threshold() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, -2.0f, 1.8f, 0.8f, -2.5f, 1.2f, 0.3f})
        .atTimes({100, 200, 300, 400, 500, 600, 700, 800})
        .build();
}

/**
 * @brief Signal with lockout time test
 * 
 * Data: {0.5, 1.5, 0.8, 1.8, 0.5, 1.2, 0.3}
 * Times: {100, 200, 250, 300, 400, 450, 500}
 * 
 * With threshold=1.0, lockout=100:
 *   - Interval [200-200]: single sample at 200
 *   - Interval [300-300]: single sample at 300 (100 units after 200)
 *   - Interval [450-450]: single sample at 450 (150 units after 300)
 */
inline std::shared_ptr<AnalogTimeSeries> with_lockout() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 0.8f, 1.8f, 0.5f, 1.2f, 0.3f})
        .atTimes({100, 200, 250, 300, 400, 450, 500})
        .build();
}

/**
 * @brief Signal with minimum duration test
 * 
 * Data: {0.5, 1.5, 0.8, 1.8, 1.2, 1.1, 0.5}
 * Times: {100, 200, 250, 300, 400, 500, 600}
 * 
 * With threshold=1.0, min_duration=150:
 *   - Interval [300-500]: duration=200, meets minimum
 *   - Interval at [200-250] is too short (duration=50)
 */
inline std::shared_ptr<AnalogTimeSeries> with_min_duration() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 0.8f, 1.8f, 1.2f, 1.1f, 0.5f})
        .atTimes({100, 200, 250, 300, 400, 500, 600})
        .build();
}

/**
 * @brief Signal that ends while above threshold
 * 
 * Data: {0.5, 1.5, 2.0, 1.8, 1.2}
 * Times: {100, 200, 300, 400, 500}
 * 
 * With threshold=1.0:
 *   - Interval [200-500]: extends to end of signal
 */
inline std::shared_ptr<AnalogTimeSeries> ends_above_threshold() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 2.0f, 1.8f, 1.2f})
        .atTimes({100, 200, 300, 400, 500})
        .build();
}

/**
 * @brief Signal with no intervals detected
 * 
 * Data: {0.1, 0.2, 0.3, 0.4, 0.5}
 * Times: {100, 200, 300, 400, 500}
 * 
 * With threshold=1.0:
 *   - No intervals (all values below threshold)
 */
inline std::shared_ptr<AnalogTimeSeries> no_intervals() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.1f, 0.2f, 0.3f, 0.4f, 0.5f})
        .atTimes({100, 200, 300, 400, 500})
        .build();
}

/**
 * @brief Signal for progress callback testing
 * 
 * Data: {0.5, 1.5, 0.8, 2.0, 0.3}
 * Times: {100, 200, 300, 400, 500}
 */
inline std::shared_ptr<AnalogTimeSeries> progress_callback() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 0.8f, 2.0f, 0.3f})
        .atTimes({100, 200, 300, 400, 500})
        .build();
}

/**
 * @brief Complex signal with multiple parameters
 * 
 * Data: {0.0, 2.0, 1.8, 1.5, 0.5, 2.5, 2.2, 1.9, 0.8, 1.1, 0.3}
 * Times: {0, 100, 150, 200, 300, 400, 450, 500, 600, 700, 800}
 * 
 * With threshold=1.0, lockout=50, min_duration=100:
 *   - Interval [100-200]: duration=100, meets minimum
 *   - Interval [400-500]: duration=100, meets minimum
 */
inline std::shared_ptr<AnalogTimeSeries> complex_signal() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.0f, 2.0f, 1.8f, 1.5f, 0.5f, 2.5f, 2.2f, 1.9f, 0.8f, 1.1f, 0.3f})
        .atTimes({0, 100, 150, 200, 300, 400, 450, 500, 600, 700, 800})
        .build();
}

/**
 * @brief Single sample above threshold
 * 
 * Data: {2.0}
 * Times: {100}
 * 
 * With threshold=1.0:
 *   - Interval [100-100]: single point
 */
inline std::shared_ptr<AnalogTimeSeries> single_above() {
    return AnalogTimeSeriesBuilder()
        .withValues({2.0f})
        .atTimes({100})
        .build();
}

/**
 * @brief Single sample below threshold
 * 
 * Data: {0.5}
 * Times: {100}
 * 
 * With threshold=1.0:
 *   - No intervals
 */
inline std::shared_ptr<AnalogTimeSeries> single_below() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f})
        .atTimes({100})
        .build();
}

/**
 * @brief All values above threshold
 * 
 * Data: {1.5, 2.0, 1.8, 2.5, 1.2}
 * Times: {100, 200, 300, 400, 500}
 * 
 * With threshold=1.0:
 *   - Interval [100-500]: entire signal
 */
inline std::shared_ptr<AnalogTimeSeries> all_above() {
    return AnalogTimeSeriesBuilder()
        .withValues({1.5f, 2.0f, 1.8f, 2.5f, 1.2f})
        .atTimes({100, 200, 300, 400, 500})
        .build();
}

/**
 * @brief Signal with zero threshold
 * 
 * Data: {-1.0, 0.0, 1.0, -0.5, 0.5}
 * Times: {100, 200, 300, 400, 500}
 * 
 * With threshold=0.0, positive direction:
 *   - Interval [300-300]: value 1.0
 *   - Interval [500-500]: value 0.5
 */
inline std::shared_ptr<AnalogTimeSeries> zero_threshold() {
    return AnalogTimeSeriesBuilder()
        .withValues({-1.0f, 0.0f, 1.0f, -0.5f, 0.5f})
        .atTimes({100, 200, 300, 400, 500})
        .build();
}

/**
 * @brief Signal with negative threshold value
 * 
 * Data: {-2.0, -1.0, -0.5, -1.5, -0.8}
 * Times: {100, 200, 300, 400, 500}
 * 
 * With threshold=-1.0, negative direction:
 *   - Interval [100-100]: value -2.0
 *   - Interval [400-400]: value -1.5
 */
inline std::shared_ptr<AnalogTimeSeries> negative_value() {
    return AnalogTimeSeriesBuilder()
        .withValues({-2.0f, -1.0f, -0.5f, -1.5f, -0.8f})
        .atTimes({100, 200, 300, 400, 500})
        .build();
}

/**
 * @brief Signal with very large lockout time
 * 
 * Data: {0.5, 1.5, 0.8, 1.8, 0.5, 1.2}
 * Times: {100, 200, 300, 400, 500, 600}
 * 
 * With threshold=1.0, lockout=1000:
 *   - Only first interval [200-200] detected
 *   - All others within lockout period
 */
inline std::shared_ptr<AnalogTimeSeries> large_lockout() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 0.8f, 1.8f, 0.5f, 1.2f})
        .atTimes({100, 200, 300, 400, 500, 600})
        .build();
}

/**
 * @brief Signal with very large minimum duration
 * 
 * Data: {0.5, 1.5, 1.8, 1.2, 0.5}
 * Times: {100, 200, 300, 400, 500}
 * 
 * With threshold=1.0, min_duration=1000:
 *   - No intervals meet minimum duration
 */
inline std::shared_ptr<AnalogTimeSeries> large_min_duration() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 1.8f, 1.2f, 0.5f})
        .atTimes({100, 200, 300, 400, 500})
        .build();
}

/**
 * @brief Signal with irregular timestamp spacing
 * 
 * Data: {0.5, 1.5, 0.8, 1.8, 0.5}
 * Times: {0, 1, 100, 101, 1000}
 * 
 * Tests handling of non-uniform time intervals
 */
inline std::shared_ptr<AnalogTimeSeries> irregular_spacing() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 0.8f, 1.8f, 0.5f})
        .atTimes({0, 1, 100, 101, 1000})
        .build();
}

/**
 * @brief Single sample above threshold followed by below threshold
 * 
 * Data: {0.5, 2.0, 0.8, 0.3}
 * Times: {100, 200, 300, 400}
 * 
 * With threshold=1.0, lockout=0:
 *   - Interval [200-200]: isolated sample
 */
inline std::shared_ptr<AnalogTimeSeries> single_sample_lockout() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 2.0f, 0.8f, 0.3f})
        .atTimes({100, 200, 300, 400})
        .build();
}

/**
 * @brief Multiple single samples above threshold
 * 
 * Data: {0.5, 2.0, 0.8, 1.5, 0.3, 1.8, 0.6}
 * Times: {100, 200, 300, 400, 500, 600, 700}
 * 
 * With threshold=1.0, lockout=0:
 *   - Three isolated single-sample intervals
 */
inline std::shared_ptr<AnalogTimeSeries> multiple_single_samples() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 2.0f, 0.8f, 1.5f, 0.3f, 1.8f, 0.6f})
        .atTimes({100, 200, 300, 400, 500, 600, 700})
        .build();
}

/**
 * @brief Signal for operation interface tests
 * 
 * Data: {0.5, 1.5, 0.8, 1.8}
 * Times: {100, 200, 300, 400}
 */
inline std::shared_ptr<AnalogTimeSeries> operation_interface() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 0.8f, 1.8f})
        .atTimes({100, 200, 300, 400})
        .build();
}

/**
 * @brief Signal for testing different threshold directions
 * 
 * Data: {0.5, -1.5, 0.8, 1.8}
 * Times: {100, 200, 300, 400}
 * 
 * Tests positive, negative, and absolute thresholds
 */
inline std::shared_ptr<AnalogTimeSeries> operation_different_directions() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, -1.5f, 0.8f, 1.8f})
        .atTimes({100, 200, 300, 400})
        .build();
}

/**
 * @brief Signal with gaps for missing data testing (positive threshold)
 * 
 * Data: {0.5, 1.5, 1.8, 0.5, 1.2}
 * Times: {100, 101, 102, 152, 153}
 * 
 * Gap between t=102 and t=152 (50 time units)
 * With TREAT_AS_ZERO mode, gap breaks intervals
 * With IGNORE mode, gap is skipped
 */
inline std::shared_ptr<AnalogTimeSeries> missing_data_positive() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 1.8f, 0.5f, 1.2f})
        .atTimes({100, 101, 102, 152, 153})
        .build();
}

/**
 * @brief Signal with gaps for missing data testing (negative threshold)
 * 
 * Data: {0.5, -1.5, 0.5, -1.2}
 * Times: {100, 101, 151, 152}
 * 
 * Gap between t=101 and t=151 (50 time units)
 */
inline std::shared_ptr<AnalogTimeSeries> missing_data_negative() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, -1.5f, 0.5f, -1.2f})
        .atTimes({100, 101, 151, 152})
        .build();
}

/**
 * @brief Signal with gaps for missing data ignore mode testing
 * 
 * Data: {0.5, 1.5, 1.8, 0.5, 1.2}
 * Times: {100, 101, 102, 152, 153}
 * 
 * Same as missing_data_positive but for IGNORE mode testing
 */
inline std::shared_ptr<AnalogTimeSeries> missing_data_ignore() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 1.8f, 0.5f, 1.2f})
        .atTimes({100, 101, 102, 152, 153})
        .build();
}

/**
 * @brief Signal with no gaps in data
 * 
 * Data: {0.5, 1.5, 1.8, 0.5, 1.2}
 * Times: {100, 101, 102, 103, 104}
 * 
 * Continuous data with no gaps for comparison
 */
inline std::shared_ptr<AnalogTimeSeries> no_gaps() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 1.8f, 0.5f, 1.2f})
        .atTimes({100, 101, 102, 103, 104})
        .build();
}

/**
 * @brief Standard test signal used across multiple tests
 * 
 * Data: {0.5, 1.5, 2.0, 1.8, 0.8, 2.5, 1.2, 0.3}
 * Times: {100, 200, 300, 400, 500, 600, 700, 800}
 * 
 * Used for JSON pipeline tests and basic validation
 */
inline std::shared_ptr<AnalogTimeSeries> test_signal() {
    return AnalogTimeSeriesBuilder()
        .withValues({0.5f, 1.5f, 2.0f, 1.8f, 0.8f, 2.5f, 1.2f, 0.3f})
        .atTimes({100, 200, 300, 400, 500, 600, 700, 800})
        .build();
}

/**
 * @brief Empty signal for null/edge case tests
 * 
 * Data: {}
 * Times: {}
 */
inline std::shared_ptr<AnalogTimeSeries> empty_signal() {
    return AnalogTimeSeriesBuilder()
        .withValues({})
        .atTimes({})
        .build();
}

} // namespace analog_scenarios

#endif // INTERVAL_THRESHOLD_SCENARIOS_HPP
