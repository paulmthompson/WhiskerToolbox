#include "IntervalReduction.hpp"

#include "TransformsV2/core/ComputeContext.hpp"
#include "TransformsV2/core/DataManagerIntegration.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/core/ParameterIO.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/interval_data.hpp"

#include "fixtures/builders/AnalogTimeSeriesBuilder.hpp"
#include "fixtures/builders/DigitalTimeSeriesBuilder.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>
#include <vector>

using namespace WhiskerToolbox::Transforms::V2;

// ============================================================================
// Helper to access TensorData element at (row, col) for 2D tensors
// ============================================================================
namespace {

float tensorAt(TensorData const & t, std::size_t row, std::size_t col) {
    std::size_t idx[2] = {row, col};
    return t.at(std::span<std::size_t const>(idx, 2));
}

}// anonymous namespace

// ============================================================================
// AnalogIntervalReduction Tests
// ============================================================================

TEST_CASE("V2 Binary Container Transform: AnalogIntervalReduction - MeanValue",
          "[transforms][v2][binary_container][interval_reduction]") {

    auto & registry = ElementRegistry::instance();
    ComputeContext ctx;

    SECTION("Basic mean of ramp signal within intervals") {
        // Signal: values [0, 1, 2, ..., 9] at times [0, 1, ..., 9]
        auto ats = AnalogTimeSeriesBuilder()
                .withRamp(0, 9, 0.0f, 9.0f)
                .build();

        // Intervals: [0,4] (values 0..4, mean=2), [5,9] (values 5..9, mean=7)
        auto dis = DigitalIntervalSeriesBuilder()
                .withInterval(0, 4)
                .withInterval(5, 9)
                .build();

        IntervalReductionParams params;
        params.reduction_name = "MeanValue";
        params.column_name = "test_mean";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                TensorData,
                IntervalReductionParams>(
                "AnalogIntervalReduction",
                *dis, *ats, params, ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->numRows() == 2);
        REQUIRE(result->numColumns() == 1);

        auto const & col_names = result->columnNames();
        REQUIRE(col_names.size() == 1);
        REQUIRE(col_names[0] == "test_mean");

        // Mean of [0,1,2,3,4] = 2.0, Mean of [5,6,7,8,9] = 7.0
        REQUIRE_THAT(tensorAt(*result, 0, 0), Catch::Matchers::WithinAbs(2.0, 0.01));
        REQUIRE_THAT(tensorAt(*result, 1, 0), Catch::Matchers::WithinAbs(7.0, 0.01));
    }

    SECTION("MaxValue reduction") {
        auto ats = AnalogTimeSeriesBuilder()
                .withRamp(0, 9, 0.0f, 9.0f)
                .build();

        auto dis = DigitalIntervalSeriesBuilder()
                .withInterval(0, 4)
                .withInterval(5, 9)
                .build();

        IntervalReductionParams params;
        params.reduction_name = "MaxValue";
        params.column_name = "test_max";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                TensorData,
                IntervalReductionParams>(
                "AnalogIntervalReduction",
                *dis, *ats, params, ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->numRows() == 2);
        // Max of [0,1,2,3,4] = 4.0, Max of [5,6,7,8,9] = 9.0
        REQUIRE_THAT(tensorAt(*result, 0, 0), Catch::Matchers::WithinAbs(4.0, 0.01));
        REQUIRE_THAT(tensorAt(*result, 1, 0), Catch::Matchers::WithinAbs(9.0, 0.01));
    }

    SECTION("Empty intervals produce empty TensorData") {
        auto ats = AnalogTimeSeriesBuilder()
                .withRamp(0, 9, 0.0f, 9.0f)
                .build();

        auto dis = DigitalIntervalSeriesBuilder()
                .build();

        IntervalReductionParams params;
        params.reduction_name = "MeanValue";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                TensorData,
                IntervalReductionParams>(
                "AnalogIntervalReduction",
                *dis, *ats, params, ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->numRows() == 0);
    }

    SECTION("Default parameters use MeanValue and 'value' column") {
        auto ats = AnalogTimeSeriesBuilder()
                .withRamp(0, 9, 0.0f, 9.0f)
                .build();

        auto dis = DigitalIntervalSeriesBuilder()
                .withInterval(0, 9)
                .build();

        IntervalReductionParams params;// all defaults

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                TensorData,
                IntervalReductionParams>(
                "AnalogIntervalReduction",
                *dis, *ats, params, ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->numRows() == 1);
        auto const & col_names = result->columnNames();
        REQUIRE(col_names.size() == 1);
        REQUIRE(col_names[0] == "value");
        // Mean of [0..9] = 4.5
        REQUIRE_THAT(tensorAt(*result, 0, 0), Catch::Matchers::WithinAbs(4.5, 0.01));
    }

    SECTION("Progress callback fires") {
        auto ats = AnalogTimeSeriesBuilder()
                .withRamp(0, 4, 0.0f, 4.0f)
                .build();

        auto dis = DigitalIntervalSeriesBuilder()
                .withInterval(0, 4)
                .build();

        int last_progress = -1;
        ComputeContext progress_ctx;
        progress_ctx.progress = [&](int p) { last_progress = p; };

        IntervalReductionParams params;

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                TensorData,
                IntervalReductionParams>(
                "AnalogIntervalReduction",
                *dis, *ats, params, progress_ctx);

        REQUIRE(result != nullptr);
        REQUIRE(last_progress == 100);
    }
}

