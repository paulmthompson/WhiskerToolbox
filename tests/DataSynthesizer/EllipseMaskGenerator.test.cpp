/**
 * @file EllipseMaskGenerator.test.cpp
 * @brief Unit tests for the EllipseMask generator.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "Masks/Mask_Data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
#include <vector>

using namespace WhiskerToolbox::DataSynthesizer;

static std::shared_ptr<MaskData> runEllipseMask(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("EllipseMask", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<MaskData>>(*result);
}

static std::vector<Mask2D> getMasksAtTime(MaskData const & md, int frame) {
    auto range = md.getAtTime(TimeFrameIndex(frame));
    return {range.begin(), range.end()};
}

TEST_CASE("EllipseMask produces correct number of frames", "[EllipseMask]") {
    auto md = runEllipseMask(R"({"image_width": 300, "image_height": 300, "center_x": 100, "center_y": 100, "semi_major": 20, "semi_minor": 10, "num_frames": 8})");
    REQUIRE(md->getTimeCount() == 8);
}

TEST_CASE("EllipseMask produces non-empty masks", "[EllipseMask]") {
    auto md = runEllipseMask(R"({"image_width": 300, "image_height": 300, "center_x": 100, "center_y": 100, "semi_major": 15, "semi_minor": 10, "num_frames": 1})");
    auto masks = getMasksAtTime(*md, 0);
    REQUIRE(!masks.empty());
    REQUIRE(masks[0].size() > 0);
}

TEST_CASE("EllipseMask pixel count approximates pi*a*b", "[EllipseMask]") {
    float const a = 30.0f;
    float const b = 15.0f;
    auto md = runEllipseMask(R"({"image_width": 500, "image_height": 500, "center_x": 200, "center_y": 200, "semi_major": 30, "semi_minor": 15, "num_frames": 1})");
    auto masks = getMasksAtTime(*md, 0);
    REQUIRE(!masks.empty());

    auto const pixel_count = static_cast<float>(masks[0].size());
    float const expected_area = std::numbers::pi_v<float> * a * b;

    REQUIRE(pixel_count > expected_area * 0.93f);
    REQUIRE(pixel_count < expected_area * 1.07f);
}

TEST_CASE("EllipseMask axis-aligned matches generate_ellipse_pixels output", "[EllipseMask]") {
    auto md = runEllipseMask(R"({"image_width": 300, "image_height": 300, "center_x": 100, "center_y": 100, "semi_major": 20, "semi_minor": 10, "num_frames": 1})");
    auto masks = getMasksAtTime(*md, 0);
    REQUIRE(!masks.empty());

    auto const expected_pixels = generate_ellipse_pixels(100.0f, 100.0f, 20.0f, 10.0f);
    REQUIRE(masks[0].size() == expected_pixels.size());
}

TEST_CASE("EllipseMask rotated preserves area", "[EllipseMask]") {
    auto md_0 = runEllipseMask(R"({"image_width": 500, "image_height": 500, "center_x": 200, "center_y": 200, "semi_major": 30, "semi_minor": 15, "angle": 0, "num_frames": 1})");
    auto md_45 = runEllipseMask(R"({"image_width": 500, "image_height": 500, "center_x": 200, "center_y": 200, "semi_major": 30, "semi_minor": 15, "angle": 45, "num_frames": 1})");

    auto masks_0 = getMasksAtTime(*md_0, 0);
    auto masks_45 = getMasksAtTime(*md_45, 0);
    REQUIRE(!masks_0.empty());
    REQUIRE(!masks_45.empty());

    auto const area_0 = static_cast<float>(masks_0[0].size());
    auto const area_45 = static_cast<float>(masks_45[0].size());

    REQUIRE(area_45 > area_0 * 0.95f);
    REQUIRE(area_45 < area_0 * 1.05f);
}

TEST_CASE("EllipseMask rejects invalid parameters", "[EllipseMask]") {
    auto & reg = GeneratorRegistry::instance();
    REQUIRE_FALSE(reg.generate("EllipseMask", R"({"image_width": 300, "image_height": 300, "center_x": 100, "center_y": 100, "semi_major": 20, "semi_minor": 10, "num_frames": 0})").has_value());
    REQUIRE_FALSE(reg.generate("EllipseMask", R"({"image_width": 300, "image_height": 300, "center_x": 100, "center_y": 100, "semi_major": -5, "semi_minor": 10, "num_frames": 1})").has_value());
    REQUIRE_FALSE(reg.generate("EllipseMask", R"({"image_width": 300, "image_height": 300, "center_x": 100, "center_y": 100, "semi_major": 20, "semi_minor": -5, "num_frames": 1})").has_value());
}
