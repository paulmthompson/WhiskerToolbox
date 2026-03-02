/**
 * @file HeatmapDataPipeline.test.cpp
 * @brief Integration tests for the heatmap data pipeline
 *
 * Exercises the full gather → estimate → scale → convert pipeline extracted
 * from HeatmapOpenGLWidget::rebuildScene(), with emphasis on the n=1 unit-key
 * case that has exhibited rendering failures in release builds.
 *
 * These tests run without any OpenGL context, verifying the data path only.
 * They can be run in release and debug to detect discrepancies.
 */

#include "Plots/HeatmapWidget/Core/HeatmapDataPipeline.hpp"

#include "CorePlotting/Colormaps/Colormap.hpp"
#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"
#include "CorePlotting/Mappers/HeatmapMapper.hpp"
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

// =============================================================================
// Helpers
// =============================================================================

namespace {

/**
 * @brief Check that a floating-point value is finite (not NaN or ±Inf)
 */
bool isFinite(float v) {
    return std::isfinite(v);
}

bool isFinite(double v) {
    return std::isfinite(v);
}

/**
 * @brief Create a 1:1 TimeFrame (index i → time i) with the given size
 */
std::shared_ptr<TimeFrame> createTimeFrame(int num_points) {
    std::vector<int> times;
    times.reserve(num_points);
    for (int i = 0; i < num_points; ++i) {
        times.push_back(i);
    }
    return std::make_shared<TimeFrame>(times);
}

/**
 * @brief Create a DigitalEventSeries from a vector of integer timestamps
 */
std::shared_ptr<DigitalEventSeries>
createEventSeries(std::vector<int> const & timestamps) {
    std::vector<TimeFrameIndex> events;
    events.reserve(timestamps.size());
    for (int const t: timestamps) {
        events.emplace_back(t);
    }
    return std::make_shared<DigitalEventSeries>(events);
}

/**
 * @brief Standard test fixture: one spike unit + one alignment event set
 *
 * Sets up:
 * - TimeFrame: 0..9999 (1:1)
 * - "spikes": events at 500, 510, 520, 530, 540
 * - "alignment": events at 500
 * - window_size: 100  (so bins cover [-50, 50) relative to alignment)
 *
 * Returns the DataManager; the keys "spikes" and "alignment" are registered.
 */
struct SingleUnitFixture {
    std::shared_ptr<DataManager> dm;
    PlotAlignmentData alignment_data;
    WhiskerToolbox::Plots::HeatmapPipelineConfig config;

    SingleUnitFixture() {
        dm = std::make_shared<DataManager>();

        auto tf = createTimeFrame(10000);
        dm->removeTime(TimeKey("time"));
        dm->setTime(TimeKey("time"), tf);

        // Spike data: several events around t=500
        auto spikes = createEventSeries({500, 510, 520, 530, 540});
        spikes->setTimeFrame(tf);
        dm->setData<DigitalEventSeries>("spikes", spikes, TimeKey("time"));

        // Alignment events: single alignment at t=500
        auto alignment = createEventSeries({500});
        alignment->setTimeFrame(tf);
        dm->setData<DigitalEventSeries>("alignment", alignment, TimeKey("time"));

        // Alignment configuration
        alignment_data.alignment_event_key = "alignment";
        alignment_data.window_size = 100.0;

        // Pipeline configuration
        config.window_size = 100.0;
        config.scaling = WhiskerToolbox::Plots::ScalingMode::RawCount;
        config.estimation_params = WhiskerToolbox::Plots::BinningParams{.bin_size = 10.0};
        config.time_units_per_second = 1000.0;
    }
};

/**
 * @brief Fixture with multiple alignment events (trials)
 */
struct MultiTrialFixture {
    std::shared_ptr<DataManager> dm;
    PlotAlignmentData alignment_data;
    WhiskerToolbox::Plots::HeatmapPipelineConfig config;

