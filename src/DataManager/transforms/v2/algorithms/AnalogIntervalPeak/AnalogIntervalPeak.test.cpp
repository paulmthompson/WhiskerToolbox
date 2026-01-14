#include "AnalogIntervalPeak.hpp"

#include "transforms/v2/core/ElementRegistry.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include "fixtures/scenarios/analog/analog_interval_peak_scenarios.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <memory>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;

TEST_CASE("TransformsV2: Analog Interval Peak - Maximum Within Intervals", "[transforms][v2][analog_interval_peak]") {
    auto & registry = ElementRegistry::instance();

    SECTION("Basic maximum detection within intervals") {
        auto [ats, dis] = analog_interval_peak_scenarios::basic_max_within();

        AnalogIntervalPeakParams params;
        params.peak_type = "maximum";
        params.search_mode = "within_intervals";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                DigitalEventSeries,
                AnalogIntervalPeakParams>(
                "AnalogIntervalPeak",
                *dis,
                *ats,
                params);

        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 2);

        // First interval [0, 200] -> max at index 200 (value 5.0)
        REQUIRE(events[0].time() == TimeFrameIndex(200.0));
        // Second interval [300, 500] -> max at index 300 (value 3.0)
        REQUIRE(events[1].time() == TimeFrameIndex(300.0));
    }

    SECTION("Maximum detection with progress callback") {
        auto [ats, dis] = analog_interval_peak_scenarios::max_with_progress();

        AnalogIntervalPeakParams params;
        params.peak_type = "maximum";
        params.search_mode = "within_intervals";

        int progress_val = -1;
        ComputeContext ctx;
        ctx.progress = [&](int p) { progress_val = p; };

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                DigitalEventSeries,
                AnalogIntervalPeakParams>(
                "AnalogIntervalPeak",
                *dis,
                *ats,
                params,
                ctx);

        REQUIRE(result != nullptr);
        REQUIRE(progress_val == 100);

        auto const & events = result->view();
        REQUIRE(result->size() == 2);
        REQUIRE(events[0].time() == TimeFrameIndex(10.0));// max in [0,20] is at index 10
        REQUIRE(events[1].time() == TimeFrameIndex(30.0));// max in [30,40] is at index 30
    }

    SECTION("Multiple intervals with varying peak locations") {
        auto [ats, dis] = analog_interval_peak_scenarios::multiple_intervals_varying();

        AnalogIntervalPeakParams params;
        params.peak_type = "maximum";
        params.search_mode = "within_intervals";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                DigitalEventSeries,
                AnalogIntervalPeakParams>(
                "AnalogIntervalPeak",
                *dis,
                *ats,
                params);

        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 3);
        REQUIRE(events[0].time() == TimeFrameIndex(10.0));// max in [0,20] is 9.0 at index 10
        REQUIRE(events[1].time() == TimeFrameIndex(40.0));// max in [30,50] is 8.0 at index 40
        REQUIRE(events[2].time() == TimeFrameIndex(70.0));// max in [60,80] is 10.0 at index 70
    }
}

TEST_CASE("TransformsV2: Analog Interval Peak - Minimum Within Intervals", "[transforms][v2][analog_interval_peak]") {
    auto & registry = ElementRegistry::instance();

    SECTION("Basic minimum detection within intervals") {
        auto [ats, dis] = analog_interval_peak_scenarios::basic_min_within();

        AnalogIntervalPeakParams params;
        params.peak_type = "minimum";
        params.search_mode = "within_intervals";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                DigitalEventSeries,
                AnalogIntervalPeakParams>(
                "AnalogIntervalPeak",
                *dis,
                *ats,
                params);

        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 2);
        REQUIRE(events[0].time() == TimeFrameIndex(200.0));// min in [0,200] is 1.0 at index 200
        REQUIRE(events[1].time() == TimeFrameIndex(400.0));// min in [300,500] is 2.0 at index 400
    }

    SECTION("Minimum with negative values") {
        auto [ats, dis] = analog_interval_peak_scenarios::min_with_negative();

        AnalogIntervalPeakParams params;
        params.peak_type = "minimum";
        params.search_mode = "within_intervals";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                DigitalEventSeries,
                AnalogIntervalPeakParams>(
                "AnalogIntervalPeak",
                *dis,
                *ats,
                params);

        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 2);
        REQUIRE(events[0].time() == TimeFrameIndex(10.0));// min in [0,20] is -5.0 at index 10
        REQUIRE(events[1].time() == TimeFrameIndex(30.0));// min in [20,40] is -3.0 at index 30
    }
}

