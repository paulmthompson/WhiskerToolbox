#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/DataTypes/HistogramData.hpp"
#include "CorePlotting/Mappers/HistogramMapper.hpp"

using namespace CorePlotting;
using Catch::Matchers::WithinAbs;

// ============================================================================
// HistogramData tests
// ============================================================================

TEST_CASE("HistogramData accessors", "[CorePlotting][HistogramData]")
{
    HistogramData data;
    data.bin_start = -100.0;
    data.bin_width = 20.0;
    data.counts = {1.0, 3.0, 7.0, 2.0, 0.0};

    SECTION("numBins") {
        REQUIRE(data.numBins() == 5);
    }

    SECTION("binLeft") {
        REQUIRE_THAT(data.binLeft(0), WithinAbs(-100.0, 1e-9));
        REQUIRE_THAT(data.binLeft(2), WithinAbs(-60.0, 1e-9));
        REQUIRE_THAT(data.binLeft(4), WithinAbs(-20.0, 1e-9));
    }

    SECTION("binCenter") {
        REQUIRE_THAT(data.binCenter(0), WithinAbs(-90.0, 1e-9));
        REQUIRE_THAT(data.binCenter(2), WithinAbs(-50.0, 1e-9));
    }

    SECTION("binRight") {
        REQUIRE_THAT(data.binRight(0), WithinAbs(-80.0, 1e-9));
        REQUIRE_THAT(data.binRight(4), WithinAbs(0.0, 1e-9));
    }

    SECTION("binEnd") {
        REQUIRE_THAT(data.binEnd(), WithinAbs(0.0, 1e-9));
    }

    SECTION("maxCount") {
        REQUIRE_THAT(data.maxCount(), WithinAbs(7.0, 1e-9));
    }

    SECTION("empty histogram") {
        HistogramData empty;
        REQUIRE(empty.numBins() == 0);
        REQUIRE_THAT(empty.maxCount(), WithinAbs(0.0, 1e-9));
    }
}

// ============================================================================
// HistogramMapper::toBars tests
// ============================================================================

