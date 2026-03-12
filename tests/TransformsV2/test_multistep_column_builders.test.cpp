/**
 * @file test_multistep_column_builders.test.cpp
 * @brief Tests for Phase 6.3b: Multi-step pipeline support in TensorColumnBuilders.
 *
 * Tests verify that builder-produced ColumnProviderFn closures correctly handle
 * multi-step element transform pipelines for both timestamp-row and interval-row
 * tensor columns.
 *
 * Test organisation:
 *   Section A: Timestamp rows with element steps (no reduction)
 *   Section B: Timestamp rows with element steps + range reduction
 *   Section C: Interval rows with element steps + range reduction
 *   Section D: Interval rows with pre-reductions + element steps + range reduction (ZScore)
 *   Section E: End-to-end via buildProviderFromRecipe with pipeline JSON
 *   Section F: Error cases and edge cases
 *
 * @see TensorColumnBuilders.hpp for API documentation
 */

#include "TransformsV2/core/TensorColumnBuilders.hpp"

#include "DataManager/DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Tensors/TensorData.hpp"
#include "DataManager/Tensors/RowDescriptor.hpp"
#include "DataManager/Tensors/storage/LazyColumnTensorStorage.hpp"

#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/core/PipelineLoader.hpp"
#include "TransformsV2/core/RangeReductionRegistry.hpp"
#include "TransformsV2/core/ElementRegistry.hpp"
#include "TransformsV2/algorithms/ZScoreNormalization/ZScoreNormalizationV2.hpp"

#include "TimeFrame/StrongTimeTypes.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <any>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace WhiskerToolbox::TensorBuilders;
using namespace WhiskerToolbox::Transforms::V2;
using Catch::Matchers::WithinAbs;

// =============================================================================
// Test Helpers
// =============================================================================

namespace {

/**
 * @brief Create AnalogTimeSeries with custom values at timestamp indices 0..N-1.
 */
std::shared_ptr<AnalogTimeSeries> createAnalogWithValues(std::vector<float> const & values) {
    std::vector<TimeFrameIndex> times;
    times.reserve(values.size());
    for (std::size_t i = 0; i < values.size(); ++i) {
        times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
    }
    return std::make_shared<AnalogTimeSeries>(
        std::vector<float>(values), std::move(times));
}

std::shared_ptr<DigitalIntervalSeries> createIntervalSeries(
    std::vector<std::pair<int64_t, int64_t>> const & intervals)
{
    std::vector<Interval> vec;
    vec.reserve(intervals.size());
    for (auto const & [s, e] : intervals) {
        vec.push_back(Interval{s, e});
    }
    return std::make_shared<DigitalIntervalSeries>(vec);
}

std::vector<TimeFrameIndex> makeRowTimes(std::vector<int64_t> const & ts) {
    std::vector<TimeFrameIndex> result;
    result.reserve(ts.size());
    for (auto t : ts) {
        result.push_back(TimeFrameIndex(t));
    }
    return result;
}

std::unique_ptr<DataManager> makeDMWithAnalogValues(
    std::string const & key, std::vector<float> const & values)
{
    auto dm = std::make_unique<DataManager>();
    auto analog = createAnalogWithValues(values);
    dm->setData<AnalogTimeSeries>(key, analog, TimeKey("time"));
    return dm;
}

} // anonymous namespace

// =============================================================================
// Section A: Timestamp rows with element steps (no reduction)
// =============================================================================

