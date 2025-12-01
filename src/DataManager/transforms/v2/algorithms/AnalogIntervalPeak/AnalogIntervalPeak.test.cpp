#include "AnalogIntervalPeak.hpp"

#include "transforms/v2/core/ElementRegistry.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include "fixtures/AnalogIntervalPeakTestFixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <memory>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;

TEST_CASE_METHOD(AnalogIntervalPeakTestFixture, "TransformsV2: Analog Interval Peak - Maximum Within Intervals", "[transforms][v2][analog_interval_peak]") {
    auto& registry = ElementRegistry::instance();
    
    SECTION("Basic maximum detection within intervals") {
        auto ats = m_test_analog_signals["basic_max_within"];
        auto dis = m_test_interval_series["basic_max_within_intervals"];
        
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
        
        auto const& events = result->getEventSeries();
        REQUIRE(events.size() == 2);
        
        // First interval [0, 200] -> max at index 200 (value 5.0)
        REQUIRE(events[0] == TimeFrameIndex(200.0));
        // Second interval [300, 500] -> max at index 300 (value 3.0)
        REQUIRE(events[1] == TimeFrameIndex(300.0));
    }
    
    SECTION("Maximum detection with progress callback") {
        auto ats = m_test_analog_signals["max_with_progress"];
        auto dis = m_test_interval_series["max_with_progress_intervals"];
        
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
        
        auto const& events = result->getEventSeries();
        REQUIRE(events.size() == 2);
        REQUIRE(events[0] == TimeFrameIndex(10.0)); // max in [0,20] is at index 10
        REQUIRE(events[1] == TimeFrameIndex(30.0)); // max in [30,40] is at index 30
    }
    
    SECTION("Multiple intervals with varying peak locations") {
        auto ats = m_test_analog_signals["multiple_intervals_varying"];
        auto dis = m_test_interval_series["multiple_intervals_varying_intervals"];
        
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
        
        auto const& events = result->getEventSeries();
        REQUIRE(events.size() == 3);
        REQUIRE(events[0] == TimeFrameIndex(10.0)); // max in [0,20] is 9.0 at index 10
        REQUIRE(events[1] == TimeFrameIndex(40.0)); // max in [30,50] is 8.0 at index 40
        REQUIRE(events[2] == TimeFrameIndex(70.0)); // max in [60,80] is 10.0 at index 70
    }
}

TEST_CASE_METHOD(AnalogIntervalPeakTestFixture, "TransformsV2: Analog Interval Peak - Minimum Within Intervals", "[transforms][v2][analog_interval_peak]") {
    auto& registry = ElementRegistry::instance();
    
    SECTION("Basic minimum detection within intervals") {
        auto ats = m_test_analog_signals["basic_min_within"];
        auto dis = m_test_interval_series["basic_min_within_intervals"];
        
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
        
        auto const& events = result->getEventSeries();
        REQUIRE(events.size() == 2);
        REQUIRE(events[0] == TimeFrameIndex(200.0)); // min in [0,200] is 1.0 at index 200
        REQUIRE(events[1] == TimeFrameIndex(400.0)); // min in [300,500] is 2.0 at index 400
    }
    
    SECTION("Minimum with negative values") {
        auto ats = m_test_analog_signals["min_with_negative"];
        auto dis = m_test_interval_series["min_with_negative_intervals"];
        
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
        
        auto const& events = result->getEventSeries();
        REQUIRE(events.size() == 2);
        REQUIRE(events[0] == TimeFrameIndex(10.0)); // min in [0,20] is -5.0 at index 10
        REQUIRE(events[1] == TimeFrameIndex(30.0)); // min in [20,40] is -3.0 at index 30
    }
}

TEST_CASE_METHOD(AnalogIntervalPeakTestFixture, "TransformsV2: Analog Interval Peak - Between Interval Starts", "[transforms][v2][analog_interval_peak]") {
    auto& registry = ElementRegistry::instance();
    
    SECTION("Maximum between interval starts") {
        auto ats = m_test_analog_signals["max_between_starts"];
        auto dis = m_test_interval_series["max_between_starts_intervals"];
        
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
        
        auto const& events = result->getEventSeries();
        REQUIRE(events.size() == 3);
        
        // Between start 0 and start 20 (indices 0-19): max is 2.0 at index 10
        REQUIRE(events[0] == TimeFrameIndex(10.0));
        // Between start 20 and start 40 (indices 20-39): max is 8.0 at index 30
        REQUIRE(events[1] == TimeFrameIndex(30.0));
        // Last interval: from start 40 to end 50 (indices 40-50): max is 10.0 at index 40
        REQUIRE(events[2] == TimeFrameIndex(40.0));
    }
    
    SECTION("Minimum between interval starts") {
        auto ats = m_test_analog_signals["min_between_starts"];
        auto dis = m_test_interval_series["min_between_starts_intervals"];
        
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
        
        auto const& events = result->getEventSeries();
        REQUIRE(events.size() == 3);
        
        // Between 0 and 200: min is 2.0 at index 100
        REQUIRE(events[0] == TimeFrameIndex(100.0));
        // Between 200 and 400: min is 3.0 at index 300
        REQUIRE(events[1] == TimeFrameIndex(300.0));
        // Last from 400 to 500: min is 1.0 at index 500
        REQUIRE(events[2] == TimeFrameIndex(500.0));
    }
}