TEST_CASE("TransformsV2: Analog Interval Peak - Between Interval Starts", "[transforms][v2][analog_interval_peak]") {
    auto & registry = ElementRegistry::instance();

    SECTION("Maximum between interval starts") {
        auto [ats, dis] = analog_interval_peak_scenarios::max_between_starts();

        AnalogIntervalPeakParams params;
        params.peak_type = "maximum";
        params.search_mode = "between_starts";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                DigitalEventSeries,
                AnalogIntervalPeakParams>(
                "AnalogIntervalPeak",
                *dis,
                *ats,
                params);

        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 3);

        // Between start 0 and start 20 (indices 0-19): max is 2.0 at index 10
        REQUIRE(events[0].time() == TimeFrameIndex(10.0));
        // Between start 20 and start 40 (indices 20-39): max is 8.0 at index 30
        REQUIRE(events[1].time() == TimeFrameIndex(30.0));
        // Last interval: from start 40 to end 50 (indices 40-50): max is 10.0 at index 40
        REQUIRE(events[2].time() == TimeFrameIndex(40.0));
    }

    SECTION("Minimum between interval starts") {
        auto [ats, dis] = analog_interval_peak_scenarios::min_between_starts();

        AnalogIntervalPeakParams params;
        params.peak_type = "minimum";
        params.search_mode = "between_starts";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                DigitalEventSeries,
                AnalogIntervalPeakParams>(
                "AnalogIntervalPeak",
                *dis,
                *ats,
                params);

        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 3);

        // Between 0 and 200: min is 2.0 at index 100
        REQUIRE(events[0].time() == TimeFrameIndex(100.0));
        // Between 200 and 400: min is 3.0 at index 300
        REQUIRE(events[1].time() == TimeFrameIndex(300.0));
        // Last from 400 to 500: min is 1.0 at index 500
        REQUIRE(events[2].time() == TimeFrameIndex(500.0));
    }
}

TEST_CASE("TransformsV2: Analog Interval Peak - Edge Cases", "[transforms][v2][analog_interval_peak]") {
    auto & registry = ElementRegistry::instance();

    SECTION("Empty intervals - no events") {
        auto [ats, dis] = analog_interval_peak_scenarios::empty_intervals();

        AnalogIntervalPeakParams params;
        params.peak_type = "maximum";
        params.search_mode = "within_intervals";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                DigitalEventSeries,
                AnalogIntervalPeakParams>(
                "AnalogIntervalPeak",
                *dis,
                *ats,
                params);

        REQUIRE(result != nullptr);
        REQUIRE(result->view().empty());
    }

    SECTION("Interval with no corresponding analog data") {
        auto [ats, dis] = analog_interval_peak_scenarios::no_data_interval();

        AnalogIntervalPeakParams params;
        params.peak_type = "maximum";
        params.search_mode = "within_intervals";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                DigitalEventSeries,
                AnalogIntervalPeakParams>(
                "AnalogIntervalPeak",
                *dis,
                *ats,
                params);

        REQUIRE(result != nullptr);
        // No data in the interval, so no events
        REQUIRE(result->view().empty());
    }

    SECTION("Single data point interval") {
        auto [ats, dis] = analog_interval_peak_scenarios::single_point();

        AnalogIntervalPeakParams params;
        params.peak_type = "maximum";
        params.search_mode = "within_intervals";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                DigitalEventSeries,
                AnalogIntervalPeakParams>(
                "AnalogIntervalPeak",
                *dis,
                *ats,
                params);

        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 1);
        REQUIRE(events[0].time() == TimeFrameIndex(10.0));
    }

    SECTION("Multiple intervals, some without data") {
        auto [ats, dis] = analog_interval_peak_scenarios::mixed_data_availability();

        AnalogIntervalPeakParams params;
        params.peak_type = "maximum";
        params.search_mode = "within_intervals";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                DigitalEventSeries,
                AnalogIntervalPeakParams>(
                "AnalogIntervalPeak",
                *dis,
                *ats,
                params);

        REQUIRE(result != nullptr);

        auto const & events = result->view();
        // Only intervals with data should produce events
        REQUIRE(result->size() == 2);
        REQUIRE(events[0].time() == TimeFrameIndex(10.0));// max in [0,10] is 5.0 at index 10
        REQUIRE(events[1].time() == TimeFrameIndex(20.0));// max in [10,20] is 8.0 at index 20
    }
}

