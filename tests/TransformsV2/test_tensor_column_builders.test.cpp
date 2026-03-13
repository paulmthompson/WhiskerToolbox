/**
 * @file TensorColumnBuilders.test.cpp
 * @brief Tests for TensorColumnBuilders — generic pipeline column providers.
 *
 * Tests verify that builder-produced ColumnProviderFn closures generate correct
 * output for:
 *   - Pattern A: Generic pipeline passthrough (timestamp-row)
 *   - Pattern B: Generic interval gather + pipeline (interval-row)
 *   - Interval property extraction (Start, End, Duration)
 *   - ColumnRecipe → ColumnProviderFn dispatching
 *   - Invalidation wiring (DataManager observers → column invalidation)
 *
 * @see TensorColumnBuilders.hpp for API documentation
 */

#include "TransformsV2/core/TensorColumnBuilders.hpp"

#include "DataManager/DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Points/Point_Data.hpp"
#include "Tensors/TensorData.hpp"
#include "Tensors/storage/LazyColumnTensorStorage.hpp"

#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/core/RangeReductionRegistry.hpp"

#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <any>
#include <cmath>
#include <memory>
#include <numeric>
#include <vector>

using namespace WhiskerToolbox::TensorBuilders;
using WhiskerToolbox::Transforms::V2::TransformPipeline;
using Catch::Matchers::WithinAbs;

// =============================================================================
// Test Helpers
// =============================================================================

namespace {

/**
 * @brief Create an AnalogTimeSeries with values = index (0, 1, 2, ..., num_samples-1)
 *        at timestamps 0, 1, 2, ...
 */
std::shared_ptr<AnalogTimeSeries> createLinearAnalog(std::size_t num_samples) {
    std::vector<float> data;
    std::vector<TimeFrameIndex> times;
    data.reserve(num_samples);
    times.reserve(num_samples);
    for (std::size_t i = 0; i < num_samples; ++i) {
        data.push_back(static_cast<float>(i));
        times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
    }
    return std::make_shared<AnalogTimeSeries>(std::move(data), std::move(times));
}

/**
 * @brief Create a DigitalIntervalSeries from (start, end) pairs
 */
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

/**
 * @brief Create a DigitalEventSeries at specified times
 */
std::shared_ptr<DigitalEventSeries> createEventSeries(
    std::vector<int64_t> const & times)
{
    auto series = std::make_shared<DigitalEventSeries>();
    for (auto t : times) {
        series->addEvent(TimeFrameIndex(t));
    }
    return series;
}

/**
 * @brief Set up a minimal DataManager with an AnalogTimeSeries.
 * Uses a pointer since DataManager is non-copyable.
 */
std::unique_ptr<DataManager> makeDMWithAnalog(std::string const & key, std::size_t samples) {
    auto dm = std::make_unique<DataManager>();
    auto analog = createLinearAnalog(samples);
    dm->setData<AnalogTimeSeries>(key, analog, TimeKey("time"));
    return dm;
}

/**
 * @brief Create row timestamps vector from ints
 */
std::vector<TimeFrameIndex> makeRowTimes(std::vector<int64_t> const & ts) {
    std::vector<TimeFrameIndex> result;
    result.reserve(ts.size());
    for (auto t : ts) {
        result.push_back(TimeFrameIndex(t));
    }
    return result;
}

} // anonymous namespace

// =============================================================================
// buildPipelineColumnProvider — Passthrough Tests (Pattern A, empty pipeline)
// =============================================================================

TEST_CASE("buildPipelineColumnProvider - basic passthrough", "[TensorColumnBuilders]") {
    auto dm = makeDMWithAnalog("analog_src", 100);
    auto row_times = makeRowTimes({0, 10, 20, 50, 99});

    auto provider = buildPipelineColumnProvider(*dm, "analog_src", row_times, TransformPipeline{});
    auto values = provider();

    REQUIRE(values.size() == 5);
    CHECK(values[0] == 0.0f);
    CHECK(values[1] == 10.0f);
    CHECK(values[2] == 20.0f);
    CHECK(values[3] == 50.0f);
    CHECK(values[4] == 99.0f);
}