// ============================================================================
// EventIntervalReduction Tests
// ============================================================================

TEST_CASE("V2 Binary Container Transform: EventIntervalReduction - EventCount",
          "[transforms][v2][binary_container][interval_reduction]") {

    auto & registry = ElementRegistry::instance();
    ComputeContext ctx;

    SECTION("Count events within intervals") {
        // Events at times: 1, 3, 5, 7, 9
        auto des = DigitalEventSeriesBuilder()
                .withEvents({1, 3, 5, 7, 9})
                .build();

        // Intervals: [0,4] has events {1,3} -> count=2, [5,9] has events {5,7,9} -> count=3
        auto dis = DigitalIntervalSeriesBuilder()
                .withInterval(0, 4)
                .withInterval(5, 9)
                .build();

        IntervalReductionParams params;
        params.reduction_name = "EventCount";
        params.column_name = "event_count";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                DigitalEventSeries,
                TensorData,
                IntervalReductionParams>(
                "EventIntervalReduction",
                *dis, *des, params, ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->numRows() == 2);
        REQUIRE(result->numColumns() == 1);

        auto const & col_names = result->columnNames();
        REQUIRE(col_names[0] == "event_count");

        REQUIRE_THAT(tensorAt(*result, 0, 0), Catch::Matchers::WithinAbs(2.0, 0.01));
        REQUIRE_THAT(tensorAt(*result, 1, 0), Catch::Matchers::WithinAbs(3.0, 0.01));
    }

    SECTION("Interval with no events returns 0") {
        auto des = DigitalEventSeriesBuilder()
                .withEvents({50, 60})
                .build();

        // No events in [0,10]
        auto dis = DigitalIntervalSeriesBuilder()
                .withInterval(0, 10)
                .build();

        IntervalReductionParams params;
        params.reduction_name = "EventCount";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                DigitalEventSeries,
                TensorData,
                IntervalReductionParams>(
                "EventIntervalReduction",
                *dis, *des, params, ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->numRows() == 1);
        REQUIRE_THAT(tensorAt(*result, 0, 0), Catch::Matchers::WithinAbs(0.0, 0.01));
    }
}

// ============================================================================
// IntervalOverlapReduction Tests
// ============================================================================

