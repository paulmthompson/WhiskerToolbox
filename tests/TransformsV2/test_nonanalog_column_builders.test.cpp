/**
 * @file test_nonanalog_column_builders.test.cpp
 * @brief Tests for generic pipeline column builders with non-Analog source types.
 *
 * Phase 3, Step 3.8 — Verifies that `buildPipelineColumnProvider()` (Pattern A)
 * works correctly with MaskData, LineData, and PointData sources via element-
 * transform pipelines.
 *
 * Pattern B (interval-row via `buildIntervalPipelineProvider()`) currently
 * does NOT support MaskData/LineData/PointData because:
 *   1. GatherPipelineExecutor.cpp does not include their headers, so
 *      HasDataTraits<T> evaluates to false (no DataTraits visible).
 *   2. Even with includes, GatherResult<T>::create() fails because the
 *      CopyableTimeRangeDataType concept requires createTimeRangeCopy()
 *      to return T, but RaggedTimeSeries<ElementT>::createTimeRangeCopy()
 *      returns the base class type, not the derived type.
 *   3. element_type_of<T> has no specialization for these types.
 *
 * Tests annotated [IntervalGap] document this limitation and verify runtime
 * errors are raised. When gather support is added for these types, those tests
 * should be updated to verify correct values.
 *
 * @see TensorColumnBuilders.hpp for API documentation
 * @see tensor_column_refactor_proposal.md for design rationale
 */

#include "TransformsV2/core/TensorColumnBuilders.hpp"

#include "DataManager/DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Tensors/TensorData.hpp"
#include "DataManager/Tensors/storage/LazyColumnTensorStorage.hpp"

#include "TransformsV2/core/TransformPipeline.hpp"
#include "TransformsV2/core/RangeReductionRegistry.hpp"
#include "TransformsV2/algorithms/MaskArea/MaskArea.hpp"
#include "TransformsV2/algorithms/LineLength/LineLength.hpp"

#include "Observer/Observer_Data.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <memory>
#include <vector>

using namespace WhiskerToolbox::TensorBuilders;
using WhiskerToolbox::Transforms::V2::TransformPipeline;
using WhiskerToolbox::Transforms::V2::Examples::MaskAreaParams;
using WhiskerToolbox::Transforms::V2::Examples::LineLengthParams;
using Catch::Matchers::WithinAbs;

// =============================================================================
// Test Helpers
// =============================================================================

