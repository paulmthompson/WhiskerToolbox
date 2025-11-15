#include "analog_interval_peak.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "transforms/data_transforms.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <memory>
#include <vector>

TEST_CASE("Data Transform: Analog Interval Peak - Maximum Within Intervals", "[transforms][analog_interval_peak]") {
    IntervalPeakParams params;
    AnalogIntervalPeakOperation operation;

    SECTION("Basic maximum detection within intervals") {
        // Analog signal: index 0->5, values increase then decrease
        std::vector<float> values = {1.0f, 2.0f, 5.0f, 3.0f, 1.0f, 0.5f};
        std::vector<TimeFrameIndex> times;
        for (int i = 0; i < 6; ++i) {
            times.push_back(TimeFrameIndex(i * 100));
        }
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);

        // Intervals: [0, 200] and [300, 500]
        std::vector<Interval> intervals = {{0, 200}, {300, 500}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->getEventSeries();
        REQUIRE(events.size() == 2);

        // First interval [0, 200] -> max at index 2 (value 5.0)
        REQUIRE(events[0] == TimeFrameIndex(200.0));
        // Second interval [300, 500] -> max at index 3 (value 3.0)
        REQUIRE(events[1] == TimeFrameIndex(300.0));
    }

    SECTION("Maximum detection with progress callback") {
        std::vector<float> values = {1.0f, 5.0f, 2.0f, 8.0f, 3.0f};
        std::vector<TimeFrameIndex> times;
        for (int i = 0; i < 5; ++i) {
            times.push_back(TimeFrameIndex(i * 10));
        }
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);

        std::vector<Interval> intervals = {{0, 20}, {30, 40}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        int progress_val = -1;
        ProgressCallback cb = [&](int p) { progress_val = p; };

        auto result = find_interval_peaks(ats.get(), params, cb);
        REQUIRE(result != nullptr);
        REQUIRE(progress_val == 100);

        auto const & events = result->getEventSeries();
        REQUIRE(events.size() == 2);
        REQUIRE(events[0] == TimeFrameIndex(10.0)); // max in [0,20] is at index 1
        REQUIRE(events[1] == TimeFrameIndex(30.0)); // max in [30,40] is at index 3
    }

    SECTION("Multiple intervals with varying peak locations") {
        std::vector<float> values = {1.0f, 9.0f, 3.0f, 2.0f, 8.0f, 1.0f, 5.0f, 10.0f, 2.0f};
        std::vector<TimeFrameIndex> times;
        for (int i = 0; i < 9; ++i) {
            times.push_back(TimeFrameIndex(i * 10));
        }
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);

        std::vector<Interval> intervals = {{0, 20}, {30, 50}, {60, 80}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->getEventSeries();
        REQUIRE(events.size() == 3);
        REQUIRE(events[0] == TimeFrameIndex(10.0)); // max in [0,20] is 9.0 at index 1
        REQUIRE(events[1] == TimeFrameIndex(40.0)); // max in [30,50] is 8.0 at index 4
        REQUIRE(events[2] == TimeFrameIndex(70.0)); // max in [60,80] is 10.0 at index 7
    }
}

TEST_CASE("Data Transform: Analog Interval Peak - Minimum Within Intervals", "[transforms][analog_interval_peak]") {
    IntervalPeakParams params;

    SECTION("Basic minimum detection within intervals") {
        std::vector<float> values = {5.0f, 3.0f, 1.0f, 4.0f, 2.0f, 3.0f};
        std::vector<TimeFrameIndex> times;
        for (int i = 0; i < 6; ++i) {
            times.push_back(TimeFrameIndex(i * 100));
        }
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);

        std::vector<Interval> intervals = {{0, 200}, {300, 500}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.peak_type = IntervalPeakParams::PeakType::MINIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->getEventSeries();
        REQUIRE(events.size() == 2);
        REQUIRE(events[0] == TimeFrameIndex(200.0)); // min in [0,200] is 1.0 at index 2
        REQUIRE(events[1] == TimeFrameIndex(400.0)); // min in [300,500] is 2.0 at index 4
    }

    SECTION("Minimum with negative values") {
        std::vector<float> values = {1.0f, -5.0f, 2.0f, -3.0f, 0.5f};
        std::vector<TimeFrameIndex> times;
        for (int i = 0; i < 5; ++i) {
            times.push_back(TimeFrameIndex(i * 10));
        }
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);

        std::vector<Interval> intervals = {{0, 20}, {20, 40}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.peak_type = IntervalPeakParams::PeakType::MINIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->getEventSeries();
        REQUIRE(events.size() == 2);
        REQUIRE(events[0] == TimeFrameIndex(10.0)); // min in [0,20] is -5.0 at index 1
        REQUIRE(events[1] == TimeFrameIndex(30.0)); // min in [20,40] is -3.0 at index 3
    }
}