TEST_CASE("V2 Binary Container Transform: IntervalOverlapReduction - IntervalCount",
          "[transforms][v2][binary_container][interval_reduction]") {

    auto & registry = ElementRegistry::instance();
    ComputeContext ctx;

    SECTION("Count overlapping source intervals") {
        // Source intervals: [1,3], [5,8], [12,15]
        auto source = DigitalIntervalSeriesBuilder()
                .withInterval(1, 3)
                .withInterval(5, 8)
                .withInterval(12, 15)
                .build();

        // Row intervals: [0,9] overlaps with {[1,3],[5,8]} -> count=2,
        //                [10,18] overlaps with {[12,15]} -> count=1
        auto rows = DigitalIntervalSeriesBuilder()
                .withInterval(0, 9)
                .withInterval(10, 18)
                .build();

        IntervalReductionParams params;
        params.reduction_name = "IntervalCount";
        params.column_name = "overlap_count";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                DigitalIntervalSeries,
                TensorData,
                IntervalReductionParams>(
                "IntervalOverlapReduction",
                *rows, *source, params, ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->numRows() == 2);

        REQUIRE_THAT(tensorAt(*result, 0, 0), Catch::Matchers::WithinAbs(2.0, 0.01));
        REQUIRE_THAT(tensorAt(*result, 1, 0), Catch::Matchers::WithinAbs(1.0, 0.01));
    }

    SECTION("Empty source intervals produce zeros") {
        auto source = DigitalIntervalSeriesBuilder()
                .build();

        auto rows = DigitalIntervalSeriesBuilder()
                .withInterval(0, 9)
                .build();

        IntervalReductionParams params;
        params.reduction_name = "IntervalCount";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                DigitalIntervalSeries,
                TensorData,
                IntervalReductionParams>(
                "IntervalOverlapReduction",
                *rows, *source, params, ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->numRows() == 1);
        REQUIRE_THAT(tensorAt(*result, 0, 0), Catch::Matchers::WithinAbs(0.0, 0.01));
    }
}

// ============================================================================
// Registry Integration Tests
// ============================================================================

TEST_CASE("V2 IntervalReduction: Registry and metadata",
          "[transforms][v2][binary_container][interval_reduction][registry]") {

    auto & registry = ElementRegistry::instance();

    SECTION("AnalogIntervalReduction is registered") {
        auto const * meta = registry.getContainerMetadata("AnalogIntervalReduction");
        REQUIRE(meta != nullptr);
        REQUIRE(meta->is_multi_input == true);
        REQUIRE(meta->input_arity == 2);
        REQUIRE(meta->individual_input_types.size() == 2);
        REQUIRE(meta->individual_input_types[0] == std::type_index(typeid(DigitalIntervalSeries)));
        REQUIRE(meta->individual_input_types[1] == std::type_index(typeid(AnalogTimeSeries)));
        REQUIRE(meta->output_container_type == std::type_index(typeid(TensorData)));
    }

    SECTION("EventIntervalReduction is registered") {
        auto const * meta = registry.getContainerMetadata("EventIntervalReduction");
        REQUIRE(meta != nullptr);
        REQUIRE(meta->is_multi_input == true);
        REQUIRE(meta->input_arity == 2);
        REQUIRE(meta->individual_input_types.size() == 2);
        REQUIRE(meta->individual_input_types[0] == std::type_index(typeid(DigitalIntervalSeries)));
        REQUIRE(meta->individual_input_types[1] == std::type_index(typeid(DigitalEventSeries)));
        REQUIRE(meta->output_container_type == std::type_index(typeid(TensorData)));
    }

    SECTION("IntervalOverlapReduction is registered") {
        auto const * meta = registry.getContainerMetadata("IntervalOverlapReduction");
        REQUIRE(meta != nullptr);
        REQUIRE(meta->is_multi_input == true);
        REQUIRE(meta->input_arity == 2);
        REQUIRE(meta->individual_input_types.size() == 2);
        REQUIRE(meta->individual_input_types[0] == std::type_index(typeid(DigitalIntervalSeries)));
        REQUIRE(meta->individual_input_types[1] == std::type_index(typeid(DigitalIntervalSeries)));
        REQUIRE(meta->output_container_type == std::type_index(typeid(TensorData)));
    }
}

