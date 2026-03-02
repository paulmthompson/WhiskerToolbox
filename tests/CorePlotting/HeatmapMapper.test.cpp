#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Colormaps/Colormap.hpp"
#include "Mappers/HeatmapMapper.hpp"

#include <cmath>
#include <limits>

using namespace CorePlotting;

// =============================================================================
// Helper: check all elements in a glm::vec4 are finite
// =============================================================================

namespace {

bool isFinite(glm::vec4 const & v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::isfinite(v.w);
}

}// anonymous namespace

// =============================================================================
// Empty input
// =============================================================================

TEST_CASE("HeatmapMapper returns empty batch for empty input", "[HeatmapMapper]") {
    auto colormap = Colormaps::getColormap(Colormaps::ColormapPreset::Inferno);
    std::vector<HeatmapRowData> rows;

    auto batch = HeatmapMapper::toRectangleBatch(rows, colormap);
    REQUIRE(batch.bounds.empty());
    REQUIRE(batch.colors.empty());
}

// =============================================================================
// Single row, single bin
// =============================================================================

TEST_CASE("HeatmapMapper single cell produces one rectangle", "[HeatmapMapper]") {
    auto colormap = Colormaps::getColormap(Colormaps::ColormapPreset::Grayscale);

    std::vector<HeatmapRowData> rows;
    rows.push_back(HeatmapRowData{
            .values = {1.0},
            .bin_start = -50.0,
            .bin_width = 100.0,
    });

    auto batch = HeatmapMapper::toRectangleBatch(rows, colormap);
    REQUIRE(batch.bounds.size() == 1);
    REQUIRE(batch.colors.size() == 1);

    // Check rectangle geometry
    auto const & b = batch.bounds[0];
    REQUIRE_THAT(static_cast<double>(b.x), Catch::Matchers::WithinAbs(-50.0, 0.01));
    REQUIRE_THAT(static_cast<double>(b.y), Catch::Matchers::WithinAbs(0.0, 0.01));
    REQUIRE_THAT(static_cast<double>(b.z), Catch::Matchers::WithinAbs(100.0, 0.01));// width
    REQUIRE_THAT(static_cast<double>(b.w), Catch::Matchers::WithinAbs(1.0, 0.01));  // height

    // With grayscale and auto range (max=1), value=1 → white
    auto const & c = batch.colors[0];
    REQUIRE_THAT(static_cast<double>(c.r), Catch::Matchers::WithinAbs(1.0, 0.01));
}

// =============================================================================
// Multiple rows
// =============================================================================

TEST_CASE("HeatmapMapper places rows at correct Y positions", "[HeatmapMapper]") {
    auto colormap = Colormaps::getColormap(Colormaps::ColormapPreset::Grayscale);

    std::vector<HeatmapRowData> rows;
    // Two rows, each with 2 bins
    rows.push_back(HeatmapRowData{.values = {0.0, 1.0}, .bin_start = 0.0, .bin_width = 10.0});
    rows.push_back(HeatmapRowData{.values = {1.0, 0.0}, .bin_start = 0.0, .bin_width = 10.0});

    auto batch = HeatmapMapper::toRectangleBatch(rows, colormap);
    REQUIRE(batch.bounds.size() == 4);

    // Row 0 cells should have Y=0
    REQUIRE_THAT(static_cast<double>(batch.bounds[0].y), Catch::Matchers::WithinAbs(0.0, 0.01));
    REQUIRE_THAT(static_cast<double>(batch.bounds[1].y), Catch::Matchers::WithinAbs(0.0, 0.01));

    // Row 1 cells should have Y=1
    REQUIRE_THAT(static_cast<double>(batch.bounds[2].y), Catch::Matchers::WithinAbs(1.0, 0.01));
    REQUIRE_THAT(static_cast<double>(batch.bounds[3].y), Catch::Matchers::WithinAbs(1.0, 0.01));
}

// =============================================================================
// Auto range
// =============================================================================

TEST_CASE("computeAutoRange finds correct range", "[HeatmapMapper]") {
    std::vector<HeatmapRowData> rows;
    rows.push_back(HeatmapRowData{.values = {1.0, 5.0, 3.0}, .bin_start = 0.0, .bin_width = 1.0});
    rows.push_back(HeatmapRowData{.values = {0.0, 8.0, 2.0}, .bin_start = 0.0, .bin_width = 1.0});

    auto [vmin, vmax] = HeatmapMapper::computeAutoRange(rows);
    REQUIRE_THAT(static_cast<double>(vmin), Catch::Matchers::WithinAbs(0.0, 0.01));
    REQUIRE_THAT(static_cast<double>(vmax), Catch::Matchers::WithinAbs(8.0, 0.01));
}

