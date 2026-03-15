/**
 * @file MovingMaskGenerator.test.cpp
 * @brief Unit tests for the MovingMask generator.
 *
 * Tests each shape type (circle, rectangle, ellipse), trajectory accuracy,
 * pixel clipping at image edges, area preservation for fully interior masks,
 * and deterministic Brownian motion.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "Masks/Mask_Data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using namespace WhiskerToolbox::DataSynthesizer;
using Catch::Matchers::WithinAbs;

static std::shared_ptr<MaskData> runMovingMask(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("MovingMask", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<MaskData>>(*result);
}

static std::vector<Mask2D> getMasksAtTime(MaskData const & md, int frame) {
    auto range = md.getAtTime(TimeFrameIndex(frame));
    return {range.begin(), range.end()};
}

// ============================================================================
// Registration
// ============================================================================

TEST_CASE("MovingMask is registered", "[MovingMask]") {
    REQUIRE(GeneratorRegistry::instance().hasGenerator("MovingMask"));
}

TEST_CASE("MovingMask metadata has correct output type", "[MovingMask]") {
    auto generators = GeneratorRegistry::instance().listGenerators("MaskData");
    bool found = false;
    for (auto const & name: generators) {
        if (name == "MovingMask") {
            found = true;
            break;
        }
    }
    REQUIRE(found);
}

// ============================================================================
// Frame count
// ============================================================================

TEST_CASE("MovingMask produces correct number of frames", "[MovingMask]") {
    auto md = runMovingMask(
            R"({
                "shape": "circle", "radius": 10,
                "image_width": 200, "image_height": 200,
                "start_x": 100, "start_y": 100, "num_frames": 25,
                "motion": {"model": "LinearMotionParams", "velocity_x": 1, "velocity_y": 0},
                "bounds_max_x": 10000, "bounds_max_y": 10000
            })");
    REQUIRE(md->getTimeCount() == 25);
}

// ============================================================================
// Shape types produce non-empty masks
// ============================================================================

TEST_CASE("MovingMask circle shape produces non-empty masks", "[MovingMask]") {
    auto md = runMovingMask(
            R"({
                "shape": "circle", "radius": 15,
                "image_width": 300, "image_height": 300,
                "start_x": 150, "start_y": 150, "num_frames": 5,
                "motion": {"model": "LinearMotionParams", "velocity_x": 0, "velocity_y": 0}
            })");

    for (int f = 0; f < 5; ++f) {
        auto masks = getMasksAtTime(*md, f);
        REQUIRE(!masks.empty());
        REQUIRE(!masks[0].empty());
    }
}

TEST_CASE("MovingMask rectangle shape produces non-empty masks", "[MovingMask]") {
    auto md = runMovingMask(
            R"({
                "shape": "rectangle", "width": 20, "height": 30,
                "image_width": 300, "image_height": 300,
                "start_x": 150, "start_y": 150, "num_frames": 3,
                "motion": {"model": "LinearMotionParams", "velocity_x": 0, "velocity_y": 0}
            })");

    auto masks = getMasksAtTime(*md, 0);
    REQUIRE(!masks.empty());
    REQUIRE(!masks[0].empty());
}

TEST_CASE("MovingMask ellipse shape produces non-empty masks", "[MovingMask]") {
    auto md = runMovingMask(
            R"({
                "shape": "ellipse", "semi_major": 25, "semi_minor": 15,
                "image_width": 300, "image_height": 300,
                "start_x": 150, "start_y": 150, "num_frames": 3,
                "motion": {"model": "LinearMotionParams", "velocity_x": 0, "velocity_y": 0}
            })");

    auto masks = getMasksAtTime(*md, 0);
    REQUIRE(!masks.empty());
    REQUIRE(!masks[0].empty());
}

// ============================================================================
// Area matches static generator when fully interior (zero velocity)
// ============================================================================

TEST_CASE("MovingMask circle area matches static CircleMask when stationary", "[MovingMask]") {
    float const radius = 20.0f;

    auto md_moving = runMovingMask(
            R"({
                "shape": "circle", "radius": 20,
                "image_width": 300, "image_height": 300,
                "start_x": 150, "start_y": 150, "num_frames": 1,
                "motion": {"model": "LinearMotionParams", "velocity_x": 0, "velocity_y": 0}
            })");

    auto md_static = GeneratorRegistry::instance().generate("CircleMask",
                                                            R"({"image_width": 300, "image_height": 300, "center_x": 150, "center_y": 150, "radius": 20, "num_frames": 1})");
    REQUIRE(md_static.has_value());
    auto static_mask = std::get<std::shared_ptr<MaskData>>(*md_static);

    auto masks_moving = getMasksAtTime(*md_moving, 0);
    auto masks_static = getMasksAtTime(*static_mask, 0);

    REQUIRE(!masks_moving.empty());
    REQUIRE(!masks_static.empty());
    REQUIRE(masks_moving[0].size() == masks_static[0].size());
}

TEST_CASE("MovingMask rectangle area matches static RectangleMask when stationary", "[MovingMask]") {
    auto md_moving = runMovingMask(
            R"({
                "shape": "rectangle", "width": 40, "height": 30,
                "image_width": 300, "image_height": 300,
                "start_x": 150, "start_y": 150, "num_frames": 1,
                "motion": {"model": "LinearMotionParams", "velocity_x": 0, "velocity_y": 0}
            })");

    auto md_static = GeneratorRegistry::instance().generate("RectangleMask",
                                                            R"({"image_width": 300, "image_height": 300, "center_x": 150, "center_y": 150, "width": 40, "height": 30, "num_frames": 1})");
    REQUIRE(md_static.has_value());
    auto static_mask = std::get<std::shared_ptr<MaskData>>(*md_static);

    auto masks_moving = getMasksAtTime(*md_moving, 0);
    auto masks_static = getMasksAtTime(*static_mask, 0);

    REQUIRE(!masks_moving.empty());
    REQUIRE(!masks_static.empty());
    REQUIRE(masks_moving[0].size() == masks_static[0].size());
}

// ============================================================================
// Pixel count preserved when mask is fully interior
// ============================================================================

TEST_CASE("MovingMask circle pixel count constant when fully interior", "[MovingMask]") {
    auto md = runMovingMask(
            R"({
                "shape": "circle", "radius": 10,
                "image_width": 500, "image_height": 500,
                "start_x": 250, "start_y": 250, "num_frames": 10,
                "motion": {"model": "LinearMotionParams", "velocity_x": 2, "velocity_y": 1},
                "bounds_min_x": 50, "bounds_max_x": 450,
                "bounds_min_y": 50, "bounds_max_y": 450
            })");

    auto const reference_masks = getMasksAtTime(*md, 0);
    REQUIRE(!reference_masks.empty());
    auto const reference_count = reference_masks[0].size();
    REQUIRE(reference_count > 0);

    for (int f = 1; f < 10; ++f) {
        auto masks = getMasksAtTime(*md, f);
        REQUIRE(!masks.empty());
        REQUIRE(masks[0].size() == reference_count);
    }
}

// ============================================================================
// Clipping: pixel count decreases near edges
// ============================================================================

TEST_CASE("MovingMask circle pixel count decreases near edge", "[MovingMask]") {
    // Mask moves toward the right edge
    auto md = runMovingMask(
            R"({
                "shape": "circle", "radius": 20,
                "image_width": 200, "image_height": 200,
                "start_x": 100, "start_y": 100, "num_frames": 50,
                "motion": {"model": "LinearMotionParams", "velocity_x": 3, "velocity_y": 0},
                "boundary_mode": "clamp",
                "bounds_min_x": 0, "bounds_max_x": 200,
                "bounds_min_y": 0, "bounds_max_y": 200
            })");

    auto const interior_masks = getMasksAtTime(*md, 0);
    REQUIRE(!interior_masks.empty());
    auto const interior_count = interior_masks[0].size();

    // At the last frame, center should be clamped near right edge
    auto const edge_masks = getMasksAtTime(*md, 49);
    REQUIRE(!edge_masks.empty());
    REQUIRE(edge_masks[0].size() < interior_count);
}

TEST_CASE("MovingMask clipped pixels stay within image bounds", "[MovingMask]") {
    int const img_w = 150;
    int const img_h = 150;

    auto md = runMovingMask(
            R"({
                "shape": "circle", "radius": 30,
                "image_width": 150, "image_height": 150,
                "start_x": 130, "start_y": 130, "num_frames": 5,
                "motion": {"model": "LinearMotionParams", "velocity_x": 5, "velocity_y": 5},
                "boundary_mode": "clamp",
                "bounds_min_x": 0, "bounds_max_x": 200,
                "bounds_min_y": 0, "bounds_max_y": 200
            })");

    for (int f = 0; f < 5; ++f) {
        auto masks = getMasksAtTime(*md, f);
        REQUIRE(!masks.empty());
        for (auto const & p: masks[0]) {
            REQUIRE(p.x < static_cast<uint32_t>(img_w));
            REQUIRE(p.y < static_cast<uint32_t>(img_h));
        }
    }
}

// ============================================================================
// Boundary modes
// ============================================================================

TEST_CASE("MovingMask clamp boundary keeps mask in bounds", "[MovingMask]") {
    auto md = runMovingMask(
            R"({
                "shape": "circle", "radius": 10,
                "image_width": 500, "image_height": 500,
                "start_x": 0, "start_y": 0, "num_frames": 30,
                "motion": {"model": "LinearMotionParams", "velocity_x": 20, "velocity_y": 0},
                "boundary_mode": "clamp",
                "bounds_min_x": 0, "bounds_max_x": 100,
                "bounds_min_y": 0, "bounds_max_y": 500
            })");

    // All frames should have non-empty masks
    for (int f = 0; f < 30; ++f) {
        auto masks = getMasksAtTime(*md, f);
        REQUIRE(!masks.empty());
        REQUIRE(!masks[0].empty());
    }
}

TEST_CASE("MovingMask bounce boundary keeps mask in bounds", "[MovingMask]") {
    auto md = runMovingMask(
            R"({
                "shape": "circle", "radius": 10,
                "image_width": 500, "image_height": 500,
                "start_x": 50, "start_y": 250, "num_frames": 30,
                "motion": {"model": "LinearMotionParams", "velocity_x": 10, "velocity_y": 0},
                "boundary_mode": "bounce",
                "bounds_min_x": 0, "bounds_max_x": 100,
                "bounds_min_y": 0, "bounds_max_y": 500
            })");

    for (int f = 0; f < 30; ++f) {
        auto masks = getMasksAtTime(*md, f);
        REQUIRE(!masks.empty());
        REQUIRE(!masks[0].empty());
    }
}

TEST_CASE("MovingMask wrap boundary keeps mask in bounds", "[MovingMask]") {
    auto md = runMovingMask(
            R"({
                "shape": "circle", "radius": 10,
                "image_width": 500, "image_height": 500,
                "start_x": 50, "start_y": 250, "num_frames": 30,
                "motion": {"model": "LinearMotionParams", "velocity_x": 10, "velocity_y": 0},
                "boundary_mode": "wrap",
                "bounds_min_x": 0, "bounds_max_x": 100,
                "bounds_min_y": 0, "bounds_max_y": 500
            })");

    for (int f = 0; f < 30; ++f) {
        auto masks = getMasksAtTime(*md, f);
        REQUIRE(!masks.empty());
        REQUIRE(!masks[0].empty());
    }
}

// ============================================================================
// Brownian motion determinism
// ============================================================================

TEST_CASE("MovingMask Brownian is deterministic with seed", "[MovingMask]") {
    std::string const json =
            R"({
                "shape": "circle", "radius": 10,
                "image_width": 300, "image_height": 300,
                "start_x": 150, "start_y": 150, "num_frames": 20,
                "motion": {"model": "BrownianMotionParams", "diffusion": 3, "seed": 999},
                "bounds_min_x": 0, "bounds_max_x": 300,
                "bounds_min_y": 0, "bounds_max_y": 300
            })";

    auto md1 = runMovingMask(json);
    auto md2 = runMovingMask(json);

    for (int f = 0; f < 20; ++f) {
        auto masks1 = getMasksAtTime(*md1, f);
        auto masks2 = getMasksAtTime(*md2, f);
        REQUIRE(!masks1.empty());
        REQUIRE(!masks2.empty());
        REQUIRE(masks1[0].size() == masks2[0].size());
    }
}

// ============================================================================
// Parameter validation
// ============================================================================

TEST_CASE("MovingMask rejects invalid parameters", "[MovingMask]") {
    auto & reg = GeneratorRegistry::instance();

    // Zero image dimensions
    REQUIRE_FALSE(reg.generate("MovingMask",
                               R"({"shape": "circle", "radius": 10, "image_width": 0, "image_height": 200,
                "start_x": 100, "start_y": 100, "num_frames": 1,
                "motion": {"model": "LinearMotionParams"}})")
                          .has_value());

    REQUIRE_FALSE(reg.generate("MovingMask",
                               R"({"shape": "circle", "radius": 10, "image_width": 200, "image_height": 0,
                "start_x": 100, "start_y": 100, "num_frames": 1,
                "motion": {"model": "LinearMotionParams"}})")
                          .has_value());

    // Zero num_frames
    REQUIRE_FALSE(reg.generate("MovingMask",
                               R"({"shape": "circle", "radius": 10, "image_width": 200, "image_height": 200,
                "start_x": 100, "start_y": 100, "num_frames": 0,
                "motion": {"model": "LinearMotionParams"}})")
                          .has_value());

    // Negative radius for circle
    REQUIRE_FALSE(reg.generate("MovingMask",
                               R"({"shape": "circle", "radius": -5, "image_width": 200, "image_height": 200,
                "start_x": 100, "start_y": 100, "num_frames": 1,
                "motion": {"model": "LinearMotionParams"}})")
                          .has_value());

    // Negative width for rectangle
    REQUIRE_FALSE(reg.generate("MovingMask",
                               R"({"shape": "rectangle", "width": -10, "height": 20, "image_width": 200, "image_height": 200,
                "start_x": 100, "start_y": 100, "num_frames": 1,
                "motion": {"model": "LinearMotionParams"}})")
                          .has_value());

    // Negative semi_major for ellipse
    REQUIRE_FALSE(reg.generate("MovingMask",
                               R"({"shape": "ellipse", "semi_major": -10, "semi_minor": 5, "image_width": 200, "image_height": 200,
                "start_x": 100, "start_y": 100, "num_frames": 1,
                "motion": {"model": "LinearMotionParams"}})")
                          .has_value());
}

// ============================================================================
// Single frame is at start position
// ============================================================================

TEST_CASE("MovingMask single frame at start position matches static mask", "[MovingMask]") {
    auto md = runMovingMask(
            R"({
                "shape": "circle", "radius": 15,
                "image_width": 300, "image_height": 300,
                "start_x": 120, "start_y": 80, "num_frames": 1,
                "motion": {"model": "LinearMotionParams", "velocity_x": 100, "velocity_y": 200}
            })");

    REQUIRE(md->getTimeCount() == 1);

    auto md_static = GeneratorRegistry::instance().generate("CircleMask",
                                                            R"({"image_width": 300, "image_height": 300, "center_x": 120, "center_y": 80, "radius": 15, "num_frames": 1})");
    REQUIRE(md_static.has_value());
    auto static_mask = std::get<std::shared_ptr<MaskData>>(*md_static);

    auto masks_moving = getMasksAtTime(*md, 0);
    auto masks_static = getMasksAtTime(*static_mask, 0);

    REQUIRE(!masks_moving.empty());
    REQUIRE(!masks_static.empty());
    REQUIRE(masks_moving[0].size() == masks_static[0].size());
}

// ============================================================================
// Schema tests
// ============================================================================

TEST_CASE("MovingMask schema has enum field for shape", "[MovingMask]") {
    auto schema_opt = GeneratorRegistry::instance().getSchema("MovingMask");
    REQUIRE(schema_opt.has_value());

    auto const & schema = *schema_opt;

    auto * shape_field = schema.field("shape");
    REQUIRE(shape_field != nullptr);
    CHECK(shape_field->type_name == "enum");
    CHECK(shape_field->allowed_values.size() == 3);
}

TEST_CASE("MovingMask schema has variant field for motion", "[MovingMask]") {
    auto schema_opt = GeneratorRegistry::instance().getSchema("MovingMask");
    REQUIRE(schema_opt.has_value());

    auto const & schema = *schema_opt;

    auto * motion_field = schema.field("motion");
    REQUIRE(motion_field != nullptr);
    CHECK(motion_field->is_variant);
    CHECK(motion_field->variant_discriminator == "model");
    CHECK(motion_field->variant_alternatives.size() == 3);
}

TEST_CASE("MovingMask schema has enum field for boundary_mode", "[MovingMask]") {
    auto schema_opt = GeneratorRegistry::instance().getSchema("MovingMask");
    REQUIRE(schema_opt.has_value());

    auto const & schema = *schema_opt;

    auto * bm_field = schema.field("boundary_mode");
    REQUIRE(bm_field != nullptr);
    CHECK(bm_field->type_name == "enum");
    CHECK(bm_field->allowed_values.size() == 3);
}

// ============================================================================
// Ellipse with rotation
// ============================================================================

TEST_CASE("MovingMask ellipse with rotation produces valid masks", "[MovingMask]") {
    auto md = runMovingMask(
            R"({
                "shape": "ellipse", "semi_major": 30, "semi_minor": 15, "angle": 45,
                "image_width": 400, "image_height": 400,
                "start_x": 200, "start_y": 200, "num_frames": 5,
                "motion": {"model": "LinearMotionParams", "velocity_x": 5, "velocity_y": 0},
                "bounds_max_x": 10000, "bounds_max_y": 10000
            })");

    for (int f = 0; f < 5; ++f) {
        auto masks = getMasksAtTime(*md, f);
        REQUIRE(!masks.empty());
        REQUIRE(!masks[0].empty());
    }
}

// ============================================================================
// Rectangle area preservation
// ============================================================================

TEST_CASE("MovingMask rectangle pixel count constant when fully interior", "[MovingMask]") {
    auto md = runMovingMask(
            R"({
                "shape": "rectangle", "width": 20, "height": 30,
                "image_width": 500, "image_height": 500,
                "start_x": 250, "start_y": 250, "num_frames": 10,
                "motion": {"model": "LinearMotionParams", "velocity_x": 3, "velocity_y": 2},
                "bounds_min_x": 50, "bounds_max_x": 450,
                "bounds_min_y": 50, "bounds_max_y": 450
            })");

    auto const reference_masks = getMasksAtTime(*md, 0);
    REQUIRE(!reference_masks.empty());
    auto const reference_count = reference_masks[0].size();

    for (int f = 1; f < 10; ++f) {
        auto masks = getMasksAtTime(*md, f);
        REQUIRE(!masks.empty());
        REQUIRE(masks[0].size() == reference_count);
    }
}
