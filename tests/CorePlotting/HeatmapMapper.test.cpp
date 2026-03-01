#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Mappers/HeatmapMapper.hpp"
#include "Colormaps/Colormap.hpp"

using namespace CorePlotting;

// =============================================================================
// Empty input
// =============================================================================

TEST_CASE("HeatmapMapper returns empty batch for empty input", "[HeatmapMapper]")
{
    auto colormap = Colormaps::getColormap(Colormaps::ColormapPreset::Inferno);
    std::vector<HeatmapRowData> rows;

    auto batch = HeatmapMapper::toRectangleBatch(rows, colormap);
    REQUIRE(batch.bounds.empty());
    REQUIRE(batch.colors.empty());
}

// =============================================================================
// Single row, single bin
// =============================================================================

TEST_CASE("HeatmapMapper single cell produces one rectangle", "[HeatmapMapper]")
{
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
    REQUIRE_THAT(static_cast<double>(b.z), Catch::Matchers::WithinAbs(100.0, 0.01)); // width
    REQUIRE_THAT(static_cast<double>(b.w), Catch::Matchers::WithinAbs(1.0, 0.01));  // height

    // With grayscale and auto range (max=1), value=1 → white
    auto const & c = batch.colors[0];
    REQUIRE_THAT(static_cast<double>(c.r), Catch::Matchers::WithinAbs(1.0, 0.01));
}

// =============================================================================
// Multiple rows
// =============================================================================

TEST_CASE("HeatmapMapper places rows at correct Y positions", "[HeatmapMapper]")
{
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

TEST_CASE("computeAutoRange finds correct range", "[HeatmapMapper]")
{
    std::vector<HeatmapRowData> rows;
    rows.push_back(HeatmapRowData{.values = {1.0, 5.0, 3.0}, .bin_start = 0.0, .bin_width = 1.0});
    rows.push_back(HeatmapRowData{.values = {0.0, 8.0, 2.0}, .bin_start = 0.0, .bin_width = 1.0});

    auto [vmin, vmax] = HeatmapMapper::computeAutoRange(rows);
    REQUIRE_THAT(static_cast<double>(vmin), Catch::Matchers::WithinAbs(0.0, 0.01));
    REQUIRE_THAT(static_cast<double>(vmax), Catch::Matchers::WithinAbs(8.0, 0.01));
}

TEST_CASE("computeAutoRange returns default for all-zero data", "[HeatmapMapper]")
{
    std::vector<HeatmapRowData> rows;
    rows.push_back(HeatmapRowData{.values = {0.0, 0.0}, .bin_start = 0.0, .bin_width = 1.0});

    auto [vmin, vmax] = HeatmapMapper::computeAutoRange(rows);
    REQUIRE_THAT(static_cast<double>(vmin), Catch::Matchers::WithinAbs(0.0, 0.01));
    REQUIRE_THAT(static_cast<double>(vmax), Catch::Matchers::WithinAbs(1.0, 0.01));
}

// =============================================================================
// Manual range
// =============================================================================

TEST_CASE("HeatmapMapper manual range clamps colors", "[HeatmapMapper]")
{
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

TEST_CASE("HeatmapMapper symmetric range centers at zero", "[HeatmapMapper]")
{
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

TEST_CASE("HeatmapMapper buildScene produces valid scene", "[HeatmapMapper]")
{
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