TEST_CASE("computeAutoRange returns default for all-zero data", "[HeatmapMapper]") {
    std::vector<HeatmapRowData> rows;
    rows.push_back(HeatmapRowData{.values = {0.0, 0.0}, .bin_start = 0.0, .bin_width = 1.0});

    auto [vmin, vmax] = HeatmapMapper::computeAutoRange(rows);
    REQUIRE_THAT(static_cast<double>(vmin), Catch::Matchers::WithinAbs(0.0, 0.01));
    REQUIRE_THAT(static_cast<double>(vmax), Catch::Matchers::WithinAbs(1.0, 0.01));
}

// =============================================================================
// Manual range
// =============================================================================

TEST_CASE("HeatmapMapper manual range clamps colors", "[HeatmapMapper]") {
    auto colormap = Colormaps::getColormap(Colormaps::ColormapPreset::Grayscale);

    std::vector<HeatmapRowData> rows;
    rows.push_back(HeatmapRowData{
            .values = {5.0, 10.0, 15.0},
            .bin_start = 0.0,
            .bin_width = 1.0,
    });

    HeatmapColorRange range;
    range.mode = HeatmapColorRange::Mode::Manual;
    range.vmin = 0.0f;
    range.vmax = 10.0f;

    auto batch = HeatmapMapper::toRectangleBatch(rows, colormap, range);
    REQUIRE(batch.colors.size() == 3);

    // value=5.0 with range [0,10] → t=0.5 → gray
    REQUIRE_THAT(static_cast<double>(batch.colors[0].r),
                 Catch::Matchers::WithinAbs(0.5, 0.02));

    // value=10.0 → t=1.0 → white
    REQUIRE_THAT(static_cast<double>(batch.colors[1].r),
                 Catch::Matchers::WithinAbs(1.0, 0.01));

    // value=15.0 → t=1.5 clamped to 1.0 → white
    REQUIRE_THAT(static_cast<double>(batch.colors[2].r),
                 Catch::Matchers::WithinAbs(1.0, 0.01));
}

// =============================================================================
// Symmetric range
// =============================================================================

TEST_CASE("HeatmapMapper symmetric range centers at zero", "[HeatmapMapper]") {
    auto colormap = Colormaps::getColormap(Colormaps::ColormapPreset::Grayscale);

    std::vector<HeatmapRowData> rows;
    rows.push_back(HeatmapRowData{
            .values = {0.0, 5.0},
            .bin_start = 0.0,
            .bin_width = 1.0,
    });

    HeatmapColorRange range;
    range.mode = HeatmapColorRange::Mode::Symmetric;

    auto batch = HeatmapMapper::toRectangleBatch(rows, colormap, range);
    // Symmetric: vmin=-5, vmax=5
    // value=0 → t=0.5 → gray
    REQUIRE_THAT(static_cast<double>(batch.colors[0].r),
                 Catch::Matchers::WithinAbs(0.5, 0.02));
    // value=5 → t=1.0 → white
    REQUIRE_THAT(static_cast<double>(batch.colors[1].r),
                 Catch::Matchers::WithinAbs(1.0, 0.01));
}

// =============================================================================
// buildScene
// =============================================================================

TEST_CASE("HeatmapMapper buildScene produces valid scene", "[HeatmapMapper]") {
    auto colormap = Colormaps::getColormap(Colormaps::ColormapPreset::Inferno);

    std::vector<HeatmapRowData> rows;
    rows.push_back(HeatmapRowData{.values = {1.0, 2.0}, .bin_start = -10.0, .bin_width = 10.0});
    rows.push_back(HeatmapRowData{.values = {3.0, 4.0}, .bin_start = -10.0, .bin_width = 10.0});

    auto scene = HeatmapMapper::buildScene(rows, colormap);

    REQUIRE(scene.rectangle_batches.size() == 1);
    REQUIRE(scene.rectangle_batches[0].bounds.size() == 4);
    REQUIRE(scene.rectangle_batches[0].colors.size() == 4);

    // Identity matrices (widget overrides)
    REQUIRE(scene.view_matrix == glm::mat4{1.0f});
    REQUIRE(scene.projection_matrix == glm::mat4{1.0f});
}

