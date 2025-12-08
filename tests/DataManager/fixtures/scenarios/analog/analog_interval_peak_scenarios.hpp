#ifndef ANALOG_INTERVAL_PEAK_SCENARIOS_HPP
#define ANALOG_INTERVAL_PEAK_SCENARIOS_HPP

#include "fixtures/builders/AnalogTimeSeriesBuilder.hpp"
#include "fixtures/builders/DigitalTimeSeriesBuilder.hpp"
#include "fixtures/builders/TimeFrameBuilder.hpp"
#include <memory>
#include <utility>

/**
 * @brief Analog interval peak detection test scenarios
 * 
 * This namespace contains pre-configured test data for analog interval peak
 * detection algorithms. These scenarios cover maximum/minimum detection within
 * intervals, between interval starts, edge cases, and time frame conversion.
 */
namespace analog_interval_peak_scenarios {

/**
 * @brief Basic maximum detection within intervals
 * 
 * Signal: {1.0, 2.0, 5.0, 3.0, 1.0, 0.5}
 * Times: {0, 100, 200, 300, 400, 500}
 * Intervals: [[0, 200], [300, 500]]
 * 
 * Expected maximum peaks:
 *   - First interval [0, 200] -> max 5.0 at t=200
 *   - Second interval [300, 500] -> max 3.0 at t=300
 */
inline std::pair<std::shared_ptr<AnalogTimeSeries>, std::shared_ptr<DigitalIntervalSeries>> 
basic_max_within() {
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({1.0f, 2.0f, 5.0f, 3.0f, 1.0f, 0.5f})
        .atTimes({0, 100, 200, 300, 400, 500})
        .build();
    
    auto intervals = DigitalIntervalSeriesBuilder()
        .withInterval(0, 200)
        .withInterval(300, 500)
        .build();
    
    return {signal, intervals};
}

/**
 * @brief Maximum detection with progress callback
 * 
 * Signal: {1.0, 5.0, 2.0, 8.0, 3.0}
 * Times: {0, 10, 20, 30, 40}
 * Intervals: [[0, 20], [30, 40]]
 * 
 * Expected maximum peaks:
 *   - First interval [0, 20] -> max 5.0 at t=10
 *   - Second interval [30, 40] -> max 8.0 at t=30
 */
inline std::pair<std::shared_ptr<AnalogTimeSeries>, std::shared_ptr<DigitalIntervalSeries>> 
max_with_progress() {
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({1.0f, 5.0f, 2.0f, 8.0f, 3.0f})
        .atTimes({0, 10, 20, 30, 40})
        .build();
    
    auto intervals = DigitalIntervalSeriesBuilder()
        .withInterval(0, 20)
        .withInterval(30, 40)
        .build();
    
    return {signal, intervals};
}

/**
 * @brief Multiple intervals with varying peak locations
 * 
 * Signal: {1.0, 9.0, 3.0, 2.0, 8.0, 1.0, 5.0, 10.0, 2.0}
 * Times: {0, 10, 20, 30, 40, 50, 60, 70, 80}
 * Intervals: [[0, 20], [30, 50], [60, 80]]
 * 
 * Expected maximum peaks:
 *   - First interval [0, 20] -> max 9.0 at t=10
 *   - Second interval [30, 50] -> max 8.0 at t=40
 *   - Third interval [60, 80] -> max 10.0 at t=70
 */
inline std::pair<std::shared_ptr<AnalogTimeSeries>, std::shared_ptr<DigitalIntervalSeries>> 
multiple_intervals_varying() {
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({1.0f, 9.0f, 3.0f, 2.0f, 8.0f, 1.0f, 5.0f, 10.0f, 2.0f})
        .atTimes({0, 10, 20, 30, 40, 50, 60, 70, 80})
        .build();
    
    auto intervals = DigitalIntervalSeriesBuilder()
        .withInterval(0, 20)
        .withInterval(30, 50)
        .withInterval(60, 80)
        .build();
    
    return {signal, intervals};
}

/**
 * @brief Basic minimum detection within intervals
 * 
 * Signal: {5.0, 3.0, 1.0, 4.0, 2.0, 3.0}
 * Times: {0, 100, 200, 300, 400, 500}
 * Intervals: [[0, 200], [300, 500]]
 * 
 * Expected minimum peaks:
 *   - First interval [0, 200] -> min 1.0 at t=200
 *   - Second interval [300, 500] -> min 2.0 at t=400
 */
inline std::pair<std::shared_ptr<AnalogTimeSeries>, std::shared_ptr<DigitalIntervalSeries>> 
basic_min_within() {
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({5.0f, 3.0f, 1.0f, 4.0f, 2.0f, 3.0f})
        .atTimes({0, 100, 200, 300, 400, 500})
        .build();
    
    auto intervals = DigitalIntervalSeriesBuilder()
        .withInterval(0, 200)
        .withInterval(300, 500)
        .build();
    
    return {signal, intervals};
}

/**
 * @brief Minimum with negative values
 * 
 * Signal: {1.0, -5.0, 2.0, -3.0, 0.5}
 * Times: {0, 10, 20, 30, 40}
 * Intervals: [[0, 20], [20, 40]]
 * 
 * Expected minimum peaks:
 *   - First interval [0, 20] -> min -5.0 at t=10
 *   - Second interval [20, 40] -> min -3.0 at t=30
 */
inline std::pair<std::shared_ptr<AnalogTimeSeries>, std::shared_ptr<DigitalIntervalSeries>> 
min_with_negative() {
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({1.0f, -5.0f, 2.0f, -3.0f, 0.5f})
        .atTimes({0, 10, 20, 30, 40})
        .build();
    
    auto intervals = DigitalIntervalSeriesBuilder()
        .withInterval(0, 20)
        .withInterval(20, 40)
        .build();
    
    return {signal, intervals};
}

/**
 * @brief Maximum between interval starts
 * 
 * Signal: {1.0, 2.0, 5.0, 8.0, 10.0, 7.0, 3.0}
 * Times: {0, 10, 20, 30, 40, 50, 60}
 * Intervals: [[0, 10], [20, 30], [40, 50]]
 * 
 * Expected maximum peaks (between starts mode):
 *   - Between start 0 and start 20 -> max 2.0 at t=10
 *   - Between start 20 and start 40 -> max 8.0 at t=30
 *   - Last interval from start 40 to end 50 -> max 10.0 at t=40
 */
inline std::pair<std::shared_ptr<AnalogTimeSeries>, std::shared_ptr<DigitalIntervalSeries>> 
max_between_starts() {
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({1.0f, 2.0f, 5.0f, 8.0f, 10.0f, 7.0f, 3.0f})
        .atTimes({0, 10, 20, 30, 40, 50, 60})
        .build();
    
    auto intervals = DigitalIntervalSeriesBuilder()
        .withInterval(0, 10)
        .withInterval(20, 30)
        .withInterval(40, 50)
        .build();
    
    return {signal, intervals};
}

/**
 * @brief Minimum between interval starts
 * 
 * Signal: {5.0, 2.0, 8.0, 3.0, 9.0, 1.0}
 * Times: {0, 100, 200, 300, 400, 500}
 * Intervals: [[0, 100], [200, 300], [400, 500]]
 * 
 * Expected minimum peaks (between starts mode):
 *   - Between 0 and 200 -> min 2.0 at t=100
 *   - Between 200 and 400 -> min 3.0 at t=300
 *   - Last from 400 to 500 -> min 1.0 at t=500
 */
inline std::pair<std::shared_ptr<AnalogTimeSeries>, std::shared_ptr<DigitalIntervalSeries>> 
min_between_starts() {
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({5.0f, 2.0f, 8.0f, 3.0f, 9.0f, 1.0f})
        .atTimes({0, 100, 200, 300, 400, 500})
        .build();
    
    auto intervals = DigitalIntervalSeriesBuilder()
        .withInterval(0, 100)
        .withInterval(200, 300)
        .withInterval(400, 500)
        .build();
    
    return {signal, intervals};
}

/**
 * @brief Empty intervals - no events
 * 
 * Signal: {1.0, 2.0, 3.0}
 * Times: {0, 10, 20}
 * Intervals: []
 * 
 * Expected: No events (empty interval series)
 */
inline std::pair<std::shared_ptr<AnalogTimeSeries>, std::shared_ptr<DigitalIntervalSeries>> 
empty_intervals() {
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({1.0f, 2.0f, 3.0f})
        .atTimes({0, 10, 20})
        .build();
    
    auto intervals = DigitalIntervalSeriesBuilder()
        .build();
    
    return {signal, intervals};
}

/**
 * @brief Interval with no corresponding analog data
 * 
 * Signal: {1.0, 2.0, 3.0}
 * Times: {0, 10, 20}
 * Intervals: [[100, 200]]
 * 
 * Expected: No events (interval outside signal range)
 */
inline std::pair<std::shared_ptr<AnalogTimeSeries>, std::shared_ptr<DigitalIntervalSeries>> 
no_data_interval() {
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({1.0f, 2.0f, 3.0f})
        .atTimes({0, 10, 20})
        .build();
    
    auto intervals = DigitalIntervalSeriesBuilder()
        .withInterval(100, 200)
        .build();
    
    return {signal, intervals};
}

/**
 * @brief Single data point interval
 * 
 * Signal: {1.0, 5.0, 2.0}
 * Times: {0, 10, 20}
 * Intervals: [[10, 10]]
 * 
 * Expected: Event at t=10 (single point has value 5.0)
 */
inline std::pair<std::shared_ptr<AnalogTimeSeries>, std::shared_ptr<DigitalIntervalSeries>> 
single_point() {
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({1.0f, 5.0f, 2.0f})
        .atTimes({0, 10, 20})
        .build();
    
    auto intervals = DigitalIntervalSeriesBuilder()
        .withInterval(10, 10)
        .build();
    
    return {signal, intervals};
}

/**
 * @brief Multiple intervals, some without data
 * 
 * Signal: {1.0, 5.0, 8.0}
 * Times: {0, 10, 20}
 * Intervals: [[0, 10], [50, 60], [10, 20]]
 * 
 * Expected: Events only for intervals with data
 *   - Interval [0, 10] -> max 5.0 at t=10
 *   - Interval [50, 60] -> no event (no data)
 *   - Interval [10, 20] -> max 8.0 at t=20
 */
inline std::pair<std::shared_ptr<AnalogTimeSeries>, std::shared_ptr<DigitalIntervalSeries>> 
mixed_data_availability() {
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({1.0f, 5.0f, 8.0f})
        .atTimes({0, 10, 20})
        .build();
    
    auto intervals = DigitalIntervalSeriesBuilder()
        .withInterval(0, 10)
        .withInterval(50, 60)
        .withInterval(10, 20)
        .build();
    
    return {signal, intervals};
}

/**
 * @brief Different timeframes - conversion required
 * 
 * Signal: {1.0, 5.0, 2.0, 8.0, 3.0}
 * Signal times: {0, 1, 2, 3, 4} (indices)
 * Signal timeframe: {0, 10, 20, 30, 40} (timestamps)
 * Intervals: [[1, 3]] (in interval timeframe indices)
 * Interval timeframe: {0, 5, 15, 25, 35} (timestamps)
 * 
 * Expected: Requires timeframe conversion between signal and intervals
 */
inline std::tuple<std::shared_ptr<AnalogTimeSeries>, std::shared_ptr<DigitalIntervalSeries>,
                  std::shared_ptr<TimeFrame>, std::shared_ptr<TimeFrame>>
different_timeframes() {
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({1.0f, 5.0f, 2.0f, 8.0f, 3.0f})
        .atTimes({0, 1, 2, 3, 4})
        .build();
    
    auto signal_timeframe = TimeFrameBuilder()
        .withRange(0, 40, 10)
        .build();
    signal->setTimeFrame(signal_timeframe);
    
    auto intervals = DigitalIntervalSeriesBuilder()
        .withInterval(1, 3)
        .build();
    
    auto interval_timeframe = TimeFrameBuilder()
        .withTimes({0, 5, 15, 25, 35})
        .build();
    intervals->setTimeFrame(interval_timeframe);
    
    return {signal, intervals, signal_timeframe, interval_timeframe};
}

/**
 * @brief Same timeframe - no conversion needed
 * 
 * Signal: {1.0, 9.0, 3.0, 5.0}
 * Signal times: {0, 1, 2, 3} (indices)
 * Signal timeframe: {0, 10, 20, 30} (timestamps)
 * Intervals: [[0, 2]]
 * Interval timeframe: {0, 10, 20, 30} (same as signal)
 * 
 * Expected maximum peak:
 *   - Interval [0, 2] -> max 9.0 at t=1
 */
inline std::tuple<std::shared_ptr<AnalogTimeSeries>, std::shared_ptr<DigitalIntervalSeries>,
                  std::shared_ptr<TimeFrame>>
same_timeframe() {
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({1.0f, 9.0f, 3.0f, 5.0f})
        .atTimes({0, 1, 2, 3})
        .build();
    
    auto timeframe = TimeFrameBuilder()
        .withTimes({0, 10, 20, 30})
        .build();
    signal->setTimeFrame(timeframe);
    
    auto intervals = DigitalIntervalSeriesBuilder()
        .withInterval(0, 2)
        .build();
    intervals->setTimeFrame(timeframe);
    
    return {signal, intervals, timeframe};
}

/**
 * @brief Simple signal for operation interface tests
 * 
 * Signal: {1.0, 5.0, 2.0, 8.0, 3.0}
 * Times: {0, 10, 20, 30, 40}
 * Intervals: [[0, 20], [30, 40]]
 * 
 * Expected maximum peaks:
 *   - First interval [0, 20] -> max 5.0 at t=10
 *   - Second interval [30, 40] -> max 8.0 at t=30
 */
inline std::pair<std::shared_ptr<AnalogTimeSeries>, std::shared_ptr<DigitalIntervalSeries>> 
operation_interface() {
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({1.0f, 5.0f, 2.0f, 8.0f, 3.0f})
        .atTimes({0, 10, 20, 30, 40})
        .build();
    
    auto intervals = DigitalIntervalSeriesBuilder()
        .withInterval(0, 20)
        .withInterval(30, 40)
        .build();
    
    return {signal, intervals};
}

/**
 * @brief Simple signal for progress callback tests
 * 
 * Signal: {1.0, 5.0, 2.0, 8.0, 3.0}
 * Times: {0, 10, 20, 30, 40}
 * Intervals: [[0, 20]]
 * 
 * Expected maximum peak:
 *   - Interval [0, 20] -> max 5.0 at t=10
 */
inline std::pair<std::shared_ptr<AnalogTimeSeries>, std::shared_ptr<DigitalIntervalSeries>> 
operation_progress() {
    auto signal = AnalogTimeSeriesBuilder()
        .withValues({1.0f, 5.0f, 2.0f, 8.0f, 3.0f})
        .atTimes({0, 10, 20, 30, 40})
        .build();
    
    auto intervals = DigitalIntervalSeriesBuilder()
        .withInterval(0, 20)
        .build();
    
    return {signal, intervals};
}

/**
 * @brief Simple signal for basic tests
 * 
 * Signal: {1.0, 2.0, 3.0}
 * Times: {0, 10, 20}
 */
inline std::shared_ptr<AnalogTimeSeries> simple_signal() {
    return AnalogTimeSeriesBuilder()
        .withValues({1.0f, 2.0f, 3.0f})
        .atTimes({0, 10, 20})
        .build();
}

} // namespace analog_interval_peak_scenarios

#endif // ANALOG_INTERVAL_PEAK_SCENARIOS_HPP