namespace {

/**
 * @brief Create a MaskData with masks at given timestamps.
 *        Each mask is a square of side `size` starting at (0,0).
 *        Area = size * size pixels.
 */
std::shared_ptr<MaskData> createSquareMasks(
    std::vector<std::pair<int64_t, uint32_t>> const & time_size_pairs)
{
    auto masks = std::make_shared<MaskData>();
    for (auto const & [t, side] : time_size_pairs) {
        std::vector<Point2D<uint32_t>> pixels;
        for (uint32_t y = 0; y < side; ++y) {
            for (uint32_t x = 0; x < side; ++x) {
                pixels.push_back(Point2D<uint32_t>{x, y});
            }
        }
        masks->addAtTime(TimeFrameIndex(t), Mask2D(pixels), NotifyObservers::No);
    }
    return masks;
}

/**
 * @brief Create a LineData with horizontal lines at given timestamps.
 *        Each line goes from (0,0) to (length,0), so arc length = length.
 */
std::shared_ptr<LineData> createHorizontalLines(
    std::vector<std::pair<int64_t, float>> const & time_length_pairs)
{
    auto lines = std::make_shared<LineData>();
    for (auto const & [t, length] : time_length_pairs) {
        Line2D line({Point2D<float>(0.0f, 0.0f), Point2D<float>(length, 0.0f)});
        lines->addAtTime(TimeFrameIndex(t), line, NotifyObservers::No);
    }
    return lines;
}

/**
 * @brief Create a PointData with points at given timestamps.
 */
std::shared_ptr<PointData> createPoints(
    std::vector<std::pair<int64_t, Point2D<float>>> const & time_point_pairs)
{
    auto points = std::make_shared<PointData>();
    for (auto const & [t, pt] : time_point_pairs) {
        points->addAtTime(TimeFrameIndex(t), pt, NotifyObservers::No);
    }
    return points;
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
// MaskData — Pattern A (timestamp-row, CalculateMaskArea pipeline)
// =============================================================================

TEST_CASE("MaskData - timestamp-row via CalculateMaskArea pipeline",
          "[TensorColumnBuilders][NonAnalog]") {
    DataManager dm;
    // Masks at t=0 (2x2=4px), t=5 (3x3=9px), t=10 (5x5=25px)
    auto masks = createSquareMasks({{0, 2}, {5, 3}, {10, 5}});
    dm.setData<MaskData>("masks", masks, TimeKey("time"));

    auto row_times = makeRowTimes({0, 5, 10});

    TransformPipeline pipeline;
    pipeline.addStep("CalculateMaskArea", MaskAreaParams{});

    auto provider = buildPipelineColumnProvider(dm, "masks", row_times, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 3);
    CHECK_THAT(values[0], WithinAbs(4.0f, 0.01));
    CHECK_THAT(values[1], WithinAbs(9.0f, 0.01));
    CHECK_THAT(values[2], WithinAbs(25.0f, 0.01));
}

TEST_CASE("MaskData - timestamp-row with missing timestamps returns NaN",
          "[TensorColumnBuilders][NonAnalog]") {
    DataManager dm;
    auto masks = createSquareMasks({{0, 2}, {5, 3}});
    dm.setData<MaskData>("masks", masks, TimeKey("time"));

    // Request timestamp beyond source range
    auto row_times = makeRowTimes({0, 5, 999});

    TransformPipeline pipeline;
    pipeline.addStep("CalculateMaskArea", MaskAreaParams{});

    auto provider = buildPipelineColumnProvider(dm, "masks", row_times, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 3);
    CHECK_THAT(values[0], WithinAbs(4.0f, 0.01));
    CHECK_THAT(values[1], WithinAbs(9.0f, 0.01));
    CHECK(std::isnan(values[2]));
}

TEST_CASE("MaskData - empty pipeline rejected (non-float source)",
          "[TensorColumnBuilders][NonAnalog]") {
    DataManager dm;
    auto masks = createSquareMasks({{0, 2}});
    dm.setData<MaskData>("masks", masks, TimeKey("time"));

    auto row_times = makeRowTimes({0});

    // Empty pipeline on MaskData → pipelineProducesFloat returns false
    CHECK_THROWS_AS(
        buildPipelineColumnProvider(dm, "masks", row_times, TransformPipeline{}),
        std::runtime_error);
}

// =============================================================================
// MaskData — Pattern B (interval-row) — NOT YET SUPPORTED
//
// GatherResult<MaskData>::create() fails because CopyableTimeRangeDataType
// concept is not satisfied (createTimeRangeCopy returns RaggedTimeSeries<Mask2D>,
// not MaskData), and element_type_of<MaskData> has no specialization.
// These tests verify the runtime error.
// =============================================================================

TEST_CASE("MaskData - interval-row throws (gather not yet supported)",
          "[TensorColumnBuilders][NonAnalog][IntervalGap]") {
    DataManager dm;
    auto masks = createSquareMasks({{0, 1}, {1, 2}, {2, 3}});
    dm.setData<MaskData>("masks", masks, TimeKey("time"));

    auto intervals = createIntervalSeries({{0, 2}});

    TransformPipeline pipeline;
    pipeline.addStep("CalculateMaskArea", MaskAreaParams{});
    pipeline.setRangeReductionErased("MeanValue", std::any{});

    // Build succeeds (validation is type-index based), but materialization
    // throws because gatherAndExecutePipeline doesn't support MaskData.
    auto provider = buildIntervalPipelineProvider(dm, "masks", intervals, std::move(pipeline));
    CHECK_THROWS(provider());
}

// =============================================================================
// LineData — Pattern A (timestamp-row, CalculateLineLength pipeline)
// =============================================================================

TEST_CASE("LineData - timestamp-row via CalculateLineLength pipeline",
          "[TensorColumnBuilders][NonAnalog]") {
    DataManager dm;
    // Horizontal lines: t=0 (len=10), t=5 (len=20), t=10 (len=50)
    auto lines = createHorizontalLines({{0, 10.0f}, {5, 20.0f}, {10, 50.0f}});
    dm.setData<LineData>("lines", lines, TimeKey("time"));

    auto row_times = makeRowTimes({0, 5, 10});

    TransformPipeline pipeline;
    pipeline.addStep("CalculateLineLength", LineLengthParams{});

    auto provider = buildPipelineColumnProvider(dm, "lines", row_times, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 3);
    CHECK_THAT(values[0], WithinAbs(10.0f, 0.01));
    CHECK_THAT(values[1], WithinAbs(20.0f, 0.01));
    CHECK_THAT(values[2], WithinAbs(50.0f, 0.01));
}

TEST_CASE("LineData - timestamp-row with missing timestamps returns NaN",
          "[TensorColumnBuilders][NonAnalog]") {
    DataManager dm;
    auto lines = createHorizontalLines({{0, 10.0f}, {5, 20.0f}});
    dm.setData<LineData>("lines", lines, TimeKey("time"));

    auto row_times = makeRowTimes({0, 5, 999});

    TransformPipeline pipeline;
    pipeline.addStep("CalculateLineLength", LineLengthParams{});

    auto provider = buildPipelineColumnProvider(dm, "lines", row_times, std::move(pipeline));
    auto values = provider();

    REQUIRE(values.size() == 3);
    CHECK_THAT(values[0], WithinAbs(10.0f, 0.01));
    CHECK_THAT(values[1], WithinAbs(20.0f, 0.01));
    CHECK(std::isnan(values[2]));
}

TEST_CASE("LineData - empty pipeline rejected (non-float source)",
          "[TensorColumnBuilders][NonAnalog]") {
    DataManager dm;
    auto lines = createHorizontalLines({{0, 10.0f}});
    dm.setData<LineData>("lines", lines, TimeKey("time"));

    auto row_times = makeRowTimes({0});

    CHECK_THROWS_AS(
        buildPipelineColumnProvider(dm, "lines", row_times, TransformPipeline{}),
        std::runtime_error);
}

// =============================================================================
// LineData — Pattern B (interval-row) — NOT YET SUPPORTED
// =============================================================================

TEST_CASE("LineData - interval-row throws (gather not yet supported)",
          "[TensorColumnBuilders][NonAnalog][IntervalGap]") {
    DataManager dm;
    auto lines = createHorizontalLines(
        {{0, 10.0f}, {1, 20.0f}, {2, 30.0f}});
    dm.setData<LineData>("lines", lines, TimeKey("time"));

    auto intervals = createIntervalSeries({{0, 2}});

    TransformPipeline pipeline;
    pipeline.addStep("CalculateLineLength", LineLengthParams{});
    pipeline.setRangeReductionErased("MeanValue", std::any{});

    auto provider = buildIntervalPipelineProvider(dm, "lines", intervals, std::move(pipeline));
    CHECK_THROWS(provider());
}

// =============================================================================
// PointData — Validation Tests (no Point2D<float> → float transform exists)
// =============================================================================

TEST_CASE("PointData - empty pipeline rejected (non-float element type)",
          "[TensorColumnBuilders][NonAnalog]") {
    DataManager dm;
    auto points = createPoints({{0, Point2D<float>(1.0f, 2.0f)}});
    dm.setData<PointData>("points", points, TimeKey("time"));

    auto row_times = makeRowTimes({0});

    // Empty pipeline on PointData → pipelineProducesFloat returns false
    CHECK_THROWS_AS(
        buildPipelineColumnProvider(dm, "points", row_times, TransformPipeline{}),
        std::runtime_error);
}

TEST_CASE("PointData - interval-row throws (gather not supported)",
          "[TensorColumnBuilders][NonAnalog][IntervalGap]") {
    DataManager dm;
    auto points = createPoints({{0, Point2D<float>(1.0f, 2.0f)}});
    dm.setData<PointData>("points", points, TimeKey("time"));

    auto intervals = createIntervalSeries({{0, 1}});

    // MeanValue on PointData without a float-producing transform —
    // gather is not supported for PointData.
    TransformPipeline pipeline;
    pipeline.setRangeReductionErased("MeanValue", std::any{});

    auto provider = buildIntervalPipelineProvider(dm, "points", intervals, std::move(pipeline));
    CHECK_THROWS(provider());
}

// =============================================================================
// Integration — cross-type lazy tensor assembly
// =============================================================================

TEST_CASE("Integration - lazy tensor with MaskData and LineData columns",
          "[TensorColumnBuilders][NonAnalog]") {
    DataManager dm;

    // Mask data: areas 4, 9, 16 at timestamps 0, 1, 2
    auto masks = createSquareMasks({{0, 2}, {1, 3}, {2, 4}});
    dm.setData<MaskData>("masks", masks, TimeKey("time"));

    // Line data: lengths 10, 20, 30 at timestamps 0, 1, 2
    auto lines = createHorizontalLines({{0, 10.0f}, {1, 20.0f}, {2, 30.0f}});
    dm.setData<LineData>("lines", lines, TimeKey("time"));

    auto row_times = makeRowTimes({0, 1, 2});

    // Column 0: mask area
    TransformPipeline pipe_mask;
    pipe_mask.addStep("CalculateMaskArea", MaskAreaParams{});
    auto col0 = buildPipelineColumnProvider(dm, "masks", row_times, std::move(pipe_mask));

    // Column 1: line length
    TransformPipeline pipe_line;
    pipe_line.addStep("CalculateLineLength", LineLengthParams{});
    auto col1 = buildPipelineColumnProvider(dm, "lines", row_times, std::move(pipe_line));

    // Assemble lazy tensor
    std::vector<ColumnSource> columns;
    columns.push_back(ColumnSource{"mask_area", std::move(col0), {}});
    columns.push_back(ColumnSource{"line_length", std::move(col1), {}});

    auto tensor = TensorData::createFromLazyColumns(
        row_times.size(), std::move(columns), RowDescriptor::ordinal(row_times.size()));

    // Read column 0 (mask area)
    auto mask_col = tensor.getColumn(0);
    REQUIRE(mask_col.size() == 3);
    CHECK_THAT(mask_col[0], WithinAbs(4.0f, 0.01));
    CHECK_THAT(mask_col[1], WithinAbs(9.0f, 0.01));
    CHECK_THAT(mask_col[2], WithinAbs(16.0f, 0.01));

    // Read column 1 (line length)
    auto line_col = tensor.getColumn(1);
    REQUIRE(line_col.size() == 3);
    CHECK_THAT(line_col[0], WithinAbs(10.0f, 0.01));
    CHECK_THAT(line_col[1], WithinAbs(20.0f, 0.01));
    CHECK_THAT(line_col[2], WithinAbs(30.0f, 0.01));
}

// No interval-row integration test — gather not yet supported for MaskData/LineData.
// The timestamp-row integration test above demonstrates cross-type assembly.

// =============================================================================
// buildProviderFromRecipe — MaskData via JSON pipeline
// =============================================================================

TEST_CASE("MaskData - buildProviderFromRecipe with pipeline JSON",
          "[TensorColumnBuilders][NonAnalog]") {
    DataManager dm;
    auto masks = createSquareMasks({{0, 2}, {5, 3}, {10, 5}});
    dm.setData<MaskData>("masks", masks, TimeKey("time"));

    auto row_times = makeRowTimes({0, 5, 10});

    ColumnRecipe recipe;
    recipe.column_name = "mask_area";
    recipe.source_key = "masks";
    recipe.pipeline_json = R"({
        "steps": [
            {
                "step_id": "area",
                "transform_name": "CalculateMaskArea",
                "parameters": {}
            }
        ]
    })";

