#include "analog_interval_peak.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "transforms/data_transforms.hpp"

#include "fixtures/scenarios/analog/analog_interval_peak_scenarios.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <memory>
#include <vector>

TEST_CASE("Data Transform: Analog Interval Peak - Maximum Within Intervals", "[transforms][analog_interval_peak]") {
    IntervalPeakParams params;
    AnalogIntervalPeakOperation operation;

    SECTION("Basic maximum detection within intervals") {
        auto [ats, dis] = analog_interval_peak_scenarios::basic_max_within();

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 2);

        // First interval [0, 200] -> max at index 2 (value 5.0)
        REQUIRE(events[0].time() == TimeFrameIndex(200.0));
        // Second interval [300, 500] -> max at index 3 (value 3.0)
        REQUIRE(events[1].time() == TimeFrameIndex(300.0));
    }

    SECTION("Maximum detection with progress callback") {
        auto [ats, dis] = analog_interval_peak_scenarios::max_with_progress();

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        int progress_val = -1;
        ProgressCallback cb = [&](int p) { progress_val = p; };

        auto result = find_interval_peaks(ats.get(), params, cb);
        REQUIRE(result != nullptr);
        REQUIRE(progress_val == 100);

        auto const & events = result->view();
        REQUIRE(result->size() == 2);
        REQUIRE(events[0].time() == TimeFrameIndex(10.0)); // max in [0,20] is at index 1
        REQUIRE(events[1].time() == TimeFrameIndex(30.0)); // max in [30,40] is at index 3
    }

    SECTION("Multiple intervals with varying peak locations") {
        auto [ats, dis] = analog_interval_peak_scenarios::multiple_intervals_varying();

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 3);
        REQUIRE(events[0].time() == TimeFrameIndex(10.0)); // max in [0,20] is 9.0 at index 1
        REQUIRE(events[1].time() == TimeFrameIndex(40.0)); // max in [30,50] is 8.0 at index 4
        REQUIRE(events[2].time() == TimeFrameIndex(70.0)); // max in [60,80] is 10.0 at index 7
    }
}

TEST_CASE("Data Transform: Analog Interval Peak - Minimum Within Intervals", "[transforms][analog_interval_peak]") {
    IntervalPeakParams params;

    SECTION("Basic minimum detection within intervals") {
        auto [ats, dis] = analog_interval_peak_scenarios::basic_min_within();

        params.peak_type = IntervalPeakParams::PeakType::MINIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 2);
        REQUIRE(events[0].time() == TimeFrameIndex(200.0)); // min in [0,200] is 1.0 at index 2
        REQUIRE(events[1].time() == TimeFrameIndex(400.0)); // min in [300,500] is 2.0 at index 4
    }

    SECTION("Minimum with negative values") {
        auto [ats, dis] = analog_interval_peak_scenarios::min_with_negative();

        params.peak_type = IntervalPeakParams::PeakType::MINIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 2);
        REQUIRE(events[0].time() == TimeFrameIndex(10.0)); // min in [0,20] is -5.0 at index 1
        REQUIRE(events[1].time() == TimeFrameIndex(30.0)); // min in [20,40] is -3.0 at index 3
    }
}

TEST_CASE("Data Transform: Analog Interval Peak - Between Interval Starts", "[transforms][analog_interval_peak]") {
    IntervalPeakParams params;

    SECTION("Maximum between interval starts") {
        auto [ats, dis] = analog_interval_peak_scenarios::max_between_starts();

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::BETWEEN_INTERVAL_STARTS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 3);

        // Between start 0 and start 20 (indices 0-1): max is 2.0 at index 1
        REQUIRE(events[0].time() == TimeFrameIndex(10.0));
        // Between start 20 and start 40 (indices 2-3): max is 8.0 at index 3
        REQUIRE(events[1].time() == TimeFrameIndex(30.0));
        // Last interval: from start 40 to end 50 (indices 4-5): max is 10.0 at index 4
        REQUIRE(events[2].time() == TimeFrameIndex(40.0));
    }

    SECTION("Minimum between interval starts") {
        auto [ats, dis] = analog_interval_peak_scenarios::min_between_starts();

        params.peak_type = IntervalPeakParams::PeakType::MINIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::BETWEEN_INTERVAL_STARTS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 3);

        // Between 0 and 200: min is 2.0 at index 1
        REQUIRE(events[0].time() == TimeFrameIndex(100.0));
        // Between 200 and 400: min is 3.0 at index 3
        REQUIRE(events[1].time() == TimeFrameIndex(300.0));
        // Last from 400 to 500: min is 1.0 at index 5
        REQUIRE(events[2].time() == TimeFrameIndex(500.0));
    }
}