TEST_CASE("buildPipelineColumnProvider - missing timestamps produce NaN", "[TensorColumnBuilders]") {
    auto dm = makeDMWithAnalog("analog_src", 10);
    // Request timestamps beyond the source range
    auto row_times = makeRowTimes({0, 5, 999});

    auto provider = buildPipelineColumnProvider(*dm, "analog_src", row_times, TransformPipeline{});
    auto values = provider();

    REQUIRE(values.size() == 3);
    CHECK(values[0] == 0.0f);
    CHECK(values[1] == 5.0f);
    CHECK(std::isnan(values[2]));
}

TEST_CASE("buildPipelineColumnProvider - invalid source key throws", "[TensorColumnBuilders]") {
    DataManager dm;
    auto row_times = makeRowTimes({0, 1, 2});

    CHECK_THROWS_AS(
        buildPipelineColumnProvider(dm, "nonexistent", row_times, TransformPipeline{}),
        std::runtime_error);
}

TEST_CASE("buildPipelineColumnProvider - reflects data changes on re-invoke", "[TensorColumnBuilders]") {
    DataManager dm;
    auto analog = createLinearAnalog(10);
    dm.setData<AnalogTimeSeries>("src", analog, TimeKey("time"));

    auto row_times = makeRowTimes({0, 5});
    auto provider = buildPipelineColumnProvider(dm, "src", row_times, TransformPipeline{});

    auto v1 = provider();
    CHECK(v1[0] == 0.0f);
    CHECK(v1[1] == 5.0f);

    // Replace source data with different values
    std::vector<float> new_data{100.0f, 101.0f, 102.0f, 103.0f, 104.0f,
                                105.0f, 106.0f, 107.0f, 108.0f, 109.0f};
    std::vector<TimeFrameIndex> new_times;
    for (int i = 0; i < 10; ++i) new_times.push_back(TimeFrameIndex(i));
    auto new_analog = std::make_shared<AnalogTimeSeries>(std::move(new_data), std::move(new_times));
    dm.setData<AnalogTimeSeries>("src", new_analog, TimeKey("time"));

    auto v2 = provider();
    CHECK(v2[0] == 100.0f);
    CHECK(v2[1] == 105.0f);
}

TEST_CASE("buildPipelineColumnProvider - empty row times throws", "[TensorColumnBuilders]") {
    auto dm = makeDMWithAnalog("src", 10);
    std::vector<TimeFrameIndex> empty_times;

    CHECK_THROWS_AS(
        buildPipelineColumnProvider(*dm, "src", empty_times, TransformPipeline{}),
        std::runtime_error);
}

// =============================================================================
// buildIntervalPropertyProvider Tests
// =============================================================================

TEST_CASE("buildIntervalPropertyProvider - Start", "[TensorColumnBuilders]") {
    auto intervals = createIntervalSeries({{10, 30}, {50, 80}, {100, 200}});

    auto provider = buildIntervalPropertyProvider(intervals, IntervalProperty::Start);
    auto values = provider();

    REQUIRE(values.size() == 3);
    CHECK(values[0] == 10.0f);
    CHECK(values[1] == 50.0f);
    CHECK(values[2] == 100.0f);
}

TEST_CASE("buildIntervalPropertyProvider - End", "[TensorColumnBuilders]") {
    auto intervals = createIntervalSeries({{10, 30}, {50, 80}, {100, 200}});

    auto provider = buildIntervalPropertyProvider(intervals, IntervalProperty::End);
    auto values = provider();

    REQUIRE(values.size() == 3);
    CHECK(values[0] == 30.0f);
    CHECK(values[1] == 80.0f);
    CHECK(values[2] == 200.0f);
}

TEST_CASE("buildIntervalPropertyProvider - Duration", "[TensorColumnBuilders]") {
    auto intervals = createIntervalSeries({{10, 30}, {50, 80}, {100, 200}});

    auto provider = buildIntervalPropertyProvider(intervals, IntervalProperty::Duration);
    auto values = provider();

    REQUIRE(values.size() == 3);
    CHECK(values[0] == 20.0f);
    CHECK(values[1] == 30.0f);
    CHECK(values[2] == 100.0f);
}

