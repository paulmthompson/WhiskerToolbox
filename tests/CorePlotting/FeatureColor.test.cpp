/**
 * @file FeatureColor.test.cpp
 * @brief Tests for the CorePlotting::FeatureColor shared library
 *
 * Tests cover:
 * - ResolveFeatureColors: ATS lookup, TensorData TFI hash join, TensorData ordinal, DIS containment
 * - ApplyFeatureColors: ContinuousMapping and ThresholdMapping application to scene glyphs
 * - FeatureColorRange: Auto, Manual, and Symmetric range computation
 * - FeatureColorCompatibility: Validation of source descriptors against DataManager
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "FeatureColor/ApplyFeatureColors.hpp"
#include "FeatureColor/FeatureColorCompatibility.hpp"
#include "FeatureColor/FeatureColorMapping.hpp"
#include "FeatureColor/FeatureColorRange.hpp"
#include "FeatureColor/FeatureColorSourceDescriptor.hpp"
#include "FeatureColor/PlotSourceRowType.hpp"
#include "FeatureColor/ResolveFeatureColors.hpp"

#include "SceneGraph/RenderablePrimitives.hpp"

#include "DataManager/DataManager.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

using namespace CorePlotting::FeatureColor;
using Catch::Matchers::WithinAbs;

// =============================================================================
// Test Helpers
// =============================================================================

namespace {

std::shared_ptr<AnalogTimeSeries> createTestATS(std::size_t count) {
    std::vector<float> data;
    std::vector<TimeFrameIndex> times;
    data.reserve(count);
    times.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        data.push_back(static_cast<float>(i) * 10.0f);
        times.push_back(TimeFrameIndex(static_cast<int64_t>(i)));
    }
    return std::make_shared<AnalogTimeSeries>(std::move(data), std::move(times));
}

std::shared_ptr<TensorData> createTestTensorTFI(
        std::size_t row_count,
        std::vector<std::string> column_names,
        std::vector<std::vector<float>> column_data) {

    auto const num_cols = column_names.size();
    auto tf = std::make_shared<TimeFrame>();

    auto time_storage = std::make_shared<DenseTimeIndexStorage>(
            TimeFrameIndex(0), row_count);

    // Build row-major flat data
    std::vector<float> flat;
    flat.reserve(row_count * num_cols);
    for (std::size_t r = 0; r < row_count; ++r) {
        for (std::size_t c = 0; c < num_cols; ++c) {
            flat.push_back(column_data[c][r]);
        }
    }

    auto tensor = TensorData::createTimeSeries2D(
            flat, row_count, num_cols, time_storage, tf, column_names);
    return std::make_shared<TensorData>(std::move(tensor));
}

std::shared_ptr<TensorData> createTestTensorOrdinal(
        std::size_t row_count,
        std::vector<std::string> column_names,
        std::vector<std::vector<float>> column_data) {

    auto const num_cols = column_names.size();

    std::vector<float> flat;
    flat.reserve(row_count * num_cols);
    for (std::size_t r = 0; r < row_count; ++r) {
        for (std::size_t c = 0; c < num_cols; ++c) {
            flat.push_back(column_data[c][r]);
        }
    }

    auto tensor = TensorData::createOrdinal2D(flat, row_count, num_cols, column_names);
    return std::make_shared<TensorData>(std::move(tensor));
}

std::shared_ptr<DigitalIntervalSeries> createTestDIS() {
    // Intervals: [10, 20), [30, 40)
    std::vector<Interval> intervals;
    intervals.push_back(Interval{int64_t{10}, int64_t{20}});
    intervals.push_back(Interval{int64_t{30}, int64_t{40}});
    return std::make_shared<DigitalIntervalSeries>(intervals);
}

CorePlotting::RenderableScene createTestScene(std::size_t point_count) {
    CorePlotting::RenderableScene scene;
    CorePlotting::RenderableGlyphBatch batch;

    for (std::size_t i = 0; i < point_count; ++i) {
        batch.positions.emplace_back(static_cast<float>(i), 0.0f);
        batch.colors.emplace_back(1.0f, 1.0f, 1.0f, 1.0f);// White default
        batch.entity_ids.push_back(EntityId{static_cast<uint64_t>(i)});
    }

    scene.glyph_batches.push_back(std::move(batch));
    return scene;
}

}// anonymous namespace

// =============================================================================
// resolveFeatureColors — AnalogTimeSeries
// =============================================================================

TEST_CASE("resolveFeatureColors from AnalogTimeSeries", "[FeatureColor]") {
    DataManager dm;
    auto ats = createTestATS(5);// values: 0, 10, 20, 30, 40
    dm.setData<AnalogTimeSeries>("velocity", ats, TimeKey("time"));

    FeatureColorSourceDescriptor desc;
    desc.data_key = "velocity";
    auto tf = dm.getTime();

    std::vector<TimeFrameIndex> point_times = {
            TimeFrameIndex(0), TimeFrameIndex(2), TimeFrameIndex(4)};

    auto result = resolveFeatureColors(dm, desc, point_times, tf);

    REQUIRE(result.size() == 3);
    REQUIRE(result[0].has_value());
    REQUIRE_THAT(*result[0], WithinAbs(0.0, 0.01));
    REQUIRE(result[1].has_value());
    REQUIRE_THAT(*result[1], WithinAbs(20.0, 0.01));
    REQUIRE(result[2].has_value());
    REQUIRE_THAT(*result[2], WithinAbs(40.0, 0.01));
}

TEST_CASE("resolveFeatureColors ATS missing time returns nullopt", "[FeatureColor]") {
    DataManager dm;
    auto ats = createTestATS(3);// values at 0,1,2
    dm.setData<AnalogTimeSeries>("vel", ats, TimeKey("time"));

    FeatureColorSourceDescriptor desc;
    desc.data_key = "vel";
    auto tf = dm.getTime();

    std::vector<TimeFrameIndex> point_times = {
            TimeFrameIndex(0), TimeFrameIndex(100)};// 100 doesn't exist

    auto result = resolveFeatureColors(dm, desc, point_times, tf);

    REQUIRE(result.size() == 2);
    REQUIRE(result[0].has_value());
    REQUIRE_FALSE(result[1].has_value());
}

// =============================================================================
// resolveFeatureColors — TensorData (TFI)
// =============================================================================

TEST_CASE("resolveFeatureColors from TensorData TFI by column name", "[FeatureColor]") {
    DataManager dm;
    // 3 rows at TFI 0,1,2; columns "alpha" and "beta"
    auto tensor = createTestTensorTFI(3, {"alpha", "beta"},
                                      {{1.0f, 2.0f, 3.0f}, {10.0f, 20.0f, 30.0f}});
    dm.setData<TensorData>("features", tensor, TimeKey("time"));

    FeatureColorSourceDescriptor desc;
    desc.data_key = "features";
    desc.tensor_column_name = "beta";
    auto tf = dm.getTime();

    std::vector<TimeFrameIndex> point_times = {
            TimeFrameIndex(0), TimeFrameIndex(2)};

    auto result = resolveFeatureColors(dm, desc, point_times, tf);

    REQUIRE(result.size() == 2);
    REQUIRE(result[0].has_value());
    REQUIRE_THAT(*result[0], WithinAbs(10.0, 0.01));
    REQUIRE(result[1].has_value());
    REQUIRE_THAT(*result[1], WithinAbs(30.0, 0.01));
}

TEST_CASE("resolveFeatureColors from TensorData TFI by column index", "[FeatureColor]") {
    DataManager dm;
    auto tensor = createTestTensorTFI(3, {"a", "b"},
                                      {{1.0f, 2.0f, 3.0f}, {100.0f, 200.0f, 300.0f}});
    dm.setData<TensorData>("t", tensor, TimeKey("time"));

    FeatureColorSourceDescriptor desc;
    desc.data_key = "t";
    desc.tensor_column_index = 1;
    auto tf = dm.getTime();

    std::vector<TimeFrameIndex> point_times = {TimeFrameIndex(1)};

    auto result = resolveFeatureColors(dm, desc, point_times, tf);

    REQUIRE(result.size() == 1);
    REQUIRE(result[0].has_value());
    REQUIRE_THAT(*result[0], WithinAbs(200.0, 0.01));
}

TEST_CASE("resolveFeatureColors TFI mismatch returns nullopt", "[FeatureColor]") {
    DataManager dm;
    auto tensor = createTestTensorTFI(2, {"v"}, {{5.0f, 6.0f}});
    dm.setData<TensorData>("t", tensor, TimeKey("time"));

    FeatureColorSourceDescriptor desc;
    desc.data_key = "t";
    desc.tensor_column_name = "v";
    auto tf = dm.getTime();

    std::vector<TimeFrameIndex> point_times = {TimeFrameIndex(99)};

    auto result = resolveFeatureColors(dm, desc, point_times, tf);

    REQUIRE(result.size() == 1);
    REQUIRE_FALSE(result[0].has_value());
}

// =============================================================================
// resolveFeatureColors — TensorData (Ordinal)
// =============================================================================

TEST_CASE("resolveFeatureColors from TensorData ordinal", "[FeatureColor]") {
    DataManager dm;
    auto tensor = createTestTensorOrdinal(3, {"score"}, {{10.0f, 20.0f, 30.0f}});
    dm.setData<TensorData>("ord", tensor, TimeKey("time"));

    FeatureColorSourceDescriptor desc;
    desc.data_key = "ord";
    desc.tensor_column_name = "score";
    auto tf = dm.getTime();

    // Ordinal join: row 0 → point 0, row 1 → point 1, row 2 → point 2
    std::vector<TimeFrameIndex> point_times = {
            TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3)};

    auto result = resolveFeatureColors(dm, desc, point_times, tf);

    REQUIRE(result.size() == 4);
    REQUIRE(result[0].has_value());
    REQUIRE_THAT(*result[0], WithinAbs(10.0, 0.01));
    REQUIRE(result[1].has_value());
    REQUIRE_THAT(*result[1], WithinAbs(20.0, 0.01));
    REQUIRE(result[2].has_value());
    REQUIRE_THAT(*result[2], WithinAbs(30.0, 0.01));
    REQUIRE_FALSE(result[3].has_value());// Beyond tensor rows
}

// =============================================================================
// resolveFeatureColors — DigitalIntervalSeries
// =============================================================================

TEST_CASE("resolveFeatureColors from DigitalIntervalSeries", "[FeatureColor]") {
    DataManager dm;
    auto dis = createTestDIS();// Intervals: [10,20), [30,40)
    auto tf = dm.getTime();
    dis->setTimeFrame(tf);
    dm.setData<DigitalIntervalSeries>("whisking", dis, TimeKey("time"));

    FeatureColorSourceDescriptor desc;
    desc.data_key = "whisking";

    std::vector<TimeFrameIndex> point_times = {
            TimeFrameIndex(5), // outside → 0.0
            TimeFrameIndex(15),// inside [10,20) → 1.0
            TimeFrameIndex(25),// outside → 0.0
            TimeFrameIndex(35) // inside [30,40) → 1.0
    };

    auto result = resolveFeatureColors(dm, desc, point_times, tf);

    REQUIRE(result.size() == 4);
    REQUIRE(result[0].has_value());
    REQUIRE_THAT(*result[0], WithinAbs(0.0, 0.01));
    REQUIRE(result[1].has_value());
    REQUIRE_THAT(*result[1], WithinAbs(1.0, 0.01));
    REQUIRE(result[2].has_value());
    REQUIRE_THAT(*result[2], WithinAbs(0.0, 0.01));
    REQUIRE(result[3].has_value());
    REQUIRE_THAT(*result[3], WithinAbs(1.0, 0.01));
}

// =============================================================================
// resolveFeatureColors — Key not found
// =============================================================================

TEST_CASE("resolveFeatureColors returns nullopt for missing key", "[FeatureColor]") {
    DataManager dm;
    auto tf = dm.getTime();

    FeatureColorSourceDescriptor desc;
    desc.data_key = "nonexistent";

    std::vector<TimeFrameIndex> point_times = {TimeFrameIndex(0)};
    auto result = resolveFeatureColors(dm, desc, point_times, tf);

    REQUIRE(result.size() == 1);
    REQUIRE_FALSE(result[0].has_value());
}

TEST_CASE("resolveFeatureColors empty point_times returns empty", "[FeatureColor]") {
    DataManager dm;
    auto tf = dm.getTime();

    FeatureColorSourceDescriptor desc;
    desc.data_key = "anything";

    auto result = resolveFeatureColors(dm, desc, std::span<TimeFrameIndex const>{}, tf);
    REQUIRE(result.empty());
}

// =============================================================================
// applyFeatureColorsToScene — ContinuousMapping
// =============================================================================

TEST_CASE("applyFeatureColorsToScene ContinuousMapping", "[FeatureColor]") {
    auto scene = createTestScene(3);

    std::vector<std::optional<float>> resolved = {0.0f, 0.5f, 1.0f};

    ContinuousMapping cm;
    cm.preset = CorePlotting::Colormaps::ColormapPreset::Grayscale;
    cm.vmin = 0.0f;
    cm.vmax = 1.0f;

    applyFeatureColorsToScene(scene, resolved, cm, 3);

    auto const & colors = scene.glyph_batches[0].colors;
    // Grayscale: 0.0 → black(0,0,0), 0.5 → gray(0.5,0.5,0.5), 1.0 → white(1,1,1)
    REQUIRE_THAT(static_cast<double>(colors[0].r), WithinAbs(0.0, 0.02));
    REQUIRE_THAT(static_cast<double>(colors[1].r), WithinAbs(0.5, 0.02));
    REQUIRE_THAT(static_cast<double>(colors[2].r), WithinAbs(1.0, 0.02));
}

TEST_CASE("applyFeatureColorsToScene skips nullopt points", "[FeatureColor]") {
    auto scene = createTestScene(3);

    // Point 1 has no resolved value — should keep its white default
    std::vector<std::optional<float>> resolved = {0.5f, std::nullopt, 0.5f};

    ContinuousMapping cm;
    cm.preset = CorePlotting::Colormaps::ColormapPreset::Grayscale;
    cm.vmin = 0.0f;
    cm.vmax = 1.0f;

    applyFeatureColorsToScene(scene, resolved, cm, 3);

    auto const & colors = scene.glyph_batches[0].colors;
    // Point 0 and 2 should be gray-ish, point 1 should remain white
    REQUIRE_THAT(static_cast<double>(colors[0].r), WithinAbs(0.5, 0.02));
    REQUIRE_THAT(static_cast<double>(colors[1].r), WithinAbs(1.0, 0.01));// Still white
    REQUIRE_THAT(static_cast<double>(colors[1].g), WithinAbs(1.0, 0.01));// Still white
    REQUIRE_THAT(static_cast<double>(colors[2].r), WithinAbs(0.5, 0.02));
}

TEST_CASE("applyFeatureColorsToScene skips skip_indices", "[FeatureColor]") {
    auto scene = createTestScene(3);

    std::vector<std::optional<float>> resolved = {0.0f, 0.0f, 0.0f};

    ContinuousMapping cm;
    cm.preset = CorePlotting::Colormaps::ColormapPreset::Grayscale;
    cm.vmin = 0.0f;
    cm.vmax = 1.0f;

    std::unordered_set<std::size_t> skip = {1};
    applyFeatureColorsToScene(scene, resolved, cm, 3, skip);

    auto const & colors = scene.glyph_batches[0].colors;
    // Point 0, 2: should be black (value 0.0 in grayscale)
    REQUIRE_THAT(static_cast<double>(colors[0].r), WithinAbs(0.0, 0.02));
    // Point 1: skipped — should remain white
    REQUIRE_THAT(static_cast<double>(colors[1].r), WithinAbs(1.0, 0.01));
    REQUIRE_THAT(static_cast<double>(colors[2].r), WithinAbs(0.0, 0.02));
}

// =============================================================================
// applyFeatureColorsToScene — ThresholdMapping
// =============================================================================

TEST_CASE("applyFeatureColorsToScene ThresholdMapping", "[FeatureColor]") {
    auto scene = createTestScene(4);

    std::vector<std::optional<float>> resolved = {0.0f, 1.0f, 0.3f, 0.7f};

    ThresholdMapping tm;
    tm.threshold = 0.5f;
    tm.above_color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);// Red
    tm.below_color = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);// Blue

    applyFeatureColorsToScene(scene, resolved, tm, 4);

    auto const & colors = scene.glyph_batches[0].colors;
    // 0.0 < 0.5 → blue
    REQUIRE_THAT(static_cast<double>(colors[0].b), WithinAbs(1.0, 0.01));
    // 1.0 >= 0.5 → red
    REQUIRE_THAT(static_cast<double>(colors[1].r), WithinAbs(1.0, 0.01));
    // 0.3 < 0.5 → blue
    REQUIRE_THAT(static_cast<double>(colors[2].b), WithinAbs(1.0, 0.01));
    // 0.7 >= 0.5 → red
    REQUIRE_THAT(static_cast<double>(colors[3].r), WithinAbs(1.0, 0.01));
}

// =============================================================================
// computeEffectiveColorRange
// =============================================================================

TEST_CASE("computeEffectiveColorRange Auto mode", "[FeatureColor]") {
    std::vector<std::optional<float>> vals = {1.0f, std::nullopt, 5.0f, 3.0f};

    ColorRangeConfig config;
    config.mode = ColorRangeMode::Auto;

    auto range = computeEffectiveColorRange(vals, config);
    REQUIRE(range.has_value());
    REQUIRE_THAT(static_cast<double>(range->first), WithinAbs(1.0, 0.01));
    REQUIRE_THAT(static_cast<double>(range->second), WithinAbs(5.0, 0.01));
}

TEST_CASE("computeEffectiveColorRange Manual mode", "[FeatureColor]") {
    std::vector<std::optional<float>> vals = {1.0f, 5.0f};

    ColorRangeConfig config;
    config.mode = ColorRangeMode::Manual;
    config.manual_vmin = -10.0f;
    config.manual_vmax = 10.0f;

    auto range = computeEffectiveColorRange(vals, config);
    REQUIRE(range.has_value());
    REQUIRE_THAT(static_cast<double>(range->first), WithinAbs(-10.0, 0.01));
    REQUIRE_THAT(static_cast<double>(range->second), WithinAbs(10.0, 0.01));
}

TEST_CASE("computeEffectiveColorRange Symmetric mode", "[FeatureColor]") {
    std::vector<std::optional<float>> vals = {-2.0f, 5.0f};

    ColorRangeConfig config;
    config.mode = ColorRangeMode::Symmetric;

    auto range = computeEffectiveColorRange(vals, config);
    REQUIRE(range.has_value());
    REQUIRE_THAT(static_cast<double>(range->first), WithinAbs(-5.0, 0.01));
    REQUIRE_THAT(static_cast<double>(range->second), WithinAbs(5.0, 0.01));
}

TEST_CASE("computeEffectiveColorRange all nullopt returns nullopt", "[FeatureColor]") {
    std::vector<std::optional<float>> vals = {std::nullopt, std::nullopt};

    ColorRangeConfig config;
    config.mode = ColorRangeMode::Auto;

    auto range = computeEffectiveColorRange(vals, config);
    REQUIRE_FALSE(range.has_value());
}

// =============================================================================
// checkFeatureColorCompatibility
// =============================================================================

TEST_CASE("checkFeatureColorCompatibility valid ATS", "[FeatureColor]") {
    DataManager dm;
    dm.setData<AnalogTimeSeries>("vel", createTestATS(5), TimeKey("time"));

    FeatureColorSourceDescriptor desc;
    desc.data_key = "vel";

    auto result = checkFeatureColorCompatibility(dm, desc,
                                                 PlotSourceRowType::AnalogTimeSeries, 5);
    REQUIRE(result.compatible);
    REQUIRE(result.reason.empty());
}

TEST_CASE("checkFeatureColorCompatibility valid TensorData", "[FeatureColor]") {
    DataManager dm;
    auto t = createTestTensorTFI(3, {"x"}, {{1.0f, 2.0f, 3.0f}});
    dm.setData<TensorData>("feat", t, TimeKey("time"));

    FeatureColorSourceDescriptor desc;
    desc.data_key = "feat";
    desc.tensor_column_name = "x";

    auto result = checkFeatureColorCompatibility(dm, desc,
                                                 PlotSourceRowType::TensorTimeFrameIndex, 3);
    REQUIRE(result.compatible);
}

TEST_CASE("checkFeatureColorCompatibility missing column", "[FeatureColor]") {
    DataManager dm;
    auto t = createTestTensorTFI(3, {"x"}, {{1.0f, 2.0f, 3.0f}});
    dm.setData<TensorData>("feat", t, TimeKey("time"));

    FeatureColorSourceDescriptor desc;
    desc.data_key = "feat";
    desc.tensor_column_name = "nonexistent";

    auto result = checkFeatureColorCompatibility(dm, desc,
                                                 PlotSourceRowType::TensorTimeFrameIndex, 3);
    REQUIRE_FALSE(result.compatible);
    REQUIRE(result.reason.find("nonexistent") != std::string::npos);
}

TEST_CASE("checkFeatureColorCompatibility missing key", "[FeatureColor]") {
    DataManager dm;

    FeatureColorSourceDescriptor desc;
    desc.data_key = "ghost";

    auto result = checkFeatureColorCompatibility(dm, desc,
                                                 PlotSourceRowType::AnalogTimeSeries, 0);
    REQUIRE_FALSE(result.compatible);
}

TEST_CASE("checkFeatureColorCompatibility empty key", "[FeatureColor]") {
    DataManager dm;

    FeatureColorSourceDescriptor desc;
    desc.data_key = "";

    auto result = checkFeatureColorCompatibility(dm, desc,
                                                 PlotSourceRowType::AnalogTimeSeries, 0);
    REQUIRE_FALSE(result.compatible);
}

TEST_CASE("checkFeatureColorCompatibility TensorData no column specified", "[FeatureColor]") {
    DataManager dm;
    auto t = createTestTensorTFI(3, {"x"}, {{1.0f, 2.0f, 3.0f}});
    dm.setData<TensorData>("feat", t, TimeKey("time"));

    FeatureColorSourceDescriptor desc;
    desc.data_key = "feat";
    // Neither tensor_column_name nor tensor_column_index set

    auto result = checkFeatureColorCompatibility(dm, desc,
                                                 PlotSourceRowType::TensorTimeFrameIndex, 3);
    REQUIRE_FALSE(result.compatible);
    REQUIRE(result.reason.find("column") != std::string::npos);
}

TEST_CASE("checkFeatureColorCompatibility valid DIS", "[FeatureColor]") {
    DataManager dm;
    auto dis = createTestDIS();
    dis->setTimeFrame(dm.getTime());
    dm.setData<DigitalIntervalSeries>("whisking", dis, TimeKey("time"));

    FeatureColorSourceDescriptor desc;
    desc.data_key = "whisking";

    auto result = checkFeatureColorCompatibility(dm, desc,
                                                 PlotSourceRowType::AnalogTimeSeries, 100);
    REQUIRE(result.compatible);
}