    MultiTrialFixture() {
        dm = std::make_shared<DataManager>();

        auto tf = createTimeFrame(10000);
        dm->removeTime(TimeKey("time"));
        dm->setTime(TimeKey("time"), tf);

        // Spike data: events around t=500 and t=2000
        auto spikes = createEventSeries({500, 510, 520, 2000, 2010, 2020});
        spikes->setTimeFrame(tf);
        dm->setData<DigitalEventSeries>("spikes", spikes, TimeKey("time"));

        // Alignment at both t=500 and t=2000
        auto alignment = createEventSeries({500, 2000});
        alignment->setTimeFrame(tf);
        dm->setData<DigitalEventSeries>("alignment", alignment, TimeKey("time"));

        alignment_data.alignment_event_key = "alignment";
        alignment_data.window_size = 100.0;

        config.window_size = 100.0;
        config.scaling = WhiskerToolbox::Plots::ScalingMode::RawCount;
        config.estimation_params = WhiskerToolbox::Plots::BinningParams{.bin_size = 10.0};
        config.time_units_per_second = 1000.0;
    }
};

}// anonymous namespace

// =============================================================================
// Data Pipeline Tests — single unit key (n=1)
// =============================================================================

TEST_CASE("runHeatmapPipeline single unit produces non-empty result",
          "[HeatmapDataPipeline][n1]") {
    SingleUnitFixture const f;
    std::vector<std::string> const unit_keys = {"spikes"};

    auto result = WhiskerToolbox::Plots::runHeatmapPipeline(
            f.dm, unit_keys, f.alignment_data, f.config);

    REQUIRE(result.success);
    REQUIRE(result.rows.size() == 1);
    REQUIRE_FALSE(result.rows[0].values.empty());
    REQUIRE(result.rows[0].bin_width > 0.0);
}

TEST_CASE("runHeatmapPipeline single unit values are finite",
          "[HeatmapDataPipeline][n1]") {
    SingleUnitFixture const f;
    std::vector<std::string> const unit_keys = {"spikes"};

    auto result = WhiskerToolbox::Plots::runHeatmapPipeline(
            f.dm, unit_keys, f.alignment_data, f.config);

    REQUIRE(result.success);
    for (auto const & row: result.rows) {
        for (double const v: row.values) {
            REQUIRE(isFinite(v));
        }
        REQUIRE(isFinite(row.bin_start));
        REQUIRE(isFinite(row.bin_width));
    }
}

TEST_CASE("runHeatmapPipeline single unit has non-zero spike counts",
          "[HeatmapDataPipeline][n1]") {
    SingleUnitFixture const f;
    std::vector<std::string> const unit_keys = {"spikes"};

    auto result = WhiskerToolbox::Plots::runHeatmapPipeline(
            f.dm, unit_keys, f.alignment_data, f.config);

    REQUIRE(result.success);
    REQUIRE(result.rows.size() == 1);

    // At least some bins should have non-zero counts
    double const total = std::accumulate(
            result.rows[0].values.begin(),
            result.rows[0].values.end(), 0.0);
    REQUIRE(total > 0.0);
}

TEST_CASE("runHeatmapPipeline single unit rate estimates match",
          "[HeatmapDataPipeline][n1]") {
    SingleUnitFixture const f;
    std::vector<std::string> const unit_keys = {"spikes"};

    auto result = WhiskerToolbox::Plots::runHeatmapPipeline(
            f.dm, unit_keys, f.alignment_data, f.config);

    REQUIRE(result.success);
    REQUIRE(result.rate_estimates.size() == 1);
    REQUIRE_FALSE(result.rate_estimates[0].times.empty());
    REQUIRE(result.rate_estimates[0].times.size() ==
            result.rate_estimates[0].values.size());
    REQUIRE(result.rate_estimates[0].num_trials >= 1);
}

TEST_CASE("runHeatmapPipeline single unit with FiringRateHz scaling",
          "[HeatmapDataPipeline][n1]") {
    SingleUnitFixture f;
    f.config.scaling = WhiskerToolbox::Plots::ScalingMode::FiringRateHz;
    std::vector<std::string> const unit_keys = {"spikes"};

    auto result = WhiskerToolbox::Plots::runHeatmapPipeline(
            f.dm, unit_keys, f.alignment_data, f.config);

    REQUIRE(result.success);
    REQUIRE(result.rows.size() == 1);
    for (double const v: result.rows[0].values) {
        REQUIRE(isFinite(v));
        REQUIRE(v >= 0.0);
    }
}