TEST_CASE("buildIntervalPropertyProvider - null intervals throws", "[TensorColumnBuilders]") {
    CHECK_THROWS_AS(
        buildIntervalPropertyProvider(nullptr, IntervalProperty::Start),
        std::runtime_error);
}

// =============================================================================
// buildIntervalPipelineProvider Tests — AnalogTimeSeries (Pattern B)
// =============================================================================

TEST_CASE("buildIntervalPipelineProvider - Analog MeanValue", "[TensorColumnBuilders]") {
    // Linear analog: values = 0..99 at times 0..99
    DataManager dm;
    auto analog = createLinearAnalog(100);
    dm.setData<AnalogTimeSeries>("analog", analog, TimeKey("time"));

    // Two intervals: [10, 20] and [50, 60]
    auto intervals = createIntervalSeries({{10, 20}, {50, 60}});

    // Pipeline with MeanValue reduction
    auto pipeline = WhiskerToolbox::Transforms::V2::TransformPipeline();
    pipeline.setRangeReductionErased("MeanValue", std::any{});

    auto provider = buildIntervalPipelineProvider(dm, "analog", intervals, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 2);

    // Mean of [10..20] inclusive = (10+11+...+20)/11 = 165/11 = 15.0
    CHECK_THAT(values[0], WithinAbs(15.0, 0.01));

    // Mean of [50..60] inclusive = (50+51+...+60)/11 = 605/11 = 55.0
    CHECK_THAT(values[1], WithinAbs(55.0, 0.01));
}

TEST_CASE("buildIntervalPipelineProvider - Analog SumValue", "[TensorColumnBuilders]") {
    DataManager dm;
    auto analog = createLinearAnalog(100);
    dm.setData<AnalogTimeSeries>("analog", analog, TimeKey("time"));

    auto intervals = createIntervalSeries({{0, 4}});

    auto pipeline = WhiskerToolbox::Transforms::V2::TransformPipeline();
    pipeline.setRangeReductionErased("SumValue", std::any{});

    auto provider = buildIntervalPipelineProvider(dm, "analog", intervals, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 1);
    // Sum of [0..4] = 0+1+2+3+4 = 10
    CHECK_THAT(values[0], WithinAbs(10.0, 0.01));
}