TEST_CASE("Data Transform: Analog Interval Peak - Between Interval Starts", "[transforms][analog_interval_peak]") {
    IntervalPeakParams params;

    SECTION("Maximum between interval starts") {
        // Values: steady increase
        std::vector<float> values = {1.0f, 2.0f, 5.0f, 8.0f, 10.0f, 7.0f, 3.0f};
        std::vector<TimeFrameIndex> times;
        for (int i = 0; i < 7; ++i) {
            times.push_back(TimeFrameIndex(i * 10));
        }
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);

        // Intervals start at 0, 20, 40
        std::vector<Interval> intervals = {{0, 10}, {20, 30}, {40, 50}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::BETWEEN_INTERVAL_STARTS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->getEventSeries();
        REQUIRE(events.size() == 3);

        // Between start 0 and start 20 (indices 0-1): max is 2.0 at index 1
        REQUIRE(events[0] == TimeFrameIndex(10.0));
        // Between start 20 and start 40 (indices 2-3): max is 8.0 at index 3
        REQUIRE(events[1] == TimeFrameIndex(30.0));
        // Last interval: from start 40 to end 50 (indices 4-5): max is 10.0 at index 4
        REQUIRE(events[2] == TimeFrameIndex(40.0));
    }

    SECTION("Minimum between interval starts") {
        std::vector<float> values = {5.0f, 2.0f, 8.0f, 3.0f, 9.0f, 1.0f};
        std::vector<TimeFrameIndex> times;
        for (int i = 0; i < 6; ++i) {
            times.push_back(TimeFrameIndex(i * 100));
        }
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);

        std::vector<Interval> intervals = {{0, 100}, {200, 300}, {400, 500}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.peak_type = IntervalPeakParams::PeakType::MINIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::BETWEEN_INTERVAL_STARTS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->getEventSeries();
        REQUIRE(events.size() == 3);

        // Between 0 and 200: min is 2.0 at index 1
        REQUIRE(events[0] == TimeFrameIndex(100.0));
        // Between 200 and 400: min is 3.0 at index 3
        REQUIRE(events[1] == TimeFrameIndex(300.0));
        // Last from 400 to 500: min is 1.0 at index 5
        REQUIRE(events[2] == TimeFrameIndex(500.0));
    }
}