TEST_CASE("TransformsV2: Analog Interval Peak - TimeFrame Conversion", "[transforms][v2][analog_interval_peak]") {
    auto & registry = ElementRegistry::instance();

    SECTION("Different timeframes - conversion required") {
        auto [ats, dis, signal_tf, interval_tf] = analog_interval_peak_scenarios::different_timeframes();

        AnalogIntervalPeakParams params;
        params.peak_type = "maximum";
        params.search_mode = "within_intervals";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                DigitalEventSeries,
                AnalogIntervalPeakParams>(
                "AnalogIntervalPeak",
                *dis,
                *ats,
                params);

        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 1);

        // The interval [1, 3] in interval indices corresponds to analog indices with timeframe conversion
        // Values at those indices: 5.0 and 2.0, max is 5.0 at index 1
        REQUIRE(events[0].time() == TimeFrameIndex(1.0));
    }

    SECTION("Same timeframe - no conversion needed") {
        auto [ats, dis, tf] = analog_interval_peak_scenarios::same_timeframe();

        AnalogIntervalPeakParams params;
        params.peak_type = "maximum";
        params.search_mode = "within_intervals";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                DigitalEventSeries,
                AnalogIntervalPeakParams>(
                "AnalogIntervalPeak",
                *dis,
                *ats,
                params);

        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 1);
        REQUIRE(events[0].time() == TimeFrameIndex(1.0));// max in [0,2] is 9.0 at index 1
    }
}

TEST_CASE("TransformsV2: Analog Interval Peak - Operation Interface", "[transforms][v2][analog_interval_peak]") {
    auto & registry = ElementRegistry::instance();

    SECTION("Transform is registered") {
        REQUIRE(registry.isContainerTransform("AnalogIntervalPeak"));
    }

    SECTION("Transform has correct metadata") {
        auto const * meta = registry.getContainerMetadata("AnalogIntervalPeak");
        REQUIRE(meta != nullptr);
        REQUIRE(meta->name == "AnalogIntervalPeak");
        REQUIRE(meta->is_multi_input == true);
        REQUIRE(meta->input_arity == 2);
        REQUIRE(meta->category == "Signal Processing / Time Series");
        REQUIRE(meta->supports_cancellation == true);
    }

    SECTION("execute with valid input") {
        auto [ats, dis] = analog_interval_peak_scenarios::operation_interface();

        AnalogIntervalPeakParams params;
        params.peak_type = "maximum";
        params.search_mode = "within_intervals";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                DigitalEventSeries,
                AnalogIntervalPeakParams>(
                "AnalogIntervalPeak",
                *dis,
                *ats,
                params);

        REQUIRE(result != nullptr);

        auto const & events = result->view();
        REQUIRE(result->size() == 2);
        REQUIRE(events.front().time() == TimeFrameIndex(10.0));
        REQUIRE(events[1].time() == TimeFrameIndex(30.0));
    }

    SECTION("execute with progress callback") {
        auto [ats, dis] = analog_interval_peak_scenarios::operation_progress();

        AnalogIntervalPeakParams params;
        params.peak_type = "maximum";
        params.search_mode = "within_intervals";

        int progress_val = -1;
        ComputeContext ctx;
        ctx.progress = [&](int p) { progress_val = p; };

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                DigitalEventSeries,
                AnalogIntervalPeakParams>(
                "AnalogIntervalPeak",
                *dis,
                *ats,
                params,
                ctx);

        REQUIRE(progress_val == 100);
        REQUIRE(result != nullptr);
    }
}
