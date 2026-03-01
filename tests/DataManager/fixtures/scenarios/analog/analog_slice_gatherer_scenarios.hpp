#ifndef ANALOG_SLICE_GATHERER_SCENARIOS_HPP
#define ANALOG_SLICE_GATHERER_SCENARIOS_HPP

#include "fixtures/builders/AnalogTimeSeriesBuilder.hpp"
#include "fixtures/builders/DigitalTimeSeriesBuilder.hpp"
#include "fixtures/builders/TimeFrameBuilder.hpp"

#include "TimeFrame/StrongTimeTypes.hpp"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

/**
 * @brief Test scenarios for AnalogSliceGathererComputer
 *
 * Each scenario returns the data objects needed to exercise a particular
 * behaviour of the computer.  Scenarios are deliberately minimal – they
 * use the builder helpers directly and carry no DataManager dependency,
 * keeping setup noise out of the test assertions.
 */
namespace analog_slice_gatherer_scenarios {

// ---------------------------------------------------------------------------
// Basic slicing scenarios
// ---------------------------------------------------------------------------

/**
 * @brief Linear ramp signal with three non-overlapping intervals
 *
 * Signal: {0, 1, 2, 3, 4, 5, 6, 7, 8, 9} at times {0..9}
 * Intervals: [1,3], [5,7], [8,9]
 *
 * Expected slices (double):
 *   [0] -> {1, 2, 3}
 *   [1] -> {5, 6, 7}
 *   [2] -> {8, 9}
 */
inline auto linear_ramp_three_slices()
    -> std::tuple<std::shared_ptr<AnalogTimeSeries>,
                  std::shared_ptr<TimeFrame>,
                  std::vector<TimeFrameInterval>>
{
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f})
        .atTimes({0, 1, 2, 3, 4, 5, 6, 7, 8, 9})
        .build();

    auto tf = TimeFrameBuilder().withCount(10).build();

    std::vector<TimeFrameInterval> intervals = {
        TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(3)),
        TimeFrameInterval(TimeFrameIndex(5), TimeFrameIndex(7)),
        TimeFrameInterval(TimeFrameIndex(8), TimeFrameIndex(9)),
    };

    return {signal, tf, intervals};
}

/**
 * @brief Sine-wave signal with two equal-width intervals
 *
 * Signal: 6 sine samples at times {0..5}
 * Intervals: [0,2], [3,5]
 *
 * Expected slice sizes: 3, 3
 */
inline auto sine_wave_two_slices()
    -> std::tuple<std::shared_ptr<AnalogTimeSeries>,
                  std::shared_ptr<TimeFrame>,
                  std::vector<TimeFrameInterval>>
{
    std::vector<float> values;
    for (int i = 0; i < 6; ++i) {
        values.push_back(std::sin(static_cast<float>(i) * 0.5f));
    }

    auto signal = AnalogTimeSeriesBuilder()
        .withValues(values)
        .atTimes({0, 1, 2, 3, 4, 5})
        .build();

    auto tf = TimeFrameBuilder().withCount(6).build();

    std::vector<TimeFrameInterval> intervals = {
        TimeFrameInterval(TimeFrameIndex(0), TimeFrameIndex(2)),
        TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(5)),
    };

    return {signal, tf, intervals};
}

/**
 * @brief Linear ramp with three single-point intervals
 *
 * Signal: {0, 1, 2, 3, 4, 5} at times {0..5}
 * Intervals: [1,1], [3,3], [5,5]
 *
 * Expected slices: {1}, {3}, {5}
 */
inline auto single_point_intervals()
    -> std::tuple<std::shared_ptr<AnalogTimeSeries>,
                  std::shared_ptr<TimeFrame>,
                  std::vector<TimeFrameInterval>>
{
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f})
        .atTimes({0, 1, 2, 3, 4, 5})
        .build();

    auto tf = TimeFrameBuilder().withCount(6).build();

    std::vector<TimeFrameInterval> intervals = {
        TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(1)),
        TimeFrameInterval(TimeFrameIndex(3), TimeFrameIndex(3)),
        TimeFrameInterval(TimeFrameIndex(5), TimeFrameIndex(5)),
    };

    return {signal, tf, intervals};
}

// ---------------------------------------------------------------------------
// Template-type scenario
// ---------------------------------------------------------------------------

/**
 * @brief Fractional-value signal used for testing double vs float templates
 *
 * Signal: {1.5, 2.5, 3.5, 4.5, 5.5} at times {0..4}
 * Interval: [1, 3]
 *
 * Expected slice values: {2.5, 3.5, 4.5}
 */