TEST_CASE("Data Transform: Analog Interval Peak - Edge Cases", "[transforms][analog_interval_peak]") {
    IntervalPeakParams params;

    SECTION("Empty intervals - no events") {
        std::vector<float> values = {1.0f, 2.0f, 3.0f};
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(0), TimeFrameIndex(10), TimeFrameIndex(20)};
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);

        std::vector<Interval> intervals = {};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->getEventSeries().empty());
    }

    SECTION("Null analog time series") {
        std::vector<Interval> intervals = {{0, 10}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(nullptr, params);
        REQUIRE(result != nullptr);
        REQUIRE(result->getEventSeries().empty());
    }

    SECTION("Null interval series") {
        std::vector<float> values = {1.0f, 2.0f, 3.0f};
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(0), TimeFrameIndex(10), TimeFrameIndex(20)};
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = nullptr;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->getEventSeries().empty());
    }

    SECTION("Interval with no corresponding analog data") {
        std::vector<float> values = {1.0f, 2.0f, 3.0f};
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(0), TimeFrameIndex(10), TimeFrameIndex(20)};
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);

        // Interval outside the range of analog data
        std::vector<Interval> intervals = {{100, 200}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);
        // No data in the interval, so no events
        REQUIRE(result->getEventSeries().empty());
    }

    SECTION("Single data point interval") {
        std::vector<float> values = {1.0f, 5.0f, 2.0f};
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(0), TimeFrameIndex(10), TimeFrameIndex(20)};
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);

        std::vector<Interval> intervals = {{10, 10}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->getEventSeries();
        REQUIRE(events.size() == 1);
        REQUIRE(events[0] == TimeFrameIndex(10.0));
    }

    SECTION("Multiple intervals, some without data") {
        std::vector<float> values = {1.0f, 5.0f, 8.0f};
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(0), TimeFrameIndex(10), TimeFrameIndex(20)};
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);

        // First interval has data, second doesn't, third does
        std::vector<Interval> intervals = {{0, 10}, {50, 60}, {10, 20}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->getEventSeries();
        // Only intervals with data should produce events
        REQUIRE(events.size() == 2);
        REQUIRE(events[0] == TimeFrameIndex(10.0)); // max in [0,10] is 5.0 at index 10
        REQUIRE(events[1] == TimeFrameIndex(20.0)); // max in [10,20] is 8.0 at index 20
    }
}

TEST_CASE("Data Transform: Analog Interval Peak - TimeFrame Conversion", "[transforms][analog_interval_peak]") {
    IntervalPeakParams params;

    SECTION("Different timeframes - conversion required") {
        // Create analog time series with one timeframe
        auto timeValues = std::vector<int>();
        timeValues.push_back(0);
        timeValues.push_back(10);
        timeValues.push_back(20);
        timeValues.push_back(30);
        timeValues.push_back(40);
        auto analog_tf = std::make_shared<TimeFrame>(timeValues);

        std::vector<float> values = {1.0f, 5.0f, 2.0f, 8.0f, 3.0f};
        std::vector<TimeFrameIndex> times;
        for (int i = 0; i < 5; ++i) {
            times.push_back(TimeFrameIndex(i));
        }
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        ats->setTimeFrame(analog_tf);

        // Create interval series with a different timeframe
        auto interval_tf_values = std::vector<int>();
        interval_tf_values.push_back(0);
        interval_tf_values.push_back(5);
        interval_tf_values.push_back(15);
        interval_tf_values.push_back(25);
        interval_tf_values.push_back(35);
        auto interval_tf = std::make_shared<TimeFrame>(interval_tf_values);

        // Interval in interval_tf coordinates: [1, 3] means timestamps [5.0, 25.0]
        // In analog_tf coordinates, timestamps [5.0, 25.0] map to indices [0-1, 2]
        std::vector<Interval> intervals = {{1, 3}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);
        dis->setTimeFrame(interval_tf);

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->getEventSeries();
        REQUIRE(events.size() == 1);
        
        // The interval [5.0, 25.0] in timestamps corresponds to analog indices [1, 2]
        // Values at those indices: 5.0 and 2.0, max is 5.0 at index 1
        REQUIRE(events[0] == TimeFrameIndex(1.0));
    }

    SECTION("Same timeframe - no conversion needed") {

        auto shared_tf_values = std::vector<int>();
        shared_tf_values.push_back(0);
        shared_tf_values.push_back(10);
        shared_tf_values.push_back(20);
        shared_tf_values.push_back(30);
        auto shared_tf = std::make_shared<TimeFrame>(shared_tf_values);

        std::vector<float> values = {1.0f, 9.0f, 3.0f, 5.0f};
        std::vector<TimeFrameIndex> times;
        for (int i = 0; i < 4; ++i) {
            times.push_back(TimeFrameIndex(i));
        }
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);
        ats->setTimeFrame(shared_tf);

        std::vector<Interval> intervals = {{0, 2}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);
        dis->setTimeFrame(shared_tf);

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->getEventSeries();
        REQUIRE(events.size() == 1);
        REQUIRE(events[0] == TimeFrameIndex(1.0)); // max in [0,2] is 9.0 at index 1
    }
}