TEST_CASE("Data Transform: Analog Interval Peak - Edge Cases", "[transforms][analog_interval_peak]") {
    IntervalPeakParams params;

    SECTION("Empty intervals - no events") {
        auto [ats, dis] = analog_interval_peak_scenarios::empty_intervals();

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->size() == 0);
    }

    SECTION("Null analog time series") {
        std::vector<Interval> intervals = {{0, 10}};
        auto dis_with_data = std::make_shared<DigitalIntervalSeries>(intervals);

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis_with_data;

        auto result = find_interval_peaks(nullptr, params);
        REQUIRE(result != nullptr);
        REQUIRE(result->size() == 0);
    }

    SECTION("Null interval series") {
        auto ats = analog_interval_peak_scenarios::simple_signal();

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = nullptr;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);
        REQUIRE(result->size() == 0);
    }

    SECTION("Interval with no corresponding analog data") {
        auto [ats, dis] = analog_interval_peak_scenarios::no_data_interval();

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);
        // No data in the interval, so no events
        REQUIRE(result->size() == 0);
    }

    SECTION("Single data point interval") {
        auto [ats, dis] = analog_interval_peak_scenarios::single_point();

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 1);
        REQUIRE(events[0].time() == TimeFrameIndex(10.0));
    }

    SECTION("Multiple intervals, some without data") {
        auto [ats, dis] = analog_interval_peak_scenarios::mixed_data_availability();

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->view();
        // Only intervals with data should produce events
        REQUIRE(result->size() == 2);
        REQUIRE(events[0].time() == TimeFrameIndex(10.0)); // max in [0,10] is 5.0 at index 10
        REQUIRE(events[1].time() == TimeFrameIndex(20.0)); // max in [10,20] is 8.0 at index 20
    }
}

TEST_CASE("Data Transform: Analog Interval Peak - TimeFrame Conversion", "[transforms][analog_interval_peak]") {
    IntervalPeakParams params;

    SECTION("Different timeframes - conversion required") {
        auto [ats, dis, signal_tf, interval_tf] = analog_interval_peak_scenarios::different_timeframes();

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 1);
        
        // The interval [5.0, 25.0] in timestamps corresponds to analog indices [1, 2]
        // Values at those indices: 5.0 and 2.0, max is 5.0 at index 1
        REQUIRE(events[0].time() == TimeFrameIndex(1.0));
    }

    SECTION("Same timeframe - no conversion needed") {
        auto [ats, dis, tf] = analog_interval_peak_scenarios::same_timeframe();

        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        auto result = find_interval_peaks(ats.get(), params);
        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 1);
        REQUIRE(events[0].time() == TimeFrameIndex(1.0)); // max in [0,2] is 9.0 at index 1
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
        auto [ats, dis] = analog_interval_peak_scenarios::operation_interface();

        IntervalPeakParams params;
        params.peak_type = IntervalPeakParams::PeakType::MAXIMUM;
        params.search_mode = IntervalPeakParams::SearchMode::WITHIN_INTERVALS;
        params.interval_series = dis;

        DataTypeVariant input_variant = ats;
        DataTypeVariant result_variant = operation.execute(input_variant, &params);

        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalEventSeries>>(result_variant));
        auto result = std::get<std::shared_ptr<DigitalEventSeries>>(result_variant);
        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 2);
        REQUIRE(events[0].time() == TimeFrameIndex(10.0));
        REQUIRE(events[1].time() == TimeFrameIndex(30.0));
    }

    SECTION("execute with null parameters uses defaults") {
        auto ats = analog_interval_peak_scenarios::simple_signal();

        DataTypeVariant input_variant = ats;
        DataTypeVariant result_variant = operation.execute(input_variant, nullptr);

        // Should return empty because default params have null interval_series
        REQUIRE(std::holds_alternative<std::shared_ptr<DigitalEventSeries>>(result_variant));
        auto result = std::get<std::shared_ptr<DigitalEventSeries>>(result_variant);
        REQUIRE(result != nullptr);
        REQUIRE(result->size() == 0);
    }

    SECTION("execute with progress callback") {
        auto [ats, dis] = analog_interval_peak_scenarios::operation_progress();

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