TEST_CASE("MultiStep timestamp - ZScoreNormalizeV2 with fixed params",
          "[TensorColumnBuilders][MultiStep]") {
    // Source: values 0,1,2,3,4 at timestamps 0..4
    auto dm = makeDMWithAnalogValues("src", {0.0f, 1.0f, 2.0f, 3.0f, 4.0f});
    auto row_times = makeRowTimes({0, 1, 2, 3, 4});

    // Create pipeline with ZScoreNormalizeV2 (mean=2, std=1, no binding needed)
    TransformPipeline pipeline;
    pipeline.addStep("ZScoreNormalizeV2",
                     ZScoreNormalizationParamsV2{.mean = 2.0f, .std_dev = 1.0f});

    auto provider = buildPipelineColumnProvider(*dm, "src", row_times, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 5);
    // z = (x - 2) / 1 → -2, -1, 0, 1, 2
    CHECK_THAT(values[0], WithinAbs(-2.0f, 1e-5f));
    CHECK_THAT(values[1], WithinAbs(-1.0f, 1e-5f));
    CHECK_THAT(values[2], WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(values[3], WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(values[4], WithinAbs(2.0f, 1e-5f));
}

TEST_CASE("MultiStep timestamp - element steps return NaN for missing timestamps",
          "[TensorColumnBuilders][MultiStep]") {
    auto dm = makeDMWithAnalogValues("src", {10.0f, 20.0f, 30.0f});
    // Timestamp 100 is beyond the source range
    auto row_times = makeRowTimes({0, 100, 2});

    TransformPipeline pipeline;
    pipeline.addStep("ZScoreNormalizeV2",
                     ZScoreNormalizationParamsV2{.mean = 20.0f, .std_dev = 10.0f});

    auto provider = buildPipelineColumnProvider(*dm, "src", row_times, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 3);
    CHECK_THAT(values[0], WithinAbs(-1.0f, 1e-5f));  // (10-20)/10 = -1
    CHECK(std::isnan(values[1]));                      // missing → NaN
    CHECK_THAT(values[2], WithinAbs(1.0f, 1e-5f));    // (30-20)/10 = 1
}

TEST_CASE("MultiStep timestamp - empty pipeline falls back to passthrough",
          "[TensorColumnBuilders][MultiStep]") {
    auto dm = makeDMWithAnalogValues("src", {5.0f, 10.0f, 15.0f});
    auto row_times = makeRowTimes({0, 1, 2});

    TransformPipeline pipeline;  // empty, no steps, no reduction
    auto provider = buildPipelineColumnProvider(*dm, "src", row_times, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 3);
    CHECK(values[0] == 5.0f);
    CHECK(values[1] == 10.0f);
    CHECK(values[2] == 15.0f);
}

// =============================================================================
// Section B: Timestamp rows — range reductions are rejected (Pattern A)
// =============================================================================

TEST_CASE("MultiStep timestamp - range reduction on timestamp rows is rejected",
          "[TensorColumnBuilders][MultiStep]") {
    auto dm = makeDMWithAnalogValues("src", {0.0f, 5.0f, 10.0f});
    auto row_times = makeRowTimes({0, 1, 2});

    // Range reductions collapse an entire series to a single scalar — they are
    // not meaningful for timestamp-row columns. Use interval rows instead.
    TransformPipeline pipeline;
    pipeline.addStep("ZScoreNormalizeV2",
                     ZScoreNormalizationParamsV2{.mean = 5.0f, .std_dev = 5.0f});
    pipeline.setRangeReductionErased("MeanValue", std::any{});

    CHECK_THROWS_AS(
        buildPipelineColumnProvider(*dm, "src", row_times, std::move(pipeline)),
        std::runtime_error);
}

// =============================================================================
// Section C: Interval rows with element steps + range reduction
// =============================================================================

TEST_CASE("MultiStep interval - element steps + MeanValue reduction",
          "[TensorColumnBuilders][MultiStep]") {
    // Source: values 0,1,2,...,9 at timestamps 0..9
    auto dm = makeDMWithAnalogValues("src",
        {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f});
    // Intervals are inclusive on both ends: {0,4} gathers timestamps 0,1,2,3,4
    auto intervals = createIntervalSeries({{0, 4}, {5, 9}});

    // ZScore with fixed mean=0, std=1 (no-op), then MeanValue reduction
    // Just verifies the multi-step path works for intervals
    TransformPipeline pipeline;
    pipeline.addStep("ZScoreNormalizeV2",
                     ZScoreNormalizationParamsV2{.mean = 0.0f, .std_dev = 1.0f});
    pipeline.setRangeReductionErased("MeanValue", std::any{});

    auto provider = buildIntervalPipelineProvider(
        *dm, "src", intervals, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 2);
    // Interval [0,4]: values 0,1,2,3,4 → z-scores with mean=0,std=1 → same → mean = 2.0
    CHECK_THAT(values[0], WithinAbs(2.0f, 1e-5f));
    // Interval [5,9]: values 5,6,7,8,9 → same → mean = 7.0
    CHECK_THAT(values[1], WithinAbs(7.0f, 1e-5f));
}

TEST_CASE("MultiStep interval - element steps transform values before reduction",
          "[TensorColumnBuilders][MultiStep]") {
    // Source: values 10, 20, 30, 40, 50 at timestamps 0..4
    auto dm = makeDMWithAnalogValues("src", {10.0f, 20.0f, 30.0f, 40.0f, 50.0f});
    auto intervals = createIntervalSeries({{0, 4}});  // inclusive: timestamps 0-4

    // ZScore with mean=30, std=10 → [-2, -1, 0, 1, 2]
    // Then MeanValue → should be 0.0
    TransformPipeline pipeline;
    pipeline.addStep("ZScoreNormalizeV2",
                     ZScoreNormalizationParamsV2{.mean = 30.0f, .std_dev = 10.0f});
    pipeline.setRangeReductionErased("MeanValue", std::any{});

    auto provider = buildIntervalPipelineProvider(
        *dm, "src", intervals, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 1);
    CHECK_THAT(values[0], WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("MultiStep interval - element steps with MaxValue reduction",
          "[TensorColumnBuilders][MultiStep]") {
    // Source: values 1, 3, 5, 7, 9 at timestamps 0..4
    auto dm = makeDMWithAnalogValues("src", {1.0f, 3.0f, 5.0f, 7.0f, 9.0f});
    auto intervals = createIntervalSeries({{0, 4}});  // inclusive: timestamps 0-4

    // ZScore(mean=5, std=2) → [-2, -1, 0, 1, 2]
    // MaxValue → 2.0
    TransformPipeline pipeline;
    pipeline.addStep("ZScoreNormalizeV2",
                     ZScoreNormalizationParamsV2{.mean = 5.0f, .std_dev = 2.0f});
    pipeline.setRangeReductionErased("MaxValue", std::any{});

    auto provider = buildIntervalPipelineProvider(
        *dm, "src", intervals, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 1);
    CHECK_THAT(values[0], WithinAbs(2.0f, 1e-5f));
}

// =============================================================================
// Section D: Interval rows with pre-reductions + element steps + reduction
// =============================================================================

TEST_CASE("MultiStep interval - ZScore with pre-reductions (full pipeline)",
          "[TensorColumnBuilders][MultiStep]") {
    // Source: values 10, 20, 30, 40, 50 at timestamps 0..4
    // Two intervals: [0,4] and [0,2] (inclusive)
    auto dm = makeDMWithAnalogValues("src", {10.0f, 20.0f, 30.0f, 40.0f, 50.0f});
    auto intervals = createIntervalSeries({{0, 4}, {0, 2}});

    // Pipeline: pre-compute mean and std per interval, bind to ZScoreNormalizeV2,
    // then take MeanValue of the z-scored data (should be ~0 for each interval)
    TransformPipeline pipeline;

    // Pre-reductions: use Raw variants for the pipeline's AnalogTimeSeries
    // fast-path which operates on raw float spans
    pipeline.addPreReduction("MeanValueRaw", "computed_mean");
    pipeline.addPreReduction("StdValueRaw", "computed_std");

    // Element step: ZScore using bound mean/std
    pipeline.addStepWithBindings("ZScoreNormalizeV2",
        ZScoreNormalizationParamsV2{},
        {{"mean", "computed_mean"}, {"std_dev", "computed_std"}});

    pipeline.setRangeReductionErased("MeanValue", std::any{});

    auto provider = buildIntervalPipelineProvider(
        *dm, "src", intervals, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 2);
    // Mean of z-scored data should be ~0 for each interval
    CHECK_THAT(values[0], WithinAbs(0.0f, 0.01f));
    CHECK_THAT(values[1], WithinAbs(0.0f, 0.01f));
}

TEST_CASE("MultiStep interval - pre-reduction computes per-interval stats",
          "[TensorColumnBuilders][MultiStep]") {
    // Two intervals with very different value ranges to verify per-interval computation
    // Source: timestamps 0-9
    auto dm = makeDMWithAnalogValues("src",
        {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 100.0f, 200.0f, 300.0f, 400.0f, 500.0f});
    // Inclusive intervals: [0,4] and [5,9]
    auto intervals = createIntervalSeries({{0, 4}, {5, 9}});

    // Pre-compute mean per interval, then ZScore with std=1, then MaxValue
    // Interval 1: values 0-4, mean=2. ZScore → [-2,-1,0,1,2]. Max=2
    // Interval 2: values 100-500, mean=300. ZScore → [-200,-100,0,100,200]. Max=200
    TransformPipeline pipeline;
    pipeline.addPreReduction("MeanValueRaw", "computed_mean");
    pipeline.addStepWithBindings("ZScoreNormalizeV2",
        ZScoreNormalizationParamsV2{.std_dev = 1.0f},
        {{"mean", "computed_mean"}});
    pipeline.setRangeReductionErased("MaxValue", std::any{});

    auto provider = buildIntervalPipelineProvider(
        *dm, "src", intervals, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 2);
    CHECK_THAT(values[0], WithinAbs(2.0f, 1e-5f));
    CHECK_THAT(values[1], WithinAbs(200.0f, 1e-3f));
}

// =============================================================================
// Section E: End-to-end via buildProviderFromRecipe with pipeline JSON
// =============================================================================

TEST_CASE("MultiStep recipe - timestamp rows with element step JSON",
          "[TensorColumnBuilders][MultiStep]") {
    auto dm = makeDMWithAnalogValues("src", {0.0f, 10.0f, 20.0f});
    auto row_times = makeRowTimes({0, 1, 2});

    // JSON pipeline with ZScore step (fixed params)
    std::string json = R"({
        "steps": [
            {
                "step_id": "zscore_step",
                "transform_name": "ZScoreNormalizeV2",
                "parameters": { "mean": 10.0, "std_dev": 10.0 }
            }
        ]
    })";

    ColumnRecipe recipe;
    recipe.column_name = "zscore_col";
    recipe.source_key = "src";
    recipe.pipeline_json = json;

    auto provider = buildProviderFromRecipe(*dm, recipe, row_times, nullptr);
    auto values = provider();

    REQUIRE(values.size() == 3);
    // z = (x - 10) / 10: -1, 0, 1
    CHECK_THAT(values[0], WithinAbs(-1.0f, 1e-5f));
    CHECK_THAT(values[1], WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(values[2], WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("MultiStep recipe - interval rows with step + reduction JSON",
          "[TensorColumnBuilders][MultiStep]") {
    auto dm = makeDMWithAnalogValues("src", {10.0f, 20.0f, 30.0f, 40.0f, 50.0f});
    auto intervals = createIntervalSeries({{0, 4}});  // inclusive: timestamps 0-4

    // JSON: ZScore(mean=30, std=10) → MeanValue
    std::string json = R"({
        "steps": [
            {
                "step_id": "zscore_step",
                "transform_name": "ZScoreNormalizeV2",
                "parameters": { "mean": 30.0, "std_dev": 10.0 }
            }
        ],
        "range_reduction": {
            "reduction_name": "MeanValue"
        }
    })";

    ColumnRecipe recipe;
    recipe.column_name = "zscore_mean";
    recipe.source_key = "src";
    recipe.pipeline_json = json;

    auto provider = buildProviderFromRecipe(*dm, recipe, {}, intervals);
    auto values = provider();

    REQUIRE(values.size() == 1);
    CHECK_THAT(values[0], WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("MultiStep recipe - interval rows with pre-reductions + steps JSON",
          "[TensorColumnBuilders][MultiStep]") {
    auto dm = makeDMWithAnalogValues("src",
        {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f});
    // Inclusive intervals: [0,4] and [5,9]
    auto intervals = createIntervalSeries({{0, 4}, {5, 9}});

    // Full ZScore pipeline via JSON: pre-reduce mean/std → bind → normalize → MeanValue
    std::string json = R"({
        "pre_reductions": [
            { "reduction_name": "MeanValueRaw", "output_key": "computed_mean" },
            { "reduction_name": "StdValueRaw", "output_key": "computed_std" }
        ],
        "steps": [
            {
                "step_id": "zscore_step",
                "transform_name": "ZScoreNormalizeV2",
                "parameters": {},
                "param_bindings": {
                    "mean": "computed_mean",
                    "std_dev": "computed_std"
                }
            }
        ],
        "range_reduction": {
            "reduction_name": "MeanValue"
        }
    })";

    ColumnRecipe recipe;
    recipe.column_name = "zscore_full";
    recipe.source_key = "src";
    recipe.pipeline_json = json;

    auto provider = buildProviderFromRecipe(*dm, recipe, {}, intervals);
    auto values = provider();

    REQUIRE(values.size() == 2);
    // Mean of z-scored data should be ~0
    CHECK_THAT(values[0], WithinAbs(0.0f, 0.02f));
    CHECK_THAT(values[1], WithinAbs(0.0f, 0.02f));
}

// =============================================================================
// Section F: Error cases and edge cases
// =============================================================================

TEST_CASE("MultiStep timestamp - invalid source key throws",
          "[TensorColumnBuilders][MultiStep]") {
    auto dm = makeDMWithAnalogValues("src", {1.0f, 2.0f});
    auto row_times = makeRowTimes({0, 1});

    TransformPipeline pipeline;
    pipeline.addStep("ZScoreNormalizeV2",
                     ZScoreNormalizationParamsV2{.mean = 0.0f, .std_dev = 1.0f});

    CHECK_THROWS_AS(
        buildPipelineColumnProvider(*dm, "nonexistent", row_times, std::move(pipeline)),
        std::runtime_error);
}

TEST_CASE("MultiStep interval - empty interval series",
          "[TensorColumnBuilders][MultiStep]") {
    auto dm = makeDMWithAnalogValues("src", {1.0f, 2.0f, 3.0f});
    auto intervals = createIntervalSeries({});

    TransformPipeline pipeline;
    pipeline.addStep("ZScoreNormalizeV2",
                     ZScoreNormalizationParamsV2{.mean = 2.0f, .std_dev = 1.0f});
    pipeline.setRangeReductionErased("MeanValue", std::any{});

    auto provider = buildIntervalPipelineProvider(
        *dm, "src", intervals, std::move(pipeline));
    auto values = provider();

    CHECK(values.empty());
}

TEST_CASE("MultiStep interval - single-element interval",
          "[TensorColumnBuilders][MultiStep]") {
    auto dm = makeDMWithAnalogValues("src", {42.0f, 0.0f, 0.0f});
    auto intervals = createIntervalSeries({{0, 0}});  // inclusive: just timestamp 0

    // ZScore(mean=42, std=1) → (42-42)/1 = 0. MeanValue of single element = 0.
    TransformPipeline pipeline;
    pipeline.addStep("ZScoreNormalizeV2",
                     ZScoreNormalizationParamsV2{.mean = 42.0f, .std_dev = 1.0f});
    pipeline.setRangeReductionErased("MeanValue", std::any{});

    auto provider = buildIntervalPipelineProvider(
        *dm, "src", intervals, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 1);
    CHECK_THAT(values[0], WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("MultiStep interval - reduction-only pipeline still works (regression)",
          "[TensorColumnBuilders][MultiStep]") {
    // Verify that the original reduction-only path (no element steps) still works
    // after the multi-step refactor
    auto dm = makeDMWithAnalogValues("src",
        {0.0f, 1.0f, 2.0f, 3.0f, 4.0f});
    auto intervals = createIntervalSeries({{0, 4}});  // inclusive: timestamps 0-4

    TransformPipeline pipeline;
    pipeline.setRangeReductionErased("MeanValue", std::any{});

    auto provider = buildIntervalPipelineProvider(
        *dm, "src", intervals, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 1);
    CHECK_THAT(values[0], WithinAbs(2.0f, 1e-5f));
}

TEST_CASE("MultiStep end-to-end - lazy tensor with multi-step columns",
          "[TensorColumnBuilders][MultiStep]") {
    // Build a full lazy tensor with two columns:
    // 1. Raw mean (reduction only)
    // 2. ZScore'd mean (element step + reduction)
    auto dm = makeDMWithAnalogValues("src",
        {10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f});
    // Inclusive intervals: [0,4] and [5,9]
    auto intervals = createIntervalSeries({{0, 4}, {5, 9}});

    // Column 1: raw MeanValue
    TransformPipeline pipe1;
    pipe1.setRangeReductionErased("MeanValue", std::any{});
    auto col1 = buildIntervalPipelineProvider(
        *dm, "src", intervals, std::move(pipe1));

    // Column 2: ZScore(mean=55, std=10) → MeanValue
    TransformPipeline pipe2;
    pipe2.addStep("ZScoreNormalizeV2",
                  ZScoreNormalizationParamsV2{.mean = 55.0f, .std_dev = 10.0f});
    pipe2.setRangeReductionErased("MeanValue", std::any{});
    auto col2 = buildIntervalPipelineProvider(
        *dm, "src", intervals, std::move(pipe2));

    // Assemble lazy tensor
    std::vector<ColumnSource> columns = {
        {"raw_mean", std::move(col1)},
        {"zscore_mean", std::move(col2)}
    };

    auto tensor = TensorData::createFromLazyColumns(
        2, std::move(columns), RowDescriptor::ordinal(2));

    // Column 0 (raw mean): interval [0,4] → mean(10,20,30,40,50)=30
    //                       interval [5,9] → mean(60,70,80,90,100)=80
    auto row0 = tensor.row(0);
    auto row1 = tensor.row(1);
    REQUIRE(row0.size() == 2);
    REQUIRE(row1.size() == 2);

    CHECK_THAT(row0[0], WithinAbs(30.0f, 1e-5f));
    CHECK_THAT(row1[0], WithinAbs(80.0f, 1e-5f));

    // Column 1 (zscore mean): ZScore(mean=55,std=10) applied to each value first
    // Interval [0,4]: z-scores of 10-50 with mean=55,std=10 → [-4.5,-3.5,-2.5,-1.5,-0.5]
    //   → mean = -2.5
    // Interval [5,9]: z-scores of 60-100 → [0.5,1.5,2.5,3.5,4.5]
    //   → mean = 2.5
    CHECK_THAT(row0[1], WithinAbs(-2.5f, 1e-5f));
    CHECK_THAT(row1[1], WithinAbs(2.5f, 1e-5f));
}