TEST_CASE("Data Transform: Analog Interval Peak - Operation Interface", "[transforms][analog_interval_peak]") {
    AnalogIntervalPeakOperation operation;

    SECTION("getName returns correct name") {
        REQUIRE(operation.getName() == "Interval Peak Detection");
    }

    SECTION("getTargetInputTypeIndex returns AnalogTimeSeries") {
        REQUIRE(operation.getTargetInputTypeIndex() == typeid(std::shared_ptr<AnalogTimeSeries>));
    }

    SECTION("canApply returns true for valid AnalogTimeSeries") {
        auto ats = std::make_shared<AnalogTimeSeries>();
        DataTypeVariant variant = ats;
        REQUIRE(operation.canApply(variant));
    }

    SECTION("canApply returns false for null pointer") {
        std::shared_ptr<AnalogTimeSeries> null_ptr = nullptr;
        DataTypeVariant variant = null_ptr;
        REQUIRE_FALSE(operation.canApply(variant));
    }

    SECTION("canApply returns false for wrong type") {
        auto dis = std::make_shared<DigitalEventSeries>();
        DataTypeVariant variant = dis;
        REQUIRE_FALSE(operation.canApply(variant));
    }

    SECTION("getDefaultParameters returns IntervalPeakParams") {
        auto params = operation.getDefaultParameters();
        REQUIRE(params != nullptr);
        auto * typed_params = dynamic_cast<IntervalPeakParams *>(params.get());
        REQUIRE(typed_params != nullptr);
        REQUIRE(typed_params->peak_type == IntervalPeakParams::PeakType::MAXIMUM);
        REQUIRE(typed_params->search_mode == IntervalPeakParams::SearchMode::WITHIN_INTERVALS);
    }

    SECTION("execute with valid input") {
        std::vector<float> values = {1.0f, 5.0f, 2.0f, 8.0f, 3.0f};
        std::vector<TimeFrameIndex> times;
        for (int i = 0; i < 5; ++i) {
            times.push_back(TimeFrameIndex(i * 10));
        }
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);

        std::vector<Interval> intervals = {{0, 20}, {30, 40}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        IntervalPeakParams params;
        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        DataTypeVariant input_variant = ats;
        DataTypeVariant result_variant = operation.execute(input_variant, &params);

        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalEventSeries>>(result_variant));
        auto result = std::get<std::shared_ptr<DigitalEventSeries>>(result_variant);
        REQUIRE(result != nullptr);

        auto const & events = result->getEventSeries();
        REQUIRE(events.size() == 2);
        REQUIRE(events[0] == TimeFrameIndex(10.0));
        REQUIRE(events[1] == TimeFrameIndex(30.0));
    }

    SECTION("execute with null parameters uses defaults") {
        std::vector<float> values = {1.0f, 5.0f, 2.0f};
        std::vector<TimeFrameIndex> times = {TimeFrameIndex(0), TimeFrameIndex(10), TimeFrameIndex(20)};
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);

        DataTypeVariant input_variant = ats;
        DataTypeVariant result_variant = operation.execute(input_variant, nullptr);

        // Should return empty because default params have null interval_series
        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalEventSeries>>(result_variant));
        auto result = std::get<std::shared_ptr<DigitalEventSeries>>(result_variant);
        REQUIRE(result != nullptr);
        REQUIRE(result->getEventSeries().empty());
    }

    SECTION("execute with progress callback") {
        std::vector<float> values = {1.0f, 5.0f, 2.0f, 8.0f, 3.0f};
        std::vector<TimeFrameIndex> times;
        for (int i = 0; i < 5; ++i) {
            times.push_back(TimeFrameIndex(i * 10));
        }
        auto ats = std::make_shared<AnalogTimeSeries>(values, times);

        std::vector<Interval> intervals = {{0, 20}};
        auto dis = std::make_shared<DigitalIntervalSeries>(intervals);

        IntervalPeakParams params;
        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        int progress_val = -1;
        ProgressCallback cb = [&](int p) { progress_val = p; };

        DataTypeVariant input_variant = ats;
        DataTypeVariant result_variant = operation.execute(input_variant, &params, cb);

        REQUIRE(progress_val == 100);
        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalEventSeries>>(result_variant));
    }
}