inline auto fractional_ramp_single_slice()
    -> std::tuple<std::shared_ptr<AnalogTimeSeries>,
                  std::shared_ptr<TimeFrame>,
                  std::vector<TimeFrameInterval>>
{
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({1.5f, 2.5f, 3.5f, 4.5f, 5.5f})
        .atTimes({0, 1, 2, 3, 4})
        .build();

    auto tf = TimeFrameBuilder().withCount(5).build();

    std::vector<TimeFrameInterval> intervals = {
        TimeFrameInterval(TimeFrameIndex(1), TimeFrameIndex(3)),
    };

    return {signal, tf, intervals};
}

// ---------------------------------------------------------------------------
// Error / edge-case scenarios
// ---------------------------------------------------------------------------

/**
 * @brief Minimal signal for testing that an index-based ExecutionPlan throws
 *
 * Signal: {0, 1, 2, 3, 4, 5} at times {0..5}
 * Indices: {0, 1}  (not intervals – used to trigger the expected exception)
 */
inline auto simple_signal_for_error_test()
    -> std::pair<std::shared_ptr<AnalogTimeSeries>, std::shared_ptr<TimeFrame>>
{
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f})
        .atTimes({0, 1, 2, 3, 4, 5})
        .build();

    auto tf = TimeFrameBuilder().withCount(6).build();

    return {signal, tf};
}

/**
 * @brief Minimal signal for dependency-tracking tests
 *
 * Signal: {1.0, 2.0, 3.0} at times {0, 1, 2}
 */
inline auto minimal_signal()
    -> std::pair<std::shared_ptr<AnalogTimeSeries>, std::shared_ptr<TimeFrame>>
{
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({1.0f, 2.0f, 3.0f})
        .atTimes({0, 1, 2})
        .build();

    auto tf = TimeFrameBuilder().withCount(3).build();

    return {signal, tf};
}

// ---------------------------------------------------------------------------
// Cross-timeframe scenario (DataManager fixture level)
// ---------------------------------------------------------------------------

/**
 * @brief Two signals sharing the same base analog TimeFrame but with
 *        behaviour intervals recorded on a coarser TimeFrame
 *
 * Analog TimeFrame  : 0, 1, 2, …, 100  (101 points, step 1)
 * Behavior TimeFrame: 0, 2, 4, …, 100  ( 51 points, step 2)
 *
 * TriangularWave: rising 0→50 (indices 0-50), falling 50→0 (indices 50-100)
 * SineWave      : amplitude=25, frequency=0.1 Hz over 101 points
 *
 * BehaviorPeriods (in behavior_time indices):
 *   [5, 15]  -> analog time 10–30  (rising edge)
 *   [20, 30] -> analog time 40–60  (peak and start of fall)
 *   [35, 45] -> analog time 70–90  (falling edge)
 */
struct CrossTimeframeData {
    std::shared_ptr<AnalogTimeSeries>       triangular_wave;
    std::shared_ptr<AnalogTimeSeries>       sine_wave;
    std::shared_ptr<DigitalIntervalSeries>  behavior_periods;
    std::shared_ptr<TimeFrame>              analog_timeframe;
    std::shared_ptr<TimeFrame>              behavior_timeframe;
};

inline CrossTimeframeData cross_timeframe_analog_with_behavior()
{
    // Analog TimeFrame: 0..100, step 1
    auto analog_tf = TimeFrameBuilder().withRange(0, 100, 1).build();

    // Behavior TimeFrame: 0, 2, 4, …, 100
    auto behavior_tf = TimeFrameBuilder().withRange(0, 100, 2).build();

    // Triangular wave over 101 points (values: i for i<=50, 100-i otherwise)
    auto triangular = AnalogTimeSeriesBuilder()
        .withTriangleWave(0, 100, 50.0f)
        .build();
    triangular->setTimeFrame(analog_tf);

    // Sine wave over 101 points – amplitude 25, frequency 0.1
    auto sine = AnalogTimeSeriesBuilder()
        .withSineWave(0, 100, 0.1f, 25.0f)
        .build();
    sine->setTimeFrame(analog_tf);

    // Behavior intervals in behavior_time indices
    auto behavior = DigitalIntervalSeriesBuilder()
        .withInterval(5,  15)
        .withInterval(20, 30)
        .withInterval(35, 45)
        .build();
    behavior->setTimeFrame(behavior_tf);

    return {triangular, sine, behavior, analog_tf, behavior_tf};
}

} // namespace analog_slice_gatherer_scenarios

#endif // ANALOG_SLICE_GATHERER_SCENARIOS_HPP