    auto provider = buildProviderFromRecipe(dm, recipe, row_times, nullptr);
    auto values = provider();

    REQUIRE(values.size() == 3);
    CHECK_THAT(values[0], WithinAbs(4.0f, 0.01));
    CHECK_THAT(values[1], WithinAbs(9.0f, 0.01));
    CHECK_THAT(values[2], WithinAbs(25.0f, 0.01));
}

TEST_CASE("LineData - buildProviderFromRecipe with pipeline JSON",
          "[TensorColumnBuilders][NonAnalog]") {
    DataManager dm;
    auto lines = createHorizontalLines({{0, 10.0f}, {5, 20.0f}, {10, 50.0f}});
    dm.setData<LineData>("lines", lines, TimeKey("time"));

    auto row_times = makeRowTimes({0, 5, 10});

    ColumnRecipe recipe;
    recipe.column_name = "line_length";
    recipe.source_key = "lines";
    recipe.pipeline_json = R"({
        "steps": [
            {
                "step_id": "length",
                "transform_name": "CalculateLineLength",
                "parameters": {}
            }
        ]
    })";

    auto provider = buildProviderFromRecipe(dm, recipe, row_times, nullptr);
    auto values = provider();

    REQUIRE(values.size() == 3);
    CHECK_THAT(values[0], WithinAbs(10.0f, 0.01));
    CHECK_THAT(values[1], WithinAbs(20.0f, 0.01));
    CHECK_THAT(values[2], WithinAbs(50.0f, 0.01));
}

TEST_CASE("MaskData - interval recipe via JSON throws (gather not supported)",
          "[TensorColumnBuilders][NonAnalog][IntervalGap]") {
    DataManager dm;
    auto masks = createSquareMasks({{0, 1}, {1, 2}, {2, 3}});
    dm.setData<MaskData>("masks", masks, TimeKey("time"));

    auto intervals = createIntervalSeries({{0, 2}});

    ColumnRecipe recipe;
    recipe.column_name = "mean_mask_area";
    recipe.source_key = "masks";
    recipe.pipeline_json = R"({
        "steps": [
            {
                "step_id": "area",
                "transform_name": "CalculateMaskArea",
                "parameters": {}
            }
        ],
        "range_reduction": {
            "reduction_name": "MeanValue"
        }
    })";

    auto provider = buildProviderFromRecipe(dm, recipe, {}, intervals);
    CHECK_THROWS(provider());
}