TEST_CASE("runHeatmapPipeline single unit with ZScore scaling",
          "[HeatmapDataPipeline][n1]") {
    SingleUnitFixture f;
    f.config.scaling = WhiskerToolbox::Plots::ScalingMode::ZScore;
    std::vector<std::string> const unit_keys = {"spikes"};

    auto result = WhiskerToolbox::Plots::runHeatmapPipeline(
            f.dm, unit_keys, f.alignment_data, f.config);

    REQUIRE(result.success);
    REQUIRE(result.rows.size() == 1);
    for (double const v: result.rows[0].values) {
        REQUIRE(isFinite(v));
    }
}

// =============================================================================
// Mapper Integration Tests — verify rectangles are produced from n=1 pipeline
// =============================================================================

TEST_CASE("buildScene from single-unit pipeline produces rectangles",
          "[HeatmapDataPipeline][Mapper][n1]") {
    SingleUnitFixture const f;
    std::vector<std::string> const unit_keys = {"spikes"};

    auto result = WhiskerToolbox::Plots::runHeatmapPipeline(
            f.dm, unit_keys, f.alignment_data, f.config);
    REQUIRE(result.success);

    auto colormap = CorePlotting::Colormaps::getColormap(
            CorePlotting::Colormaps::ColormapPreset::Inferno);

    CorePlotting::HeatmapColorRange color_range;
    color_range.mode = CorePlotting::HeatmapColorRange::Mode::Auto;

    auto scene = CorePlotting::HeatmapMapper::buildScene(
            result.rows, colormap, color_range);

    REQUIRE_FALSE(scene.rectangle_batches.empty());
    REQUIRE_FALSE(scene.rectangle_batches[0].bounds.empty());

    // Verify every rectangle has finite bounds and color
    // bounds is glm::vec4: {x, y, width, height} = {.x, .y, .z, .w}
    for (auto const & rect: scene.rectangle_batches[0].bounds) {
        REQUIRE(isFinite(rect.x));
        REQUIRE(isFinite(rect.y));
        REQUIRE(isFinite(rect.z));
        REQUIRE(isFinite(rect.w));
        REQUIRE(rect.z > 0.0f);// width > 0
        REQUIRE(rect.w > 0.0f);// height > 0
    }
    for (auto const & color: scene.rectangle_batches[0].colors) {
        REQUIRE(isFinite(color.r));
        REQUIRE(isFinite(color.g));
        REQUIRE(isFinite(color.b));
        REQUIRE(isFinite(color.a));
    }
}

TEST_CASE("buildScene single-unit rectangles occupy correct Y range",
          "[HeatmapDataPipeline][Mapper][n1]") {
    SingleUnitFixture const f;
    std::vector<std::string> const unit_keys = {"spikes"};

    auto result = WhiskerToolbox::Plots::runHeatmapPipeline(
            f.dm, unit_keys, f.alignment_data, f.config);
    REQUIRE(result.success);

    auto colormap = CorePlotting::Colormaps::getColormap(
            CorePlotting::Colormaps::ColormapPreset::Inferno);
    CorePlotting::HeatmapColorRange color_range;
    color_range.mode = CorePlotting::HeatmapColorRange::Mode::Auto;

    auto scene = CorePlotting::HeatmapMapper::buildScene(
            result.rows, colormap, color_range);

    REQUIRE_FALSE(scene.rectangle_batches[0].bounds.empty());

    // With n=1, single row should be at Y=0 with height=1
    // bounds is glm::vec4: {x, y, width, height} = {.x, .y, .z, .w}
    for (auto const & rect: scene.rectangle_batches[0].bounds) {
        REQUIRE(rect.y == 0.0f);
        REQUIRE(rect.w == 1.0f);// height
    }
}