TEST_CASE("buildIntervalPipelineProvider - Analog MaxValue", "[TensorColumnBuilders]") {
    DataManager dm;
    auto analog = createLinearAnalog(100);
    dm.setData<AnalogTimeSeries>("analog", analog, TimeKey("time"));

    auto intervals = createIntervalSeries({{10, 20}, {90, 99}});

    auto pipeline = WhiskerToolbox::Transforms::V2::TransformPipeline();
    pipeline.setRangeReductionErased("MaxValue", std::any{});

    auto provider = buildIntervalPipelineProvider(dm, "analog", intervals, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 2);
    CHECK_THAT(values[0], WithinAbs(20.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(99.0, 0.01));
}

// =============================================================================
// buildIntervalPipelineProvider Tests — DigitalEventSeries (Pattern B)
// =============================================================================

TEST_CASE("buildIntervalPipelineProvider - Event EventCount", "[TensorColumnBuilders]") {
    DataManager dm;
    auto events = createEventSeries({5, 15, 25, 35, 45, 55});
    dm.setData<DigitalEventSeries>("events", events, TimeKey("time"));

    // Two intervals: [0,20] should contain 2 events (5,15); [30,50] should contain 2 events (35,45)
    auto intervals = createIntervalSeries({{0, 20}, {30, 50}});

    auto pipeline = WhiskerToolbox::Transforms::V2::TransformPipeline();
    pipeline.setRangeReductionErased("EventCount", std::any{});

    auto provider = buildIntervalPipelineProvider(dm, "events", intervals, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 2);
    CHECK_THAT(values[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(2.0, 0.01));
}

TEST_CASE("buildIntervalPipelineProvider - Event empty interval returns zero count",
          "[TensorColumnBuilders]") {
    DataManager dm;
    auto events = createEventSeries({5, 15, 25});
    dm.setData<DigitalEventSeries>("events", events, TimeKey("time"));

    // Interval [100, 200] has no events
    auto intervals = createIntervalSeries({{100, 200}});

    auto pipeline = WhiskerToolbox::Transforms::V2::TransformPipeline();
    pipeline.setRangeReductionErased("EventCount", std::any{});

    auto provider = buildIntervalPipelineProvider(dm, "events", intervals, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 1);
    CHECK_THAT(values[0], WithinAbs(0.0, 0.01));
}

// =============================================================================
// buildIntervalPipelineProvider — Validation
// =============================================================================

TEST_CASE("buildIntervalPipelineProvider - null intervals throws", "[TensorColumnBuilders]") {
    DataManager dm;
    auto analog = createLinearAnalog(10);
    dm.setData<AnalogTimeSeries>("analog", analog, TimeKey("time"));

    auto pipeline = WhiskerToolbox::Transforms::V2::TransformPipeline();
    pipeline.setRangeReductionErased("MeanValue", std::any{});

    CHECK_THROWS_AS(
        buildIntervalPipelineProvider(dm, "analog", nullptr, std::move(pipeline)),
        std::runtime_error);
}

TEST_CASE("buildIntervalPipelineProvider - missing reduction throws", "[TensorColumnBuilders]") {
    DataManager dm;
    auto analog = createLinearAnalog(10);
    dm.setData<AnalogTimeSeries>("analog", analog, TimeKey("time"));
    auto intervals = createIntervalSeries({{0, 5}});

    auto pipeline = WhiskerToolbox::Transforms::V2::TransformPipeline();
    // No range reduction set

    CHECK_THROWS_AS(
        buildIntervalPipelineProvider(dm, "analog", intervals, std::move(pipeline)),
        std::runtime_error);
}

TEST_CASE("buildIntervalPipelineProvider - PointData with incompatible reduction throws at runtime",
          "[TensorColumnBuilders]") {
    DataManager dm;
    // PointData source with a MeanValue reduction — builds OK but fails at runtime
    // because MeanValue operates on TimeValuePoint, not Point2D<float>
    dm.setData<PointData>("points", TimeKey("time"));
    auto intervals = createIntervalSeries({{0, 10}});

    auto pipeline = WhiskerToolbox::Transforms::V2::TransformPipeline();
    pipeline.setRangeReductionErased("MeanValue", std::any{});

    // Build succeeds (pipelineProducesFloat passes — MeanValue declares float output)
    auto provider = buildIntervalPipelineProvider(dm, "points", intervals, std::move(pipeline));

    // Runtime execution throws because the pipeline cannot process PointData with MeanValue
    CHECK_THROWS_AS(provider(), std::runtime_error);
}

// =============================================================================
// buildInvalidationWiringFn Tests
// =============================================================================

TEST_CASE("buildInvalidationWiringFn - wires observers correctly", "[TensorColumnBuilders]") {
    DataManager dm;
    auto analog = createLinearAnalog(10);
    dm.setData<AnalogTimeSeries>("src1", analog, TimeKey("time"));
    dm.setData<AnalogTimeSeries>("src2", createLinearAnalog(10), TimeKey("time"));

    // Build a lazy tensor with two columns
    auto row_times = makeRowTimes({0, 1, 2, 3, 4, 5, 6, 7, 8, 9});

    auto provider1 = buildPipelineColumnProvider(dm, "src1", row_times, TransformPipeline{});
    auto provider2 = buildPipelineColumnProvider(dm, "src2", row_times, TransformPipeline{});

    std::vector<ColumnSource> columns;
    columns.push_back(ColumnSource{"col1", std::move(provider1), {}});
    columns.push_back(ColumnSource{"col2", std::move(provider2), {}});

    auto wiring = buildInvalidationWiringFn(dm, {"src1", "src2"});

    auto row_desc = RowDescriptor::ordinal(10);
    auto tensor = TensorData::createFromLazyColumns(10, std::move(columns), std::move(row_desc), wiring);

    // Access columns to materialize them
    auto col1_before = tensor.getColumn(0);
    auto col2_before = tensor.getColumn(1);
    REQUIRE(col1_before.size() == 10);
    REQUIRE(col2_before.size() == 10);

    // Track if tensor observers fire
    bool observer_fired = false;
    [[maybe_unused]] auto obs_id = tensor.addObserver([&observer_fired]() {
        observer_fired = true;
    });

    // Notify that src1 changed — should invalidate column 0 and fire tensor observer
    auto src1 = dm.getData<AnalogTimeSeries>("src1");
    src1->notifyObservers();

    CHECK(observer_fired);
}

TEST_CASE("buildInvalidationWiringFn - empty source key skipped", "[TensorColumnBuilders]") {
    DataManager dm;
    auto analog = createLinearAnalog(10);
    dm.setData<AnalogTimeSeries>("src1", analog, TimeKey("time"));

    // Column 1 has empty key (interval property — no source dependency)
    auto wiring = buildInvalidationWiringFn(dm, {"src1", ""});

    auto provider1 = buildPipelineColumnProvider(dm, "src1", makeRowTimes({0, 1, 2}), TransformPipeline{});

    auto intervals = createIntervalSeries({{0, 1}, {2, 3}, {4, 5}});
    auto provider2 = buildIntervalPropertyProvider(intervals, IntervalProperty::Duration);

    std::vector<ColumnSource> columns;
    columns.push_back(ColumnSource{"values", std::move(provider1), {}});
    columns.push_back(ColumnSource{"duration", std::move(provider2), {}});

    auto row_desc = RowDescriptor::ordinal(3);
    // Should not throw — empty key is just skipped
    auto tensor = TensorData::createFromLazyColumns(3, std::move(columns), std::move(row_desc), wiring);

    // Columns should be accessible
    auto col1 = tensor.getColumn(0);
    auto col2 = tensor.getColumn(1);
    CHECK(col1.size() == 3);
    CHECK(col2.size() == 3);
}

// =============================================================================
// Integration: End-to-end lazy tensor from builders
// =============================================================================

TEST_CASE("Integration - build lazy tensor from interval reductions", "[TensorColumnBuilders]") {
    DataManager dm;

    // Source: linear analog 0..99
    dm.setData<AnalogTimeSeries>("signal", createLinearAnalog(100), TimeKey("time"));

    // Row intervals
    auto intervals = createIntervalSeries({{0, 9}, {10, 19}, {20, 29}});

    // Build mean column
    auto mean_pipeline = WhiskerToolbox::Transforms::V2::TransformPipeline();
    mean_pipeline.setRangeReductionErased("MeanValue", std::any{});
    auto mean_provider = buildIntervalPipelineProvider(
        dm, "signal", intervals, std::move(mean_pipeline));

    // Build max column
    auto max_pipeline = WhiskerToolbox::Transforms::V2::TransformPipeline();
    max_pipeline.setRangeReductionErased("MaxValue", std::any{});
    auto max_provider = buildIntervalPipelineProvider(
        dm, "signal", intervals, std::move(max_pipeline));

    // Build duration column
    auto duration_provider = buildIntervalPropertyProvider(
        intervals, IntervalProperty::Duration);

    // Assemble tensor
    std::vector<ColumnSource> columns;
    columns.push_back(ColumnSource{"mean", std::move(mean_provider), {}});
    columns.push_back(ColumnSource{"max", std::move(max_provider), {}});
    columns.push_back(ColumnSource{"duration", std::move(duration_provider), {}});

    auto wiring = buildInvalidationWiringFn(dm, {"signal", "signal", ""});
    auto row_desc = RowDescriptor::ordinal(3);
    auto tensor = TensorData::createFromLazyColumns(3, std::move(columns), std::move(row_desc), wiring);

    // Verify shape
    CHECK(tensor.numRows() == 3);
    CHECK(tensor.numColumns() == 3);

    // Verify mean column
    auto means = tensor.getColumn(0);
    // [0..9]: mean = 4.5
    CHECK_THAT(means[0], WithinAbs(4.5, 0.01));
    // [10..19]: mean = 14.5
    CHECK_THAT(means[1], WithinAbs(14.5, 0.01));
    // [20..29]: mean = 24.5
    CHECK_THAT(means[2], WithinAbs(24.5, 0.01));

    // Verify max column
    auto maxes = tensor.getColumn(1);
    CHECK_THAT(maxes[0], WithinAbs(9.0, 0.01));
    CHECK_THAT(maxes[1], WithinAbs(19.0, 0.01));
    CHECK_THAT(maxes[2], WithinAbs(29.0, 0.01));

    // Verify duration column
    auto durations = tensor.getColumn(2);
    CHECK_THAT(durations[0], WithinAbs(9.0, 0.01));  // 9 - 0
    CHECK_THAT(durations[1], WithinAbs(9.0, 0.01));   // 19 - 10
    CHECK_THAT(durations[2], WithinAbs(9.0, 0.01));   // 29 - 20
}

TEST_CASE("Integration - build lazy tensor from timestamp rows", "[TensorColumnBuilders]") {
    DataManager dm;
    dm.setData<AnalogTimeSeries>("signal", createLinearAnalog(100), TimeKey("time"));

    auto row_times = makeRowTimes({0, 25, 50, 75, 99});

    auto provider = buildPipelineColumnProvider(dm, "signal", row_times, TransformPipeline{});

    std::vector<ColumnSource> columns;
    columns.push_back(ColumnSource{"value", std::move(provider), {}});

    auto row_desc = RowDescriptor::ordinal(5);
    auto tensor = TensorData::createFromLazyColumns(5, std::move(columns), std::move(row_desc));

    CHECK(tensor.numRows() == 5);
    CHECK(tensor.numColumns() == 1);

    auto vals = tensor.getColumn(0);
    CHECK(vals[0] == 0.0f);
    CHECK(vals[1] == 25.0f);
    CHECK(vals[2] == 50.0f);
    CHECK(vals[3] == 75.0f);
    CHECK(vals[4] == 99.0f);
}

TEST_CASE("Integration - multiple columns added via appendColumn", "[TensorColumnBuilders]") {
    DataManager dm;
    dm.setData<AnalogTimeSeries>("signal", createLinearAnalog(50), TimeKey("time"));

    auto intervals = createIntervalSeries({{0, 9}, {10, 19}});

    // Start with just one column
    auto mean_pipeline = WhiskerToolbox::Transforms::V2::TransformPipeline();
    mean_pipeline.setRangeReductionErased("MeanValue", std::any{});
    auto mean_provider = buildIntervalPipelineProvider(
        dm, "signal", intervals, std::move(mean_pipeline));

    std::vector<ColumnSource> columns;
    columns.push_back(ColumnSource{"mean", std::move(mean_provider), {}});

    auto row_desc = RowDescriptor::ordinal(2);
    auto tensor = TensorData::createFromLazyColumns(2, std::move(columns), std::move(row_desc));

    CHECK(tensor.numColumns() == 1);

    // Append a duration column
    auto dur_provider = buildIntervalPropertyProvider(intervals, IntervalProperty::Duration);
    tensor.appendColumn("duration", std::move(dur_provider));

    CHECK(tensor.numColumns() == 2);

    auto means = tensor.getColumn(0);
    auto durations = tensor.getColumn(1);

    CHECK_THAT(means[0], WithinAbs(4.5, 0.01));
    CHECK_THAT(durations[0], WithinAbs(9.0, 0.01));
}

// ============================================================================
// Cross-TimeFrame Builder Tests (Phase 3.3)
// ============================================================================

TEST_CASE("buildIntervalPipelineProvider - cross-TimeFrame analog mean",
          "[TensorColumnBuilders][cross_timeframe]") {
    DataManager dm;

    // Interval TimeFrame: 10 Hz → times [0, 100, 200, ...]
    std::vector<int> interval_times;
    for (int i = 0; i < 20; ++i) {
        interval_times.push_back(i * 100);
    }
    auto interval_tf = std::make_shared<TimeFrame>(interval_times);
    dm.setTime(TimeKey("interval_clock"), interval_tf);

    // Source TimeFrame: 100 Hz → times [0, 10, 20, ...]
    std::vector<int> source_times;
    for (int i = 0; i < 200; ++i) {
        source_times.push_back(i * 10);
    }
    auto source_tf = std::make_shared<TimeFrame>(source_times);
    dm.setTime(TimeKey("source_clock"), source_tf);

    // Analog signal at 100 Hz: values [0, 1, 2, ..., 199]
    auto ats = createLinearAnalog(200);
    ats->setTimeFrame(source_tf);
    dm.setData<AnalogTimeSeries>("signal", ats, TimeKey("source_clock"));

    // Intervals at 10 Hz: [0,4] → [0ms, 400ms] → source [0, 40]
    auto intervals = createIntervalSeries({{0, 4}, {10, 14}});
    intervals->setTimeFrame(interval_tf);
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("interval_clock"));

    auto pipeline = WhiskerToolbox::Transforms::V2::TransformPipeline();
    pipeline.setRangeReductionErased("MeanValue", std::any{});

    auto provider = buildIntervalPipelineProvider(
            dm, "signal", intervals, std::move(pipeline));

    auto values = provider();
    REQUIRE(values.size() == 2);

    // [0,4] at 10Hz → [0ms,400ms] → source [0..40] → values ~[0..40], mean ≈ 20
    // [10,14] at 10Hz → [1000ms,1400ms] → source [100..140] → values ~[100..140], mean ≈ 120
    CHECK_THAT(values[0], WithinAbs(20.0, 1.0));
    CHECK_THAT(values[1], WithinAbs(120.0, 1.0));
}

TEST_CASE("buildIntervalPipelineProvider - cross-TimeFrame event count",
          "[TensorColumnBuilders][cross_timeframe]") {
    DataManager dm;

    std::vector<int> interval_times;
    for (int i = 0; i < 20; ++i) {
        interval_times.push_back(i * 100);
    }
    auto interval_tf = std::make_shared<TimeFrame>(interval_times);
    dm.setTime(TimeKey("interval_clock"), interval_tf);

    std::vector<int> source_times;
    for (int i = 0; i < 200; ++i) {
        source_times.push_back(i * 10);
    }
    auto source_tf = std::make_shared<TimeFrame>(source_times);
    dm.setTime(TimeKey("source_clock"), source_tf);

    // Events at source indices: 5, 15, 25, 105, 115
    auto des = createEventSeries({5, 15, 25, 105, 115});
    des->setTimeFrame(source_tf);
    dm.setData<DigitalEventSeries>("events", des, TimeKey("source_clock"));

    // Intervals at 10 Hz:
    // [0,4] → [0ms,400ms] → source [0,40] → events {5,15,25} → count=3
    // [10,14] → [1000ms,1400ms] → source [100,140] → events {105,115} → count=2
    auto intervals = createIntervalSeries({{0, 4}, {10, 14}});
    intervals->setTimeFrame(interval_tf);
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("interval_clock"));

    auto pipeline = WhiskerToolbox::Transforms::V2::TransformPipeline();
    pipeline.setRangeReductionErased("EventCount", std::any{});

    auto provider = buildIntervalPipelineProvider(
            dm, "events", intervals, std::move(pipeline));

    auto values = provider();
    REQUIRE(values.size() == 2);
    CHECK_THAT(values[0], WithinAbs(3.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(2.0, 0.01));
}

TEST_CASE("buildIntervalPipelineProvider - same TimeFrame produces same results",
          "[TensorColumnBuilders][cross_timeframe]") {
    DataManager dm;

    // Both use the same TimeFrame
    auto shared_tf = std::make_shared<TimeFrame>(
            std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
    dm.setTime(TimeKey("default"), shared_tf);

    auto ats = createLinearAnalog(10);
    ats->setTimeFrame(shared_tf);
    dm.setData<AnalogTimeSeries>("signal", ats, TimeKey("default"));

    auto intervals = createIntervalSeries({{0, 4}, {5, 9}});
    intervals->setTimeFrame(shared_tf);
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("default"));

    auto pipeline = WhiskerToolbox::Transforms::V2::TransformPipeline();
    pipeline.setRangeReductionErased("MeanValue", std::any{});

    auto provider = buildIntervalPipelineProvider(
            dm, "signal", intervals, std::move(pipeline));

    auto values = provider();
    REQUIRE(values.size() == 2);
    CHECK_THAT(values[0], WithinAbs(2.0, 0.01));
    CHECK_THAT(values[1], WithinAbs(7.0, 0.01));
}

TEST_CASE("Integration - cross-TimeFrame lazy tensor assembly",
          "[TensorColumnBuilders][cross_timeframe]") {
    DataManager dm;

    // Setup two different time bases
    std::vector<int> interval_times;
    for (int i = 0; i < 20; ++i) {
        interval_times.push_back(i * 100);
    }
    auto interval_tf = std::make_shared<TimeFrame>(interval_times);
    dm.setTime(TimeKey("interval_clock"), interval_tf);

    std::vector<int> source_times;
    for (int i = 0; i < 200; ++i) {
        source_times.push_back(i * 10);
    }
    auto source_tf = std::make_shared<TimeFrame>(source_times);
    dm.setTime(TimeKey("source_clock"), source_tf);

    // Analog signal at 100 Hz: values [0, 1, 2, ..., 199]
    auto ats = createLinearAnalog(200);
    ats->setTimeFrame(source_tf);
    dm.setData<AnalogTimeSeries>("signal", ats, TimeKey("source_clock"));

    // Intervals at 10 Hz
    auto intervals = createIntervalSeries({{0, 4}, {10, 14}});
    intervals->setTimeFrame(interval_tf);
    dm.setData<DigitalIntervalSeries>("intervals", intervals, TimeKey("interval_clock"));

    // Build two columns: mean and max, both with cross-TimeFrame
    auto mean_pipeline = WhiskerToolbox::Transforms::V2::TransformPipeline();
    mean_pipeline.setRangeReductionErased("MeanValue", std::any{});
    auto mean_provider = buildIntervalPipelineProvider(
            dm, "signal", intervals, std::move(mean_pipeline));

    auto max_pipeline = WhiskerToolbox::Transforms::V2::TransformPipeline();
    max_pipeline.setRangeReductionErased("MaxValue", std::any{});
    auto max_provider = buildIntervalPipelineProvider(
            dm, "signal", intervals, std::move(max_pipeline));

    // Also add interval duration column (no cross-TF needed for properties)
    auto dur_provider = buildIntervalPropertyProvider(intervals, IntervalProperty::Duration);

    std::vector<ColumnSource> columns;
    columns.push_back(ColumnSource{"mean", std::move(mean_provider), {}});
    columns.push_back(ColumnSource{"max", std::move(max_provider), {}});
    columns.push_back(ColumnSource{"duration", std::move(dur_provider), {}});

    auto row_desc = RowDescriptor::ordinal(2);
    auto tensor = TensorData::createFromLazyColumns(2, std::move(columns), std::move(row_desc));

    REQUIRE(tensor.numRows() == 2);
    REQUIRE(tensor.numColumns() == 3);

    auto means = tensor.getColumn(0);
    auto maxes = tensor.getColumn(1);
    auto durations = tensor.getColumn(2);

    // Row 0: interval [0,4] at 10Hz → [0ms,400ms] → source [0..40]
    CHECK_THAT(means[0], WithinAbs(20.0, 1.0));
    CHECK_THAT(maxes[0], WithinAbs(40.0, 1.0));
    CHECK_THAT(durations[0], WithinAbs(4.0, 0.01));  // duration in interval-TF indices

    // Row 1: interval [10,14] at 10Hz → [1000ms,1400ms] → source [100..140]
    CHECK_THAT(means[1], WithinAbs(120.0, 1.0));
    CHECK_THAT(maxes[1], WithinAbs(140.0, 1.0));
    CHECK_THAT(durations[1], WithinAbs(4.0, 0.01));
}