// ============================================================================
// Pipeline Integration Tests (JSON-driven execution)
// ============================================================================

TEST_CASE("V2 IntervalReduction: JSON Pipeline Execution",
          "[transforms][v2][binary_container][interval_reduction][pipeline]") {

    using namespace WhiskerToolbox::Transforms::V2;

    SECTION("AnalogIntervalReduction via DataManagerPipelineExecutor") {
        DataManager dm;
        auto time_frame = std::make_shared<TimeFrame>();
        dm.setTime(TimeKey("default"), time_frame);

        // Create analog signal: values [0..9] at times [0..9]
        auto ats = AnalogTimeSeriesBuilder()
                .withRamp(0, 9, 0.0f, 9.0f)
                .build();
        dm.setData<AnalogTimeSeries>("analog_signal", ats, TimeKey("default"));

        // Create intervals
        auto dis = DigitalIntervalSeriesBuilder()
                .withInterval(0, 4)
                .withInterval(5, 9)
                .build();
        dm.setData<DigitalIntervalSeries>("trial_intervals", dis, TimeKey("default"));

        // Build JSON pipeline
        nlohmann::json pipeline_json = {
                {"steps", {{
                        {"step_id", "reduce_step"},
                        {"transform_name", "AnalogIntervalReduction"},
                        {"input_key", "trial_intervals"},
                        {"additional_input_keys", {"analog_signal"}},
                        {"output_key", "reduced_tensor"},
                        {"parameters", {
                                {"reduction_name", "MeanValue"},
                                {"column_name", "mean_value"}
                        }}
                }}}
        };

        DataManagerPipelineExecutor executor(&dm);
        REQUIRE(executor.loadFromJson(pipeline_json));

        auto result = executor.execute();
        REQUIRE(result.success);

        // Verify output is stored in DataManager
        auto output_variant = dm.getDataVariant("reduced_tensor");
        REQUIRE(output_variant.has_value());

        auto tensor_ptr = std::get<std::shared_ptr<TensorData>>(*output_variant);
        REQUIRE(tensor_ptr != nullptr);
        REQUIRE(tensor_ptr->numRows() == 2);
        REQUIRE(tensor_ptr->numColumns() == 1);
        REQUIRE_THAT(tensorAt(*tensor_ptr, 0, 0), Catch::Matchers::WithinAbs(2.0, 0.01));
        REQUIRE_THAT(tensorAt(*tensor_ptr, 1, 0), Catch::Matchers::WithinAbs(7.0, 0.01));
    }

    SECTION("EventIntervalReduction via DataManagerPipelineExecutor") {
        DataManager dm;
        auto time_frame = std::make_shared<TimeFrame>();
        dm.setTime(TimeKey("default"), time_frame);

        auto des = DigitalEventSeriesBuilder()
                .withEvents({1, 3, 5, 7, 9})
                .build();
        dm.setData<DigitalEventSeries>("events", des, TimeKey("default"));

        auto dis = DigitalIntervalSeriesBuilder()
                .withInterval(0, 4)
                .withInterval(5, 9)
                .build();
        dm.setData<DigitalIntervalSeries>("intervals", dis, TimeKey("default"));

        nlohmann::json pipeline_json = {
                {"steps", {{
                        {"step_id", "count_step"},
                        {"transform_name", "EventIntervalReduction"},
                        {"input_key", "intervals"},
                        {"additional_input_keys", {"events"}},
                        {"output_key", "event_counts"},
                        {"parameters", {
                                {"reduction_name", "EventCount"},
                                {"column_name", "count"}
                        }}
                }}}
        };

        DataManagerPipelineExecutor executor(&dm);
        REQUIRE(executor.loadFromJson(pipeline_json));

        auto result = executor.execute();
        REQUIRE(result.success);

        auto output_variant = dm.getDataVariant("event_counts");
        REQUIRE(output_variant.has_value());

        auto tensor_ptr = std::get<std::shared_ptr<TensorData>>(*output_variant);
        REQUIRE(tensor_ptr != nullptr);
        REQUIRE(tensor_ptr->numRows() == 2);
        REQUIRE_THAT(tensorAt(*tensor_ptr, 0, 0), Catch::Matchers::WithinAbs(2.0, 0.01));
        REQUIRE_THAT(tensorAt(*tensor_ptr, 1, 0), Catch::Matchers::WithinAbs(3.0, 0.01));
    }

    SECTION("IntervalOverlapReduction via DataManagerPipelineExecutor") {
        DataManager dm;
        auto time_frame = std::make_shared<TimeFrame>();
        dm.setTime(TimeKey("default"), time_frame);

        auto source = DigitalIntervalSeriesBuilder()
                .withInterval(1, 3)
                .withInterval(5, 8)
                .withInterval(12, 15)
                .build();
        dm.setData<DigitalIntervalSeries>("source_intervals", source, TimeKey("default"));

        auto rows = DigitalIntervalSeriesBuilder()
                .withInterval(0, 9)
                .withInterval(10, 18)
                .build();
        dm.setData<DigitalIntervalSeries>("row_intervals", rows, TimeKey("default"));

        nlohmann::json pipeline_json = {
                {"steps", {{
                        {"step_id", "overlap_step"},
                        {"transform_name", "IntervalOverlapReduction"},
                        {"input_key", "row_intervals"},
                        {"additional_input_keys", {"source_intervals"}},
                        {"output_key", "overlap_counts"},
                        {"parameters", {
                                {"reduction_name", "IntervalCount"},
                                {"column_name", "overlaps"}
                        }}
                }}}
        };

        DataManagerPipelineExecutor executor(&dm);
        REQUIRE(executor.loadFromJson(pipeline_json));

        auto result = executor.execute();
        REQUIRE(result.success);

        auto output_variant = dm.getDataVariant("overlap_counts");
        REQUIRE(output_variant.has_value());

        auto tensor_ptr = std::get<std::shared_ptr<TensorData>>(*output_variant);
        REQUIRE(tensor_ptr != nullptr);
        REQUIRE(tensor_ptr->numRows() == 2);
        REQUIRE_THAT(tensorAt(*tensor_ptr, 0, 0), Catch::Matchers::WithinAbs(2.0, 0.01));
        REQUIRE_THAT(tensorAt(*tensor_ptr, 1, 0), Catch::Matchers::WithinAbs(1.0, 0.01));
    }
}