TEST_CASE("buildScene single-unit rectangle X positions within window",
          "[HeatmapDataPipeline][Mapper][n1]") {
    SingleUnitFixture const f;
    std::vector<std::string> const unit_keys = {"spikes"};

    auto result = WhiskerToolbox::Plots::runHeatmapPipeline(
            f.dm, unit_keys, f.alignment_data, f.config);
    REQUIRE(result.success);

    auto colormap = CorePlotting::Colormaps::getColormap(
            CorePlotting::Colormaps::ColormapPreset::Inferno);
    CorePlotting::HeatmapColorRange color_range;
    color_range.mode = CorePlotting::HeatmapColorRange::Mode::Auto;

    auto scene = CorePlotting::HeatmapMapper::buildScene(
            result.rows, colormap, color_range);

    REQUIRE_FALSE(scene.rectangle_batches[0].bounds.empty());

    float const half_window = static_cast<float>(f.config.window_size) / 2.0f;

    // bounds is glm::vec4: {x, y, width, height} = {.x, .y, .z, .w}
    for (auto const & rect: scene.rectangle_batches[0].bounds) {
        // All rectangles should start within [-half_window, half_window]
        REQUIRE(rect.x >= -half_window);
        REQUIRE(rect.x + rect.z <= half_window + 1.0f);// small tolerance
    }
}

// =============================================================================
// Multi-unit tests (n>1 for comparison)
// =============================================================================

TEST_CASE("runHeatmapPipeline two units produces two rows",
          "[HeatmapDataPipeline]") {
    SingleUnitFixture f;

    // Add a second spike unit
    auto tf = f.dm->getTime(TimeKey("time"));
    auto spikes2 = createEventSeries({600, 610, 620});
    spikes2->setTimeFrame(tf);
    f.dm->setData<DigitalEventSeries>("spikes2", spikes2, TimeKey("time"));

    // Adjust alignment window to cover both units' spikes
    f.alignment_data.window_size = 200.0;
    f.config.window_size = 200.0;

    std::vector<std::string> const unit_keys = {"spikes", "spikes2"};

    auto result = WhiskerToolbox::Plots::runHeatmapPipeline(
            f.dm, unit_keys, f.alignment_data, f.config);

    REQUIRE(result.success);
    REQUIRE(result.rows.size() == 2);
    for (auto const & row: result.rows) {
        REQUIRE_FALSE(row.values.empty());
        REQUIRE(row.bin_width > 0.0);
    }
}

// =============================================================================
// Multi-trial tests (multiple alignment events)
// =============================================================================

TEST_CASE("runHeatmapPipeline multi-trial single unit",
          "[HeatmapDataPipeline][n1]") {
    MultiTrialFixture const f;
    std::vector<std::string> const unit_keys = {"spikes"};

    auto result = WhiskerToolbox::Plots::runHeatmapPipeline(
            f.dm, unit_keys, f.alignment_data, f.config);

    REQUIRE(result.success);
    REQUIRE(result.rows.size() == 1);
    REQUIRE(result.rate_estimates[0].num_trials == 2);

    // Total count should be 6 (3 spikes per trial × 2 trials)
    double const total = std::accumulate(
            result.rows[0].values.begin(),
            result.rows[0].values.end(), 0.0);
    REQUIRE(total == 6.0);
}

// =============================================================================
// Edge cases
// =============================================================================

TEST_CASE("runHeatmapPipeline empty unit keys returns failure",
          "[HeatmapDataPipeline][edge]") {
    SingleUnitFixture const f;
    std::vector<std::string> const unit_keys;

    auto result = WhiskerToolbox::Plots::runHeatmapPipeline(
            f.dm, unit_keys, f.alignment_data, f.config);

    REQUIRE_FALSE(result.success);
    REQUIRE(result.rows.empty());
}

TEST_CASE("runHeatmapPipeline missing alignment key returns failure",
          "[HeatmapDataPipeline][edge]") {
    SingleUnitFixture f;
    f.alignment_data.alignment_event_key = "nonexistent";
    std::vector<std::string> const unit_keys = {"spikes"};

    auto result = WhiskerToolbox::Plots::runHeatmapPipeline(
            f.dm, unit_keys, f.alignment_data, f.config);

    REQUIRE_FALSE(result.success);
    REQUIRE(result.rows.empty());
}

