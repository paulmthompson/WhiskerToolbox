/**
 * @file MovingLineGenerator.test.cpp
 * @brief Unit tests for the MovingLine generator.
 *
 * Tests registration, trajectory accuracy, rigid translation (relative vertex
 * distances constant across frames), boundary modes, Brownian determinism,
 * schema validation, and parameter rejection.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "Lines/Line_Data.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <vector>

using namespace WhiskerToolbox::DataSynthesizer;
using Catch::Matchers::WithinAbs;

static std::shared_ptr<LineData> runMovingLine(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("MovingLine", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<LineData>>(*result);
}

static std::vector<Line2D> getLinesAtTime(LineData const & ld, int frame) {
    auto range = ld.getAtTime(TimeFrameIndex(frame));
    return {range.begin(), range.end()};
}

/// Compute centroid of a line.
static Point2D<float> lineCentroid(Line2D const & line) {
    float sx = 0.0f;
    float sy = 0.0f;
    for (auto i : line) {
        sx += i.x;
        sy += i.y;
    }
    auto const n = static_cast<float>(line.size());
    return {sx / n, sy / n};
}

// ============================================================================
// Registration
// ============================================================================

TEST_CASE("MovingLine is registered", "[MovingLine]") {
    REQUIRE(GeneratorRegistry::instance().hasGenerator("MovingLine"));
}

TEST_CASE("MovingLine metadata has correct output type", "[MovingLine]") {
    auto generators = GeneratorRegistry::instance().listGenerators("LineData");
    bool found = false;
    for (auto const & name: generators) {
        if (name == "MovingLine") {
            found = true;
            break;
        }
    }
    REQUIRE(found);
}

// ============================================================================
// Frame count and point count
// ============================================================================

TEST_CASE("MovingLine produces correct number of frames", "[MovingLine]") {
    auto ld = runMovingLine(
            R"({
                "start_x": 0, "start_y": 0, "end_x": 100, "end_y": 0,
                "num_points_per_line": 20, "num_frames": 15,
                "trajectory_start_x": 200, "trajectory_start_y": 200,
                "motion": {"model": "LinearMotionParams", "velocity_x": 1, "velocity_y": 0},
                "bounds_max_x": 10000, "bounds_max_y": 10000
            })");
    REQUIRE(ld->getTimeCount() == 15);
}

TEST_CASE("MovingLine produces lines with correct number of points", "[MovingLine]") {
    auto ld = runMovingLine(
            R"({
                "start_x": 0, "start_y": 0, "end_x": 100, "end_y": 0,
                "num_points_per_line": 30, "num_frames": 3,
                "trajectory_start_x": 200, "trajectory_start_y": 200,
                "motion": {"model": "LinearMotionParams", "velocity_x": 0, "velocity_y": 0}
            })");
    for (int f = 0; f < 3; ++f) {
        auto lines = getLinesAtTime(*ld, f);
        REQUIRE(lines.size() == 1);
        REQUIRE(lines[0].size() == 30);
    }
}

// ============================================================================
// Centroid follows trajectory
// ============================================================================

TEST_CASE("MovingLine centroid follows linear trajectory", "[MovingLine]") {
    auto ld = runMovingLine(
            R"({
                "start_x": -50, "start_y": 0, "end_x": 50, "end_y": 0,
                "num_points_per_line": 11, "num_frames": 11,
                "trajectory_start_x": 0, "trajectory_start_y": 0,
                "motion": {"model": "LinearMotionParams", "velocity_x": 5, "velocity_y": 3},
                "bounds_max_x": 10000, "bounds_max_y": 10000
            })");

    // Frame 0: centroid should be at trajectory start (0, 0)
    auto lines_0 = getLinesAtTime(*ld, 0);
    auto c0 = lineCentroid(lines_0[0]);
    REQUIRE_THAT(c0.x, WithinAbs(0.0f, 0.1f));
    REQUIRE_THAT(c0.y, WithinAbs(0.0f, 0.1f));

    // Frame 10: centroid should be at (50, 30)
    auto lines_10 = getLinesAtTime(*ld, 10);
    auto c10 = lineCentroid(lines_10[0]);
    REQUIRE_THAT(c10.x, WithinAbs(50.0f, 0.1f));
    REQUIRE_THAT(c10.y, WithinAbs(30.0f, 0.1f));
}

TEST_CASE("MovingLine zero velocity is stationary", "[MovingLine]") {
    auto ld = runMovingLine(
            R"({
                "start_x": 0, "start_y": 0, "end_x": 40, "end_y": 0,
                "num_points_per_line": 5, "num_frames": 10,
                "trajectory_start_x": 100, "trajectory_start_y": 200,
                "motion": {"model": "LinearMotionParams", "velocity_x": 0, "velocity_y": 0}
            })");

    auto lines_0 = getLinesAtTime(*ld, 0);
    auto c0 = lineCentroid(lines_0[0]);

    for (int f = 1; f < 10; ++f) {
        auto lines = getLinesAtTime(*ld, f);
        auto c = lineCentroid(lines[0]);
        REQUIRE_THAT(c.x, WithinAbs(c0.x, 1e-5f));
        REQUIRE_THAT(c.y, WithinAbs(c0.y, 1e-5f));
    }
}

// ============================================================================
// Rigid translation — relative vertex distances constant across frames
// ============================================================================

TEST_CASE("MovingLine relative vertex distances are constant across frames", "[MovingLine]") {
    auto ld = runMovingLine(
            R"({
                "start_x": 0, "start_y": 0, "end_x": 80, "end_y": 60,
                "num_points_per_line": 10, "num_frames": 20,
                "trajectory_start_x": 100, "trajectory_start_y": 100,
                "motion": {"model": "LinearMotionParams", "velocity_x": 3, "velocity_y": -2},
                "bounds_max_x": 10000, "bounds_max_y": 10000
            })");

    // Compute reference offsets from centroid at frame 0
    auto lines_0 = getLinesAtTime(*ld, 0);
    REQUIRE(lines_0.size() == 1);
    auto c0 = lineCentroid(lines_0[0]);

    std::vector<Point2D<float>> ref_offsets;
    for (auto i : lines_0[0]) {
        ref_offsets.emplace_back(i.x - c0.x, i.y - c0.y);
    }

    // All frames should have the same offsets from their respective centroid
    for (int f = 1; f < 20; ++f) {
        auto lines = getLinesAtTime(*ld, f);
        REQUIRE(lines.size() == 1);
        auto c = lineCentroid(lines[0]);

        for (size_t i = 0; i < lines[0].size(); ++i) {
            float const off_x = lines[0][i].x - c.x;
            float const off_y = lines[0][i].y - c.y;
            REQUIRE_THAT(off_x, WithinAbs(ref_offsets[i].x, 1e-4f));
            REQUIRE_THAT(off_y, WithinAbs(ref_offsets[i].y, 1e-4f));
        }
    }
}

// ============================================================================
// Vertices may extend beyond bounds (no clipping)
// ============================================================================

TEST_CASE("MovingLine vertices may extend beyond trajectory bounds", "[MovingLine]") {
    // Line is long (200 units wide), bounds are tight, so vertices will exceed bounds
    auto ld = runMovingLine(
            R"({
                "start_x": -100, "start_y": 0, "end_x": 100, "end_y": 0,
                "num_points_per_line": 11, "num_frames": 5,
                "trajectory_start_x": 50, "trajectory_start_y": 50,
                "motion": {"model": "LinearMotionParams", "velocity_x": 0, "velocity_y": 0},
                "bounds_min_x": 0, "bounds_max_x": 100,
                "bounds_min_y": 0, "bounds_max_y": 100
            })");

    auto lines = getLinesAtTime(*ld, 0);
    REQUIRE(lines.size() == 1);

    // First point should be at centroid + offset(-100, 0). Centroid at (50, 50).
    // So first point x = 50 + (-100) = -50, which is outside bounds
    REQUIRE(lines[0][0].x < 0.0f);
    // Last point x = 50 + 100 = 150, also outside bounds
    REQUIRE(lines[0][lines[0].size() - 1].x > 100.0f);
}

// ============================================================================
// Boundary modes
// ============================================================================

TEST_CASE("MovingLine clamp boundary keeps centroid in bounds", "[MovingLine]") {
    auto ld = runMovingLine(
            R"({
                "start_x": -10, "start_y": 0, "end_x": 10, "end_y": 0,
                "num_points_per_line": 5, "num_frames": 30,
                "trajectory_start_x": 0, "trajectory_start_y": 50,
                "motion": {"model": "LinearMotionParams", "velocity_x": 20, "velocity_y": 0},
                "boundary_mode": "clamp",
                "bounds_min_x": 0, "bounds_max_x": 100,
                "bounds_min_y": 0, "bounds_max_y": 100
            })");

    for (int f = 0; f < 30; ++f) {
        auto lines = getLinesAtTime(*ld, f);
        REQUIRE(!lines.empty());
        auto c = lineCentroid(lines[0]);
        REQUIRE(c.x >= -0.01f);
        REQUIRE(c.x <= 100.01f);
    }
}

TEST_CASE("MovingLine bounce boundary keeps centroid in bounds", "[MovingLine]") {
    auto ld = runMovingLine(
            R"({
                "start_x": -10, "start_y": 0, "end_x": 10, "end_y": 0,
                "num_points_per_line": 5, "num_frames": 30,
                "trajectory_start_x": 50, "trajectory_start_y": 50,
                "motion": {"model": "LinearMotionParams", "velocity_x": 10, "velocity_y": 0},
                "boundary_mode": "bounce",
                "bounds_min_x": 0, "bounds_max_x": 100,
                "bounds_min_y": 0, "bounds_max_y": 100
            })");

    for (int f = 0; f < 30; ++f) {
        auto lines = getLinesAtTime(*ld, f);
        REQUIRE(!lines.empty());
        auto c = lineCentroid(lines[0]);
        REQUIRE(c.x >= -0.01f);
        REQUIRE(c.x <= 100.01f);
    }
}

TEST_CASE("MovingLine wrap boundary keeps centroid in bounds", "[MovingLine]") {
    auto ld = runMovingLine(
            R"({
                "start_x": -10, "start_y": 0, "end_x": 10, "end_y": 0,
                "num_points_per_line": 5, "num_frames": 30,
                "trajectory_start_x": 50, "trajectory_start_y": 50,
                "motion": {"model": "LinearMotionParams", "velocity_x": 10, "velocity_y": 0},
                "boundary_mode": "wrap",
                "bounds_min_x": 0, "bounds_max_x": 100,
                "bounds_min_y": 0, "bounds_max_y": 100
            })");

    for (int f = 0; f < 30; ++f) {
        auto lines = getLinesAtTime(*ld, f);
        REQUIRE(!lines.empty());
        auto c = lineCentroid(lines[0]);
        REQUIRE(c.x >= -0.01f);
        REQUIRE(c.x <= 100.01f);
    }
}

// ============================================================================
// Sinusoidal motion — centroid returns near start after one period
// ============================================================================

TEST_CASE("MovingLine sinusoidal centroid returns near start after one period", "[MovingLine]") {
    auto ld = runMovingLine(
            R"({
                "start_x": -20, "start_y": 0, "end_x": 20, "end_y": 0,
                "num_points_per_line": 5, "num_frames": 101,
                "trajectory_start_x": 200, "trajectory_start_y": 200,
                "motion": {
                    "model": "SinusoidalMotionParams",
                    "amplitude_x": 50, "amplitude_y": 30,
                    "frequency_x": 0.01, "frequency_y": 0.01
                },
                "bounds_min_x": -1000, "bounds_max_x": 1000,
                "bounds_min_y": -1000, "bounds_max_y": 1000
            })");

    auto lines_0 = getLinesAtTime(*ld, 0);
    auto lines_100 = getLinesAtTime(*ld, 100);
    auto c0 = lineCentroid(lines_0[0]);
    auto c100 = lineCentroid(lines_100[0]);

    REQUIRE_THAT(c0.x, WithinAbs(c100.x, 0.1f));
    REQUIRE_THAT(c0.y, WithinAbs(c100.y, 0.1f));
}

// ============================================================================
// Brownian motion determinism
// ============================================================================

TEST_CASE("MovingLine Brownian is deterministic with seed", "[MovingLine]") {
    std::string const json =
            R"({
                "start_x": -30, "start_y": -10, "end_x": 30, "end_y": 10,
                "num_points_per_line": 10, "num_frames": 20,
                "trajectory_start_x": 150, "trajectory_start_y": 150,
                "motion": {"model": "BrownianMotionParams", "diffusion": 5, "seed": 777},
                "bounds_min_x": -10000, "bounds_max_x": 10000,
                "bounds_min_y": -10000, "bounds_max_y": 10000
            })";

    auto ld1 = runMovingLine(json);
    auto ld2 = runMovingLine(json);

    for (int f = 0; f < 20; ++f) {
        auto lines1 = getLinesAtTime(*ld1, f);
        auto lines2 = getLinesAtTime(*ld2, f);
        REQUIRE(lines1.size() == 1);
        REQUIRE(lines2.size() == 1);
        REQUIRE(lines1[0].size() == lines2[0].size());
        for (size_t i = 0; i < lines1[0].size(); ++i) {
            REQUIRE_THAT(lines1[0][i].x, WithinAbs(lines2[0][i].x, 1e-10));
            REQUIRE_THAT(lines1[0][i].y, WithinAbs(lines2[0][i].y, 1e-10));
        }
    }
}

// ============================================================================
// Single frame is at start position
// ============================================================================

TEST_CASE("MovingLine single frame places centroid at trajectory start", "[MovingLine]") {
    auto ld = runMovingLine(
            R"({
                "start_x": -25, "start_y": 0, "end_x": 25, "end_y": 0,
                "num_points_per_line": 5, "num_frames": 1,
                "trajectory_start_x": 300, "trajectory_start_y": 400,
                "motion": {"model": "LinearMotionParams", "velocity_x": 100, "velocity_y": 200}
            })");

    REQUIRE(ld->getTimeCount() == 1);
    auto lines = getLinesAtTime(*ld, 0);
    REQUIRE(lines.size() == 1);

    auto c = lineCentroid(lines[0]);
    REQUIRE_THAT(c.x, WithinAbs(300.0f, 0.1f));
    REQUIRE_THAT(c.y, WithinAbs(400.0f, 0.1f));
}

// ============================================================================
// Matches StraightLine when stationary at origin
// ============================================================================

TEST_CASE("MovingLine matches StraightLine when stationary with matching centroid", "[MovingLine]") {
    // StraightLine from (10, 20) to (90, 80) — centroid at (50, 50)
    auto ld_static = GeneratorRegistry::instance().generate("StraightLine",
                                                            R"({"start_x": 10, "start_y": 20, "end_x": 90, "end_y": 80,
                "num_points_per_line": 10, "num_frames": 1})");
    REQUIRE(ld_static.has_value());
    auto static_line = std::get<std::shared_ptr<LineData>>(*ld_static);

    // MovingLine with same endpoints, trajectory start at centroid (50, 50), zero velocity
    auto ld_moving = runMovingLine(
            R"({
                "start_x": 10, "start_y": 20, "end_x": 90, "end_y": 80,
                "num_points_per_line": 10, "num_frames": 1,
                "trajectory_start_x": 50, "trajectory_start_y": 50,
                "motion": {"model": "LinearMotionParams", "velocity_x": 0, "velocity_y": 0}
            })");

    auto static_lines = getLinesAtTime(*static_line, 0);
    auto moving_lines = getLinesAtTime(*ld_moving, 0);
    REQUIRE(static_lines.size() == 1);
    REQUIRE(moving_lines.size() == 1);
    REQUIRE(static_lines[0].size() == moving_lines[0].size());

    for (size_t i = 0; i < static_lines[0].size(); ++i) {
        REQUIRE_THAT(moving_lines[0][i].x, WithinAbs(static_lines[0][i].x, 1e-4f));
        REQUIRE_THAT(moving_lines[0][i].y, WithinAbs(static_lines[0][i].y, 1e-4f));
    }
}

// ============================================================================
// Schema validation
// ============================================================================

TEST_CASE("MovingLine schema has variant field for motion", "[MovingLine]") {
    auto schema_opt = GeneratorRegistry::instance().getSchema("MovingLine");
    REQUIRE(schema_opt.has_value());

    auto const & schema = *schema_opt;

    auto * motion_field = schema.field("motion");
    REQUIRE(motion_field != nullptr);
    CHECK(motion_field->is_variant);
    CHECK(motion_field->variant_discriminator == "model");
    CHECK(motion_field->variant_alternatives.size() == 3);
    CHECK(motion_field->allowed_values.size() == 3);
}

TEST_CASE("MovingLine schema has enum field for boundary_mode", "[MovingLine]") {
    auto schema_opt = GeneratorRegistry::instance().getSchema("MovingLine");
    REQUIRE(schema_opt.has_value());

    auto const & schema = *schema_opt;

    auto * bm_field = schema.field("boundary_mode");
    REQUIRE(bm_field != nullptr);
    CHECK(bm_field->type_name == "enum");
    CHECK(bm_field->allowed_values.size() == 3);
}

// ============================================================================
// Parameter validation
// ============================================================================

TEST_CASE("MovingLine rejects invalid parameters", "[MovingLine]") {
    auto & reg = GeneratorRegistry::instance();

    // Zero frames
    REQUIRE_FALSE(reg.generate("MovingLine",
                               R"({"start_x": 0, "start_y": 0, "end_x": 100, "end_y": 0,
                "num_points_per_line": 10, "num_frames": 0,
                "trajectory_start_x": 50, "trajectory_start_y": 50,
                "motion": {"model": "LinearMotionParams"}})")
                          .has_value());

    // Negative frames
    REQUIRE_FALSE(reg.generate("MovingLine",
                               R"({"start_x": 0, "start_y": 0, "end_x": 100, "end_y": 0,
                "num_points_per_line": 10, "num_frames": -5,
                "trajectory_start_x": 50, "trajectory_start_y": 50,
                "motion": {"model": "LinearMotionParams"}})")
                          .has_value());

    // Too few points
    REQUIRE_FALSE(reg.generate("MovingLine",
                               R"({"start_x": 0, "start_y": 0, "end_x": 100, "end_y": 0,
                "num_points_per_line": 1, "num_frames": 5,
                "trajectory_start_x": 50, "trajectory_start_y": 50,
                "motion": {"model": "LinearMotionParams"}})")
                          .has_value());

    REQUIRE_FALSE(reg.generate("MovingLine",
                               R"({"start_x": 0, "start_y": 0, "end_x": 100, "end_y": 0,
                "num_points_per_line": 0, "num_frames": 5,
                "trajectory_start_x": 50, "trajectory_start_y": 50,
                "motion": {"model": "LinearMotionParams"}})")
                          .has_value());
}