// ============================================================================
// Cross-TimeFrame Tests (Phase 3.3)
// ============================================================================

TEST_CASE("V2 IntervalReduction: Cross-TimeFrame AnalogIntervalReduction",
          "[transforms][v2][binary_container][interval_reduction][cross_timeframe]") {

    auto & registry = ElementRegistry::instance();
    ComputeContext ctx;

    SECTION("Intervals at 10 Hz, analog at 100 Hz — mean within intervals") {
        // Interval TimeFrame: 10 Hz → times [0, 100, 200, 300, ...]
        // Each index maps to 100ms intervals
        std::vector<int> interval_times;
        for (int i = 0; i < 40; ++i) {
            interval_times.push_back(i * 100);  // 0, 100, 200, ..., 3900 ms
        }
        auto interval_tf = std::make_shared<TimeFrame>(interval_times);

        // Source TimeFrame: 100 Hz → times [0, 10, 20, 30, ...]
        // Each index maps to 10ms intervals
        std::vector<int> source_times;
        for (int i = 0; i < 400; ++i) {
            source_times.push_back(i * 10);  // 0, 10, 20, ..., 3990 ms
        }
        auto source_tf = std::make_shared<TimeFrame>(source_times);

        // Analog data at 100 Hz: values [0, 1, 2, ..., 399] at source indices [0..399]
        auto ats = AnalogTimeSeriesBuilder()
                .withRamp(0, 399, 0.0f, 399.0f)
                .build();
        ats->setTimeFrame(source_tf);

        // Intervals at 10 Hz: [0,9] and [10,19] (in interval-TF indices)
        // [0,9] → [0ms, 900ms] → source indices [0, 90] → values 0..90
        // [10,19] → [1000ms, 1900ms] → source indices [100, 190] → values 100..190
        auto dis = DigitalIntervalSeriesBuilder()
                .withInterval(0, 9)
                .withInterval(10, 19)
                .build();
        dis->setTimeFrame(interval_tf);

        IntervalReductionParams params;
        params.reduction_name = "MeanValue";
        params.column_name = "cross_tf_mean";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                TensorData,
                IntervalReductionParams>(
                "AnalogIntervalReduction",
                *dis, *ats, params, ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->numRows() == 2);
        REQUIRE(result->numColumns() == 1);

        // First interval: source indices [0..90], values ~[0..90], mean ≈ 45
        // Second interval: source indices [100..190], values ~[100..190], mean ≈ 145
        float val0 = tensorAt(*result, 0, 0);
        float val1 = tensorAt(*result, 1, 0);
        REQUIRE_THAT(val0, Catch::Matchers::WithinAbs(45.0, 1.0));
        REQUIRE_THAT(val1, Catch::Matchers::WithinAbs(145.0, 1.0));
    }

    SECTION("Same TimeFrame — no conversion needed, identical to basic test") {
        auto shared_tf = std::make_shared<TimeFrame>(std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});

        auto ats = AnalogTimeSeriesBuilder()
                .withRamp(0, 9, 0.0f, 9.0f)
                .build();
        ats->setTimeFrame(shared_tf);

        auto dis = DigitalIntervalSeriesBuilder()
                .withInterval(0, 4)
                .withInterval(5, 9)
                .build();
        dis->setTimeFrame(shared_tf);

        IntervalReductionParams params;
        params.reduction_name = "MeanValue";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                TensorData,
                IntervalReductionParams>(
                "AnalogIntervalReduction",
                *dis, *ats, params, ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->numRows() == 2);
        REQUIRE_THAT(tensorAt(*result, 0, 0), Catch::Matchers::WithinAbs(2.0, 0.01));
        REQUIRE_THAT(tensorAt(*result, 1, 0), Catch::Matchers::WithinAbs(7.0, 0.01));
    }

    SECTION("Null TimeFrames — falls back to identity (no conversion)") {
        // Neither has a TimeFrame set, so no conversion happens
        auto ats = AnalogTimeSeriesBuilder()
                .withRamp(0, 9, 0.0f, 9.0f)
                .build();

        auto dis = DigitalIntervalSeriesBuilder()
                .withInterval(0, 4)
                .withInterval(5, 9)
                .build();

        IntervalReductionParams params;
        params.reduction_name = "MeanValue";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                AnalogTimeSeries,
                TensorData,
                IntervalReductionParams>(
                "AnalogIntervalReduction",
                *dis, *ats, params, ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->numRows() == 2);
        REQUIRE_THAT(tensorAt(*result, 0, 0), Catch::Matchers::WithinAbs(2.0, 0.01));
        REQUIRE_THAT(tensorAt(*result, 1, 0), Catch::Matchers::WithinAbs(7.0, 0.01));
    }
}