TEST_CASE("HistogramMapper::toBars", "[CorePlotting][HistogramMapper]")
{
    HistogramData data;
    data.bin_start = 0.0;
    data.bin_width = 10.0;
    data.counts = {5.0, 0.0, 3.0};

    HistogramStyle style;
    style.fill_color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.bar_gap_fraction = 0.0f; // No gap for easier testing

    SECTION("basic bar generation") {
        auto batch = HistogramMapper::toBars(data, style);

        // Zero-height bins are skipped, so 2 bars (bins 0 and 2)
        REQUIRE(batch.bounds.size() == 2);
        REQUIRE(batch.colors.size() == 2);

        // First bar: bin 0, x=0, y=0, w=10, h=5
        REQUIRE_THAT(batch.bounds[0].x, WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(batch.bounds[0].y, WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(batch.bounds[0].z, WithinAbs(10.0f, 1e-6f));  // width
        REQUIRE_THAT(batch.bounds[0].w, WithinAbs(5.0f, 1e-6f));   // height

        // Second bar: bin 2, x=20, y=0, w=10, h=3
        REQUIRE_THAT(batch.bounds[1].x, WithinAbs(20.0f, 1e-6f));
        REQUIRE_THAT(batch.bounds[1].w, WithinAbs(3.0f, 1e-6f));

        // Colors match
        REQUIRE(batch.colors[0] == style.fill_color);
    }

    SECTION("bar gap fraction") {
        style.bar_gap_fraction = 0.1f; // 10% gap on each side
        auto batch = HistogramMapper::toBars(data, style);

        // First bar: gap = 10 * 0.1 = 1.0, so x=1.0, width=8.0
        REQUIRE_THAT(batch.bounds[0].x, WithinAbs(1.0f, 1e-6f));
        REQUIRE_THAT(batch.bounds[0].z, WithinAbs(8.0f, 1e-6f));
    }

    SECTION("empty histogram produces empty batch") {
        HistogramData empty;
        auto batch = HistogramMapper::toBars(empty, style);
        REQUIRE(batch.bounds.empty());
    }
}

// ============================================================================
// HistogramMapper::toLine tests
// ============================================================================

TEST_CASE("HistogramMapper::toLine", "[CorePlotting][HistogramMapper]")
{
    HistogramData data;
    data.bin_start = 0.0;
    data.bin_width = 10.0;
    data.counts = {5.0, 3.0};

    HistogramStyle style;
    style.line_color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
    style.line_thickness = 3.0f;

    SECTION("step-function polyline") {
        auto batch = HistogramMapper::toLine(data, style);

        REQUIRE(batch.line_start_indices.size() == 1);
        REQUIRE(batch.line_vertex_counts.size() == 1);

        // Vertices: (0,0) (0,5) (10,5) (10,3) (20,3) (20,0) = 6 vertices = 12 floats
        REQUIRE(batch.vertices.size() == 12);

        // Starting point (0, 0)
        REQUIRE_THAT(batch.vertices[0], WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(batch.vertices[1], WithinAbs(0.0f, 1e-6f));

        // First bin left edge (0, 5)
        REQUIRE_THAT(batch.vertices[2], WithinAbs(0.0f, 1e-6f));
        REQUIRE_THAT(batch.vertices[3], WithinAbs(5.0f, 1e-6f));

        // First bin right edge (10, 5)
        REQUIRE_THAT(batch.vertices[4], WithinAbs(10.0f, 1e-6f));
        REQUIRE_THAT(batch.vertices[5], WithinAbs(5.0f, 1e-6f));

        // Second bin left edge (10, 3)
        REQUIRE_THAT(batch.vertices[6], WithinAbs(10.0f, 1e-6f));
        REQUIRE_THAT(batch.vertices[7], WithinAbs(3.0f, 1e-6f));

        // Second bin right edge (20, 3)
        REQUIRE_THAT(batch.vertices[8], WithinAbs(20.0f, 1e-6f));
        REQUIRE_THAT(batch.vertices[9], WithinAbs(3.0f, 1e-6f));

        // End point (20, 0)
        REQUIRE_THAT(batch.vertices[10], WithinAbs(20.0f, 1e-6f));
        REQUIRE_THAT(batch.vertices[11], WithinAbs(0.0f, 1e-6f));

        // Style propagation
        REQUIRE(batch.global_color == style.line_color);
        REQUIRE_THAT(batch.thickness, WithinAbs(3.0f, 1e-6f));
    }

    SECTION("empty histogram produces empty batch") {
        HistogramData empty;
        auto batch = HistogramMapper::toLine(empty, style);
        REQUIRE(batch.vertices.empty());
    }
}

// ============================================================================
// HistogramMapper::buildScene tests
// ============================================================================

TEST_CASE("HistogramMapper::buildScene", "[CorePlotting][HistogramMapper]")
{
    HistogramData data;
    data.bin_start = -50.0;
    data.bin_width = 25.0;
    data.counts = {2.0, 4.0, 1.0, 3.0};

    SECTION("bar mode scene") {
        auto scene = HistogramMapper::buildScene(data, HistogramDisplayMode::Bar);
        REQUIRE(scene.rectangle_batches.size() == 1);
        REQUIRE(scene.poly_line_batches.empty());
        REQUIRE(scene.rectangle_batches[0].bounds.size() == 4); // all bins > 0
    }

    SECTION("line mode scene") {
        auto scene = HistogramMapper::buildScene(data, HistogramDisplayMode::Line);
        REQUIRE(scene.poly_line_batches.size() == 1);
        REQUIRE(scene.rectangle_batches.empty());
        REQUIRE(scene.poly_line_batches[0].vertices.size() > 0);
    }
}
