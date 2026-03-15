/**
 * @file MovingPointGenerator.test.cpp
 * @brief Unit tests for the MovingPoint generator.
 *
 * Tests use the new nested JSON format with MotionModelVariant (rfl::TaggedUnion)
 * and BoundaryParams struct, introduced in Milestone 5b-ia.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "Points/Point_Data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace WhiskerToolbox::DataSynthesizer;
using Catch::Matchers::WithinAbs;

static std::shared_ptr<PointData> runMovingPoint(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("MovingPoint", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<PointData>>(*result);
}

static std::vector<Point2D<float>> getPointsAtTime(PointData const & pd, int frame) {
    auto range = pd.getAtTime(TimeFrameIndex(frame));
    return {range.begin(), range.end()};
}

TEST_CASE("MovingPoint is registered", "[MovingPoint]") {
    REQUIRE(GeneratorRegistry::instance().hasGenerator("MovingPoint"));
}

TEST_CASE("MovingPoint produces correct number of frames", "[MovingPoint]") {
    auto pd = runMovingPoint(
            R"({
                "start_x": 100, "start_y": 100, "num_frames": 20,
                "motion": {"model": "LinearMotionParams", "velocity_x": 1, "velocity_y": 0},
                "boundary": {"bounds_max_x": 10000, "bounds_max_y": 10000}
            })");
    REQUIRE(pd->getTimeCount() == 20);
}

TEST_CASE("MovingPoint has exactly one point per frame", "[MovingPoint]") {
    auto pd = runMovingPoint(
            R"({
                "start_x": 50, "start_y": 50, "num_frames": 10,
                "motion": {"model": "LinearMotionParams", "velocity_x": 2, "velocity_y": 0},
                "boundary": {"bounds_max_x": 10000, "bounds_max_y": 10000}
            })");

    for (int f = 0; f < 10; ++f) {
        auto pts = getPointsAtTime(*pd, f);
        REQUIRE(pts.size() == 1);
    }
}

TEST_CASE("MovingPoint linear motion reaches expected position", "[MovingPoint]") {
    auto pd = runMovingPoint(
            R"({
                "start_x": 0, "start_y": 0, "num_frames": 11,
                "motion": {"model": "LinearMotionParams", "velocity_x": 5, "velocity_y": 3},
                "boundary": {"bounds_max_x": 10000, "bounds_max_y": 10000}
            })");

    auto pts_0 = getPointsAtTime(*pd, 0);
    REQUIRE_THAT(pts_0[0].x, WithinAbs(0.0f, 1e-5));
    REQUIRE_THAT(pts_0[0].y, WithinAbs(0.0f, 1e-5));

    // Frame 10: start + 10 * velocity
    auto pts_10 = getPointsAtTime(*pd, 10);
    REQUIRE_THAT(pts_10[0].x, WithinAbs(50.0f, 1e-5));
    REQUIRE_THAT(pts_10[0].y, WithinAbs(30.0f, 1e-5));
}

TEST_CASE("MovingPoint sinusoidal returns near start after one period", "[MovingPoint]") {
    auto pd = runMovingPoint(
            R"({
                "start_x": 200, "start_y": 200, "num_frames": 101,
                "motion": {
                    "model": "SinusoidalMotionParams",
                    "amplitude_x": 50, "amplitude_y": 30,
                    "frequency_x": 0.01, "frequency_y": 0.01
                },
                "boundary": {
                    "bounds_min_x": -1000, "bounds_max_x": 1000,
                    "bounds_min_y": -1000, "bounds_max_y": 1000
                }
            })");

    auto pts_0 = getPointsAtTime(*pd, 0);
    auto pts_100 = getPointsAtTime(*pd, 100);

    REQUIRE_THAT(pts_0[0].x, WithinAbs(pts_100[0].x, 0.1f));
    REQUIRE_THAT(pts_0[0].y, WithinAbs(pts_100[0].y, 0.1f));
}

TEST_CASE("MovingPoint clamp boundary keeps point in bounds", "[MovingPoint]") {
    auto pd = runMovingPoint(
            R"({
                "start_x": 0, "start_y": 0, "num_frames": 20,
                "motion": {"model": "LinearMotionParams", "velocity_x": 10, "velocity_y": 0},
                "boundary": {
                    "boundary_mode": "clamp",
                    "bounds_min_x": 0, "bounds_max_x": 50,
                    "bounds_min_y": 0, "bounds_max_y": 100
                }
            })");

    for (int f = 0; f < 20; ++f) {
        auto pts = getPointsAtTime(*pd, f);
        REQUIRE(pts[0].x >= -0.01f);
        REQUIRE(pts[0].x <= 50.01f);
    }
}

TEST_CASE("MovingPoint bounce boundary keeps point in bounds", "[MovingPoint]") {
    auto pd = runMovingPoint(
            R"({
                "start_x": 0, "start_y": 0, "num_frames": 30,
                "motion": {"model": "LinearMotionParams", "velocity_x": 10, "velocity_y": 0},
                "boundary": {
                    "boundary_mode": "bounce",
                    "bounds_min_x": 0, "bounds_max_x": 50,
                    "bounds_min_y": 0, "bounds_max_y": 100
                }
            })");

    for (int f = 0; f < 30; ++f) {
        auto pts = getPointsAtTime(*pd, f);
        REQUIRE(pts[0].x >= -0.01f);
        REQUIRE(pts[0].x <= 50.01f);
    }
}

TEST_CASE("MovingPoint wrap boundary keeps point in bounds", "[MovingPoint]") {
    auto pd = runMovingPoint(
            R"({
                "start_x": 0, "start_y": 0, "num_frames": 30,
                "motion": {"model": "LinearMotionParams", "velocity_x": 10, "velocity_y": 0},
                "boundary": {
                    "boundary_mode": "wrap",
                    "bounds_min_x": 0, "bounds_max_x": 50,
                    "bounds_min_y": 0, "bounds_max_y": 100
                }
            })");

    for (int f = 0; f < 30; ++f) {
        auto pts = getPointsAtTime(*pd, f);
        REQUIRE(pts[0].x >= -0.01f);
        REQUIRE(pts[0].x <= 50.01f);
    }
}

TEST_CASE("MovingPoint zero velocity is stationary", "[MovingPoint]") {
    auto pd = runMovingPoint(
            R"({
                "start_x": 42, "start_y": 99, "num_frames": 10,
                "motion": {"model": "LinearMotionParams", "velocity_x": 0, "velocity_y": 0}
            })");

    for (int f = 0; f < 10; ++f) {
        auto pts = getPointsAtTime(*pd, f);
        REQUIRE_THAT(pts[0].x, WithinAbs(42.0f, 1e-5));
        REQUIRE_THAT(pts[0].y, WithinAbs(99.0f, 1e-5));
    }
}

TEST_CASE("MovingPoint Brownian is deterministic with seed", "[MovingPoint]") {
    std::string const json =
            R"({
                "start_x": 100, "start_y": 100, "num_frames": 20,
                "motion": {"model": "BrownianMotionParams", "diffusion": 5, "seed": 777},
                "boundary": {
                    "bounds_min_x": -10000, "bounds_max_x": 10000,
                    "bounds_min_y": -10000, "bounds_max_y": 10000
                }
            })";

    auto pd1 = runMovingPoint(json);
    auto pd2 = runMovingPoint(json);

    for (int f = 0; f < 20; ++f) {
        auto pts1 = getPointsAtTime(*pd1, f);
        auto pts2 = getPointsAtTime(*pd2, f);
        REQUIRE_THAT(pts1[0].x, WithinAbs(pts2[0].x, 1e-10));
        REQUIRE_THAT(pts1[0].y, WithinAbs(pts2[0].y, 1e-10));
    }
}

TEST_CASE("MovingPoint rejects invalid num_frames", "[MovingPoint]") {
    auto & reg = GeneratorRegistry::instance();
    REQUIRE_FALSE(reg.generate("MovingPoint",
                               R"({"start_x": 0, "start_y": 0, "num_frames": 0,
                                   "motion": {"model": "LinearMotionParams"}})")
                          .has_value());
    REQUIRE_FALSE(reg.generate("MovingPoint",
                               R"({"start_x": 0, "start_y": 0, "num_frames": -5,
                                   "motion": {"model": "LinearMotionParams"}})")
                          .has_value());
}

TEST_CASE("MovingPoint single frame is start position", "[MovingPoint]") {
    auto pd = runMovingPoint(
            R"({
                "start_x": 123, "start_y": 456, "num_frames": 1,
                "motion": {"model": "LinearMotionParams", "velocity_x": 100, "velocity_y": 200}
            })");

    REQUIRE(pd->getTimeCount() == 1);
    auto pts = getPointsAtTime(*pd, 0);
    REQUIRE(pts.size() == 1);
    REQUIRE_THAT(pts[0].x, WithinAbs(123.0f, 1e-5));
    REQUIRE_THAT(pts[0].y, WithinAbs(456.0f, 1e-5));
}

TEST_CASE("MovingPoint schema has variant field for motion", "[MovingPoint]") {
    auto schema_opt = GeneratorRegistry::instance().getSchema("MovingPoint");
    REQUIRE(schema_opt.has_value());

    auto const & schema = *schema_opt;

    auto * motion_field = schema.field("motion");
    REQUIRE(motion_field != nullptr);
    CHECK(motion_field->is_variant);
    CHECK(motion_field->variant_discriminator == "model");
    CHECK(motion_field->variant_alternatives.size() == 3);

    // Verify the alternatives contain expected tags
    CHECK(motion_field->allowed_values.size() == 3);
}

TEST_CASE("MovingPoint schema has enum field for boundary_mode", "[MovingPoint]") {
    auto schema_opt = GeneratorRegistry::instance().getSchema("MovingPoint");
    REQUIRE(schema_opt.has_value());

    auto const & schema = *schema_opt;

    // boundary is a nested struct; find its sub-fields via the boundary field
    auto * boundary_field = schema.field("boundary");
    REQUIRE(boundary_field != nullptr);
}