TEST_CASE("V2 IntervalReduction: Cross-TimeFrame EventIntervalReduction",
          "[transforms][v2][binary_container][interval_reduction][cross_timeframe]") {

    auto & registry = ElementRegistry::instance();
    ComputeContext ctx;

    SECTION("Intervals at 10 Hz, events at 100 Hz — count events in intervals") {
        // Interval TimeFrame: 10 Hz → times [0, 100, 200, ...]
        std::vector<int> interval_times;
        for (int i = 0; i < 20; ++i) {
            interval_times.push_back(i * 100);
        }
        auto interval_tf = std::make_shared<TimeFrame>(interval_times);

        // Source TimeFrame: 100 Hz → times [0, 10, 20, ...]
        std::vector<int> source_times;
        for (int i = 0; i < 200; ++i) {
            source_times.push_back(i * 10);
        }
        auto source_tf = std::make_shared<TimeFrame>(source_times);

        // Events at source indices: 5, 15, 25, 105, 115
        // In absolute times: 50ms, 150ms, 250ms, 1050ms, 1150ms
        auto des = DigitalEventSeriesBuilder()
                .withEvents({5, 15, 25, 105, 115})
                .build();
        des->setTimeFrame(source_tf);

        // Intervals in interval-TF:
        // [0, 4] → [0ms, 400ms] → source indices [0, 40]
        //   events in range: {5, 15, 25} → count = 3
        // [10, 14] → [1000ms, 1400ms] → source indices [100, 140]
        //   events in range: {105, 115} → count = 2
        auto dis = DigitalIntervalSeriesBuilder()
                .withInterval(0, 4)
                .withInterval(10, 14)
                .build();
        dis->setTimeFrame(interval_tf);

        IntervalReductionParams params;
        params.reduction_name = "EventCount";
        params.column_name = "cross_tf_event_count";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                DigitalEventSeries,
                TensorData,
                IntervalReductionParams>(
                "EventIntervalReduction",
                *dis, *des, params, ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->numRows() == 2);
        REQUIRE_THAT(tensorAt(*result, 0, 0), Catch::Matchers::WithinAbs(3.0, 0.01));
        REQUIRE_THAT(tensorAt(*result, 1, 0), Catch::Matchers::WithinAbs(2.0, 0.01));
    }
}