// =============================================================================
// Single-key (single row) edge cases
// =============================================================================

TEST_CASE("HeatmapMapper single row with multiple bins", "[HeatmapMapper][SingleKey]") {
    auto colormap = Colormaps::getColormap(Colormaps::ColormapPreset::Inferno);

    std::vector<HeatmapRowData> rows;
    rows.push_back(HeatmapRowData{
            .values = {0.0, 5.0, 10.0, 5.0, 0.0},
            .bin_start = -50.0,
            .bin_width = 20.0,
    });

    auto batch = HeatmapMapper::toRectangleBatch(rows, colormap);
    REQUIRE(batch.bounds.size() == 5);
    REQUIRE(batch.colors.size() == 5);

    // All rectangles should be at Y=0 with height=1
    for (auto const & b: batch.bounds) {
        REQUIRE_THAT(static_cast<double>(b.y),
                     Catch::Matchers::WithinAbs(0.0, 0.01));
        REQUIRE_THAT(static_cast<double>(b.w),
                     Catch::Matchers::WithinAbs(1.0, 0.01));
        REQUIRE(isFinite(b));
    }

    // All colors should be finite (no NaN/inf)
    for (auto const & c: batch.colors) {
        REQUIRE(isFinite(c));
    }
}

TEST_CASE("HeatmapMapper single row all-zero values", "[HeatmapMapper][SingleKey]") {
    auto colormap = Colormaps::getColormap(Colormaps::ColormapPreset::Inferno);

    std::vector<HeatmapRowData> rows;
    rows.push_back(HeatmapRowData{
            .values = {0.0, 0.0, 0.0},
            .bin_start = -15.0,
            .bin_width = 10.0,
    });

    auto batch = HeatmapMapper::toRectangleBatch(rows, colormap);
    REQUIRE(batch.bounds.size() == 3);
    REQUIRE(batch.colors.size() == 3);

    // Auto range fallback: [0, 1], all values=0, t=0 => all same color
    for (auto const & c: batch.colors) {
        REQUIRE(isFinite(c));
    }
}

TEST_CASE("HeatmapMapper single row uniform non-zero values", "[HeatmapMapper][SingleKey]") {
    auto colormap = Colormaps::getColormap(Colormaps::ColormapPreset::Grayscale);

    std::vector<HeatmapRowData> rows;
    rows.push_back(HeatmapRowData{
            .values = {5.0, 5.0, 5.0},
            .bin_start = 0.0,
            .bin_width = 10.0,
    });

    auto batch = HeatmapMapper::toRectangleBatch(rows, colormap);
    REQUIRE(batch.bounds.size() == 3);

    // With auto range: all values=5, computeAutoRange should handle this
    // gracefully. All colors should be identical and finite.
    for (auto const & c: batch.colors) {
        REQUIRE(isFinite(c));
    }

    // All colors should be the same
    REQUIRE(batch.colors[0] == batch.colors[1]);
    REQUIRE(batch.colors[1] == batch.colors[2]);
}

TEST_CASE("HeatmapMapper single row with negative values (z-score)", "[HeatmapMapper][SingleKey]") {
    auto colormap = Colormaps::getColormap(Colormaps::ColormapPreset::Coolwarm);

    std::vector<HeatmapRowData> rows;
    rows.push_back(HeatmapRowData{
            .values = {-2.0, -1.0, 0.0, 1.0, 2.0},
            .bin_start = -50.0,
            .bin_width = 20.0,
    });

    // Test with Auto mode
    auto batch_auto = HeatmapMapper::toRectangleBatch(rows, colormap);
    REQUIRE(batch_auto.colors.size() == 5);
    for (auto const & c: batch_auto.colors) {
        REQUIRE(isFinite(c));
    }

    // Test with Symmetric mode (more appropriate for z-scores)
    HeatmapColorRange range;
    range.mode = HeatmapColorRange::Mode::Symmetric;
    auto batch_sym = HeatmapMapper::toRectangleBatch(rows, colormap, range);
    REQUIRE(batch_sym.colors.size() == 5);
    for (auto const & c: batch_sym.colors) {
        REQUIRE(isFinite(c));
    }
}

