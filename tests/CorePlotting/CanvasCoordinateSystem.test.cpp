/**
 * @file CanvasCoordinateSystem.test.cpp
 * @brief Unit tests for CanvasCoordinateSystem, computeScalingFactors, and
 *        computeInverseScalingFactors (pure math, no Qt dependency)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/Layout/CanvasCoordinateSystem.hpp"

using Catch::Matchers::WithinAbs;

// =============================================================================
// CanvasCoordinateSystem struct
// =============================================================================

TEST_CASE("CanvasCoordinateSystem default is 640x480 Default source",
          "[CanvasCoordinateSystem]") {
    CanvasCoordinateSystem const cs;
    CHECK(cs.width == 640);
    CHECK(cs.height == 480);
    CHECK(cs.source == CanvasCoordinateSource::Default);
    CHECK(cs.source_key.empty());
}

TEST_CASE("CanvasCoordinateSystem equality ignores source and source_key",
          "[CanvasCoordinateSystem]") {
    CanvasCoordinateSystem const a{640, 480, CanvasCoordinateSource::Default, ""};
    CanvasCoordinateSystem const b{640, 480, CanvasCoordinateSource::Media, "cam0"};
    CHECK(a == b);

    CanvasCoordinateSystem const c{256, 256, CanvasCoordinateSource::Default, ""};
    CHECK(a != c);
}

TEST_CASE("CanvasCoordinateSystem toImageSize round-trips",
          "[CanvasCoordinateSystem]") {
    CanvasCoordinateSystem const cs{512, 384, CanvasCoordinateSource::DataObject, "mask1"};
    auto const sz = cs.toImageSize();
    CHECK(sz.width == 512);
    CHECK(sz.height == 384);
}

TEST_CASE("CanvasCoordinateSystem fromImageSize preserves source",
          "[CanvasCoordinateSystem]") {
    ImageSize const sz{1024, 768};
    auto const cs = CanvasCoordinateSystem::fromImageSize(
            sz, CanvasCoordinateSource::UserOverride, "user");
    CHECK(cs.width == 1024);
    CHECK(cs.height == 768);
    CHECK(cs.source == CanvasCoordinateSource::UserOverride);
    CHECK(cs.source_key == "user");
}

// =============================================================================
// computeScalingFactors
// =============================================================================

TEST_CASE("computeScalingFactors: identity when canvas == coord system, undefined ImageSize",
          "[computeScalingFactors]") {
    CanvasCoordinateSystem const cs{640, 480};
    ImageSize const undefined{-1, -1};

    auto const f = computeScalingFactors(640, 480, cs, undefined,
                                         CoordinateMappingMode::ScaleToCanvas);
    CHECK_THAT(f.x, WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(f.y, WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("computeScalingFactors: undefined ImageSize uses coord system dims",
          "[computeScalingFactors]") {
    CanvasCoordinateSystem const cs{320, 240};
    ImageSize const undefined{-1, -1};

    auto const f = computeScalingFactors(640, 480, cs, undefined,
                                         CoordinateMappingMode::ScaleToCanvas);
    CHECK_THAT(f.x, WithinAbs(2.0f, 1e-5f));
    CHECK_THAT(f.y, WithinAbs(2.0f, 1e-5f));
}

TEST_CASE("computeScalingFactors: ScaleToCanvas with defined ImageSize",
          "[computeScalingFactors]") {
    CanvasCoordinateSystem const cs{640, 480};
    ImageSize const data_size{256, 128};

    auto const f = computeScalingFactors(640, 480, cs, data_size,
                                         CoordinateMappingMode::ScaleToCanvas);
    CHECK_THAT(f.x, WithinAbs(640.0f / 256.0f, 1e-5f));
    CHECK_THAT(f.y, WithinAbs(480.0f / 128.0f, 1e-5f));
}

TEST_CASE("computeScalingFactors: SubsetOfCanvas always uses coord system",
          "[computeScalingFactors]") {
    CanvasCoordinateSystem const cs{640, 480};
    ImageSize const data_size{256, 128};

    auto const f = computeScalingFactors(640, 480, cs, data_size,
                                         CoordinateMappingMode::SubsetOfCanvas);
    CHECK_THAT(f.x, WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(f.y, WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("computeScalingFactors: non-square canvas and coord system",
          "[computeScalingFactors]") {
    CanvasCoordinateSystem const cs{800, 600};
    ImageSize const undefined{-1, -1};

    auto const f = computeScalingFactors(1600, 900, cs, undefined,
                                         CoordinateMappingMode::ScaleToCanvas);
    CHECK_THAT(f.x, WithinAbs(1600.0f / 800.0f, 1e-5f));
    CHECK_THAT(f.y, WithinAbs(900.0f / 600.0f, 1e-5f));
}

TEST_CASE("computeScalingFactors: ScaleToCanvas with same dims as coord system",
          "[computeScalingFactors]") {
    CanvasCoordinateSystem const cs{640, 480};
    ImageSize const data_size{640, 480};

    auto const f = computeScalingFactors(640, 480, cs, data_size,
                                         CoordinateMappingMode::ScaleToCanvas);
    CHECK_THAT(f.x, WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(f.y, WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("computeScalingFactors: data smaller than coord system with ScaleToCanvas",
          "[computeScalingFactors]") {
    // Data at 256x256, canvas at 640x480, coord system at 640x480
    // ScaleToCanvas: data covers full canvas → factors = 640/256, 480/256
    CanvasCoordinateSystem const cs{640, 480};
    ImageSize const data_size{256, 256};

    auto const f = computeScalingFactors(640, 480, cs, data_size,
                                         CoordinateMappingMode::ScaleToCanvas);
    CHECK_THAT(f.x, WithinAbs(640.0f / 256.0f, 1e-5f));
    CHECK_THAT(f.y, WithinAbs(480.0f / 256.0f, 1e-5f));
}

TEST_CASE("computeScalingFactors: SubsetOfCanvas ignores data ImageSize",
          "[computeScalingFactors]") {
    CanvasCoordinateSystem const cs{1024, 768};
    ImageSize const data_size{200, 200};

    auto const f = computeScalingFactors(1024, 768, cs, data_size,
                                         CoordinateMappingMode::SubsetOfCanvas);
    // SubsetOfCanvas always uses coord system dims
    CHECK_THAT(f.x, WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(f.y, WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("computeScalingFactors: canvas pixels differ from coord system with SubsetOfCanvas",
          "[computeScalingFactors]") {
    // Canvas is 1280x960 pixels but coord system is 640x480
    CanvasCoordinateSystem const cs{640, 480};
    ImageSize const data_size{320, 320};

    auto const f = computeScalingFactors(1280, 960, cs, data_size,
                                         CoordinateMappingMode::SubsetOfCanvas);
    CHECK_THAT(f.x, WithinAbs(1280.0f / 640.0f, 1e-5f));
    CHECK_THAT(f.y, WithinAbs(960.0f / 480.0f, 1e-5f));
}

// =============================================================================
// computeInverseScalingFactors
// =============================================================================

TEST_CASE("computeInverseScalingFactors: identity when canvas == coord system",
          "[computeInverseScalingFactors]") {
    CanvasCoordinateSystem const cs{640, 480};

    auto const f = computeInverseScalingFactors(640, 480, cs);
    CHECK_THAT(f.x, WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(f.y, WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("computeInverseScalingFactors: scales down when canvas > coord system",
          "[computeInverseScalingFactors]") {
    // Canvas 1280x960, coord system 640x480
    // Inverse: multiply pixel coords by 640/1280 = 0.5 to get coord system units
    CanvasCoordinateSystem const cs{640, 480};

    auto const f = computeInverseScalingFactors(1280, 960, cs);
    CHECK_THAT(f.x, WithinAbs(0.5f, 1e-5f));
    CHECK_THAT(f.y, WithinAbs(0.5f, 1e-5f));
}

TEST_CASE("computeInverseScalingFactors: scales up when canvas < coord system",
          "[computeInverseScalingFactors]") {
    CanvasCoordinateSystem const cs{640, 480};

    auto const f = computeInverseScalingFactors(320, 240, cs);
    CHECK_THAT(f.x, WithinAbs(2.0f, 1e-5f));
    CHECK_THAT(f.y, WithinAbs(2.0f, 1e-5f));
}

// =============================================================================
// Round-trip: forward × inverse == identity
// =============================================================================

TEST_CASE("computeScalingFactors and inverse round-trip to identity",
          "[computeScalingFactors][computeInverseScalingFactors]") {
    CanvasCoordinateSystem const cs{800, 600};

    auto const fwd = computeScalingFactors(1280, 960, cs, ImageSize{-1, -1},
                                           CoordinateMappingMode::ScaleToCanvas);
    auto const inv = computeInverseScalingFactors(1280, 960, cs);

    CHECK_THAT(fwd.x * inv.x, WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(fwd.y * inv.y, WithinAbs(1.0f, 1e-5f));
}

TEST_CASE("Forward scaling maps origin and max correctly",
          "[computeScalingFactors]") {
    CanvasCoordinateSystem const cs{640, 480};

    auto const f = computeScalingFactors(800, 600, cs, ImageSize{-1, -1},
                                         CoordinateMappingMode::ScaleToCanvas);

    // Origin stays at (0, 0)
    CHECK_THAT(0.0f * f.x, WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(0.0f * f.y, WithinAbs(0.0f, 1e-5f));

    // Max coord (640, 480) maps to canvas edge (800, 600)
    CHECK_THAT(640.0f * f.x, WithinAbs(800.0f, 1e-3f));
    CHECK_THAT(480.0f * f.y, WithinAbs(600.0f, 1e-3f));
}

TEST_CASE("Inverse scaling maps canvas edge back to coord system max",
          "[computeInverseScalingFactors]") {
    CanvasCoordinateSystem const cs{640, 480};

    auto const inv = computeInverseScalingFactors(800, 600, cs);

    // Canvas edge (800, 600) → coord system max (640, 480)
    CHECK_THAT(800.0f * inv.x, WithinAbs(640.0f, 1e-3f));
    CHECK_THAT(600.0f * inv.y, WithinAbs(480.0f, 1e-3f));
}