TEST_CASE("V2 IntervalReduction: Cross-TimeFrame IntervalOverlapReduction",
          "[transforms][v2][binary_container][interval_reduction][cross_timeframe]") {

    auto & registry = ElementRegistry::instance();
    ComputeContext ctx;

    SECTION("Row intervals at 10 Hz, source intervals at 100 Hz — count overlaps") {
        // Interval TimeFrame: 10 Hz
        std::vector<int> interval_times;
        for (int i = 0; i < 20; ++i) {
            interval_times.push_back(i * 100);
        }
        auto interval_tf = std::make_shared<TimeFrame>(interval_times);

        // Source TimeFrame: 100 Hz
        std::vector<int> source_times;
        for (int i = 0; i < 200; ++i) {
            source_times.push_back(i * 10);
        }
        auto source_tf = std::make_shared<TimeFrame>(source_times);

        // Source intervals at 100 Hz indices: [10,30], [50,70], [120,140]
        // In absolute times: [100ms,300ms], [500ms,700ms], [1200ms,1400ms]
        auto source = DigitalIntervalSeriesBuilder()
                .withInterval(10, 30)
                .withInterval(50, 70)
                .withInterval(120, 140)
                .build();
        source->setTimeFrame(source_tf);

        // Row intervals at 10 Hz:
        // [0, 9] → [0ms, 900ms] → source indices [0, 90]
        //   overlaps with [10,30] and [50,70] → count = 2
        // [10, 18] → [1000ms, 1800ms] → source indices [100, 180]
        //   overlaps with [120,140] → count = 1
        auto rows = DigitalIntervalSeriesBuilder()
                .withInterval(0, 9)
                .withInterval(10, 18)
                .build();
        rows->setTimeFrame(interval_tf);

        IntervalReductionParams params;
        params.reduction_name = "IntervalCount";
        params.column_name = "cross_tf_overlap_count";

        auto result = registry.executeBinaryContainerTransform<
                DigitalIntervalSeries,
                DigitalIntervalSeries,
                TensorData,
                IntervalReductionParams>(
                "IntervalOverlapReduction",
                *rows, *source, params, ctx);

        REQUIRE(result != nullptr);
        REQUIRE(result->numRows() == 2);
        REQUIRE_THAT(tensorAt(*result, 0, 0), Catch::Matchers::WithinAbs(2.0, 0.01));
        REQUIRE_THAT(tensorAt(*result, 1, 0), Catch::Matchers::WithinAbs(1.0, 0.01));
    }
}