TEST_CASE("HeatmapMapper single row empty values", "[HeatmapMapper][SingleKey]") {
    auto colormap = Colormaps::getColormap(Colormaps::ColormapPreset::Inferno);

    std::vector<HeatmapRowData> rows;
    rows.push_back(HeatmapRowData{
            .values = {},
            .bin_start = 0.0,
            .bin_width = 10.0,
    });

    auto batch = HeatmapMapper::toRectangleBatch(rows, colormap);
    // Row has no values, so no rectangles should be produced
    REQUIRE(batch.bounds.empty());
    REQUIRE(batch.colors.empty());
}

TEST_CASE("computeAutoRange single row", "[HeatmapMapper][SingleKey][AutoRange]") {
    SECTION("single value") {
        std::vector<HeatmapRowData> rows;
        rows.push_back(HeatmapRowData{
                .values = {5.0},
                .bin_start = 0.0,
                .bin_width = 1.0});

        auto [vmin, vmax] = HeatmapMapper::computeAutoRange(rows);
        REQUIRE(std::isfinite(vmin));
        REQUIRE(std::isfinite(vmax));
        // Single non-negative value: range is [0, 5]
        REQUIRE_THAT(static_cast<double>(vmin),
                     Catch::Matchers::WithinAbs(0.0, 0.01));
        REQUIRE_THAT(static_cast<double>(vmax),
                     Catch::Matchers::WithinAbs(5.0, 0.01));
    }

    SECTION("all same non-zero value") {
        std::vector<HeatmapRowData> rows;
        rows.push_back(HeatmapRowData{
                .values = {3.0, 3.0, 3.0},
                .bin_start = 0.0,
                .bin_width = 1.0});

        auto [vmin, vmax] = HeatmapMapper::computeAutoRange(rows);
        REQUIRE(std::isfinite(vmin));
        REQUIRE(std::isfinite(vmax));
        // All same non-negative value: range is [0, 3]
        REQUIRE_THAT(static_cast<double>(vmin),
                     Catch::Matchers::WithinAbs(0.0, 0.01));
        REQUIRE_THAT(static_cast<double>(vmax),
                     Catch::Matchers::WithinAbs(3.0, 0.01));
    }

    SECTION("negative values only") {
        std::vector<HeatmapRowData> rows;
        rows.push_back(HeatmapRowData{
                .values = {-3.0, -1.0, -2.0},
                .bin_start = 0.0,
                .bin_width = 1.0});

        auto [vmin, vmax] = HeatmapMapper::computeAutoRange(rows);
        REQUIRE(std::isfinite(vmin));
        REQUIRE(std::isfinite(vmax));
        REQUIRE(vmin < vmax);
        // vmin should be negative
        REQUIRE(vmin < 0.0f);
    }

    SECTION("mixed positive and negative") {
        std::vector<HeatmapRowData> rows;
        rows.push_back(HeatmapRowData{
                .values = {-2.0, 0.0, 3.0},
                .bin_start = 0.0,
                .bin_width = 1.0});

        auto [vmin, vmax] = HeatmapMapper::computeAutoRange(rows);
        REQUIRE(std::isfinite(vmin));
        REQUIRE(std::isfinite(vmax));
        REQUIRE(vmin < vmax);
        REQUIRE(vmin < 0.0f);
        REQUIRE(vmax > 0.0f);
    }
}

TEST_CASE("HeatmapMapper buildScene single row", "[HeatmapMapper][SingleKey]") {
    auto colormap = Colormaps::getColormap(Colormaps::ColormapPreset::Inferno);

    std::vector<HeatmapRowData> rows;
    rows.push_back(HeatmapRowData{
            .values = {1.0, 2.0, 3.0},
            .bin_start = -15.0,
            .bin_width = 10.0,
    });

    auto scene = HeatmapMapper::buildScene(rows, colormap);

    REQUIRE(scene.rectangle_batches.size() == 1);
    REQUIRE(scene.rectangle_batches[0].bounds.size() == 3);
    REQUIRE(scene.rectangle_batches[0].colors.size() == 3);

    // All bounds and colors should be finite
    for (auto const & b: scene.rectangle_batches[0].bounds) {
        REQUIRE(isFinite(b));
    }
    for (auto const & c: scene.rectangle_batches[0].colors) {
        REQUIRE(isFinite(c));
    }
}