TEST_CASE("runHeatmapPipeline zero window size returns failure",
          "[HeatmapDataPipeline][edge]") {
    SingleUnitFixture f;
    f.config.window_size = 0.0;
    std::vector<std::string> const unit_keys = {"spikes"};

    auto result = WhiskerToolbox::Plots::runHeatmapPipeline(
            f.dm, unit_keys, f.alignment_data, f.config);

    REQUIRE_FALSE(result.success);
}

TEST_CASE("runHeatmapPipeline null data manager returns failure",
          "[HeatmapDataPipeline][edge]") {
    SingleUnitFixture const f;
    std::vector<std::string> const unit_keys = {"spikes"};

    auto result = WhiskerToolbox::Plots::runHeatmapPipeline(
            nullptr, unit_keys, f.alignment_data, f.config);

    REQUIRE_FALSE(result.success);
}

TEST_CASE("runHeatmapPipeline bin width matches configuration",
          "[HeatmapDataPipeline][n1]") {
    SingleUnitFixture f;
    std::vector<std::string> const unit_keys = {"spikes"};

    auto result = WhiskerToolbox::Plots::runHeatmapPipeline(
            f.dm, unit_keys, f.alignment_data, f.config);

    REQUIRE(result.success);
    auto const & params = std::get<WhiskerToolbox::Plots::BinningParams>(
            f.config.estimation_params);
    REQUIRE(result.rows[0].bin_width == params.bin_size);
}

TEST_CASE("runHeatmapPipeline number of bins matches window/bin_size",
          "[HeatmapDataPipeline][n1]") {
    SingleUnitFixture f;
    std::vector<std::string> const unit_keys = {"spikes"};

    auto result = WhiskerToolbox::Plots::runHeatmapPipeline(
            f.dm, unit_keys, f.alignment_data, f.config);

    REQUIRE(result.success);

    auto const & params = std::get<WhiskerToolbox::Plots::BinningParams>(
            f.config.estimation_params);
    int const expected_bins =
            static_cast<int>(std::ceil(f.config.window_size / params.bin_size));
    REQUIRE(result.rows[0].values.size() == static_cast<size_t>(expected_bins));
}

TEST_CASE("runHeatmapPipeline single unit no spikes in window",
          "[HeatmapDataPipeline][n1][edge]") {
    // Spikes exist but none fall within the alignment window
    auto dm = std::make_shared<DataManager>();
    auto tf = createTimeFrame(10000);
    dm->removeTime(TimeKey("time"));
    dm->setTime(TimeKey("time"), tf);

    // Spikes far from alignment
    auto spikes = createEventSeries({100, 110, 120});
    spikes->setTimeFrame(tf);
    dm->setData<DigitalEventSeries>("spikes", spikes, TimeKey("time"));

    auto alignment = createEventSeries({5000});
    alignment->setTimeFrame(tf);
    dm->setData<DigitalEventSeries>("alignment", alignment, TimeKey("time"));

    PlotAlignmentData alignment_data;
    alignment_data.alignment_event_key = "alignment";
    alignment_data.window_size = 100.0;// window [4950, 5050] — no spikes

    WhiskerToolbox::Plots::HeatmapPipelineConfig config;
    config.window_size = 100.0;
    config.scaling = WhiskerToolbox::Plots::ScalingMode::RawCount;
    config.estimation_params = WhiskerToolbox::Plots::BinningParams{.bin_size = 10.0};

    std::vector<std::string> const unit_keys = {"spikes"};
    auto result = WhiskerToolbox::Plots::runHeatmapPipeline(
            dm, unit_keys, alignment_data, config);

    // Pipeline should still succeed — just with all-zero bins
    REQUIRE(result.success);
    REQUIRE(result.rows.size() == 1);
    double const total = std::accumulate(
            result.rows[0].values.begin(),
            result.rows[0].values.end(), 0.0);
    REQUIRE(total == 0.0);
}