TEST_CASE_METHOD(AnalogIntervalPeakTestFixture, "TransformsV2: Analog Interval Peak - Edge Cases", "[transforms][v2][analog_interval_peak]") {
    auto& registry = ElementRegistry::instance();
    
    SECTION("Empty intervals - no events") {
        auto ats = m_test_analog_signals["empty_intervals"];
        auto dis = m_test_interval_series["empty_intervals_intervals"];
        
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
        REQUIRE(result->getEventSeries().empty());
    }
    
    SECTION("Interval with no corresponding analog data") {
        auto ats = m_test_analog_signals["no_data_interval"];
        auto dis = m_test_interval_series["no_data_interval_intervals"];
        
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
        REQUIRE(result->getEventSeries().empty());
    }
    
    SECTION("Single data point interval") {
        auto ats = m_test_analog_signals["single_point"];
        auto dis = m_test_interval_series["single_point_intervals"];
        
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
        
        auto const& events = result->getEventSeries();
        REQUIRE(events.size() == 1);
        REQUIRE(events[0] == TimeFrameIndex(10.0));
    }
    
    SECTION("Multiple intervals, some without data") {
        auto ats = m_test_analog_signals["mixed_data_availability"];
        auto dis = m_test_interval_series["mixed_data_availability_intervals"];
        
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
        
        auto const& events = result->getEventSeries();
        // Only intervals with data should produce events
        REQUIRE(events.size() == 2);
        REQUIRE(events[0] == TimeFrameIndex(10.0)); // max in [0,10] is 5.0 at index 10
        REQUIRE(events[1] == TimeFrameIndex(20.0)); // max in [10,20] is 8.0 at index 20
    }
}

TEST_CASE_METHOD(AnalogIntervalPeakTestFixture, "TransformsV2: Analog Interval Peak - TimeFrame Conversion", "[transforms][v2][analog_interval_peak]") {
    auto& registry = ElementRegistry::instance();
    
    SECTION("Different timeframes - conversion required") {
        auto ats = m_test_analog_signals["different_timeframes"];
        auto dis = m_test_interval_series["different_timeframes_intervals"];
        
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
        
        auto const& events = result->getEventSeries();
        REQUIRE(events.size() == 1);
        
        // The interval [1, 3] in interval indices corresponds to analog indices with timeframe conversion
        // Values at those indices: 5.0 and 2.0, max is 5.0 at index 1
        REQUIRE(events[0] == TimeFrameIndex(1.0));
    }
    
    SECTION("Same timeframe - no conversion needed") {
        auto ats = m_test_analog_signals["same_timeframe"];
        auto dis = m_test_interval_series["same_timeframe_intervals"];
        
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
        
        auto const& events = result->getEventSeries();
        REQUIRE(events.size() == 1);
        REQUIRE(events[0] == TimeFrameIndex(1.0)); // max in [0,2] is 9.0 at index 1
    }
}

TEST_CASE_METHOD(AnalogIntervalPeakTestFixture, "TransformsV2: Analog Interval Peak - Operation Interface", "[transforms][v2][analog_interval_peak]") {
    auto& registry = ElementRegistry::instance();
    
    SECTION("Transform is registered") {
        REQUIRE(registry.isContainerTransform("AnalogIntervalPeak"));
    }
    
    SECTION("Transform has correct metadata") {
        auto const* meta = registry.getContainerMetadata("AnalogIntervalPeak");
        REQUIRE(meta != nullptr);
        REQUIRE(meta->name == "AnalogIntervalPeak");
        REQUIRE(meta->is_multi_input == true);
        REQUIRE(meta->input_arity == 2);
        REQUIRE(meta->category == "Signal Processing / Time Series");
        REQUIRE(meta->supports_cancellation == true);
    }
    
    SECTION("execute with valid input") {
        auto ats = m_test_analog_signals["operation_interface"];
        auto dis = m_test_interval_series["operation_interface_intervals"];
        
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
        
        auto const& events = result->getEventSeries();
        REQUIRE(events.size() == 2);
        REQUIRE(events[0] == TimeFrameIndex(10.0));
        REQUIRE(events[1] == TimeFrameIndex(30.0));
    }
    
    SECTION("execute with progress callback") {
        auto ats = m_test_analog_signals["operation_progress"];
        auto dis = m_test_interval_series["operation_progress_intervals"];
        
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