// ============================================================================
// Cross-TimeFrame JSON Pipeline Test (Phase 3.3)
// ============================================================================

TEST_CASE("V2 IntervalReduction: Cross-TimeFrame JSON Pipeline",
          "[transforms][v2][binary_container][interval_reduction][cross_timeframe][pipeline]") {

    using namespace WhiskerToolbox::Transforms::V2;

    SECTION("AnalogIntervalReduction with different TimeFrames via pipeline executor") {
        DataManager dm;

        // Two TimeFrames at different rates
        std::vector<int> interval_times;
        for (int i = 0; i < 20; ++i) {
            interval_times.push_back(i * 100);  // 10 Hz
        }
        auto interval_tf = std::make_shared<TimeFrame>(interval_times);
        dm.setTime(TimeKey("interval_clock"), interval_tf);

        std::vector<int> source_times;
        for (int i = 0; i < 200; ++i) {
            source_times.push_back(i * 10);  // 100 Hz
        }
        auto source_tf = std::make_shared<TimeFrame>(source_times);
        dm.setTime(TimeKey("source_clock"), source_tf);

        // Ramp signal at 100 Hz: values [0..199] at indices [0..199]
        auto ats = AnalogTimeSeriesBuilder()
                .withRamp(0, 199, 0.0f, 199.0f)
                .build();
        ats->setTimeFrame(source_tf);
        dm.setData<AnalogTimeSeries>("analog_signal", ats, TimeKey("source_clock"));

        // Intervals at 10 Hz: [0,4] → [0ms,400ms], [10,14] → [1000ms,1400ms]
        auto dis = DigitalIntervalSeriesBuilder()
                .withInterval(0, 4)
                .withInterval(10, 14)
                .build();
        dis->setTimeFrame(interval_tf);
        dm.setData<DigitalIntervalSeries>("trial_intervals", dis, TimeKey("interval_clock"));

        nlohmann::json pipeline_json = {
                {"steps", {{
                        {"step_id", "cross_tf_reduce"},
                        {"transform_name", "AnalogIntervalReduction"},
                        {"input_key", "trial_intervals"},
                        {"additional_input_keys", {"analog_signal"}},
                        {"output_key", "reduced_tensor"},
                        {"parameters", {
                                {"reduction_name", "MeanValue"},
                                {"column_name", "cross_tf_mean"}
                        }}
                }}}
        };

        DataManagerPipelineExecutor executor(&dm);
        REQUIRE(executor.loadFromJson(pipeline_json));

        auto result = executor.execute();
        REQUIRE(result.success);

        auto output_variant = dm.getDataVariant("reduced_tensor");
        REQUIRE(output_variant.has_value());

        auto tensor_ptr = std::get<std::shared_ptr<TensorData>>(*output_variant);
        REQUIRE(tensor_ptr != nullptr);
        REQUIRE(tensor_ptr->numRows() == 2);
        REQUIRE(tensor_ptr->numColumns() == 1);

        // [0,4] in 10Hz → [0ms,400ms] → source [0,40]: values ~[0..40], mean ≈ 20
        // [10,14] in 10Hz → [1000ms,1400ms] → source [100,140]: values ~[100..140], mean ≈ 120
        REQUIRE_THAT(tensorAt(*tensor_ptr, 0, 0), Catch::Matchers::WithinAbs(20.0, 1.0));
        REQUIRE_THAT(tensorAt(*tensor_ptr, 1, 0), Catch::Matchers::WithinAbs(120.0, 1.0));
    }
}
