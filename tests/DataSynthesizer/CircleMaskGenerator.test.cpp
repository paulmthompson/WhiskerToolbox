/**
 * @file CircleMaskGenerator.test.cpp
 * @brief Unit tests for the CircleMask generator.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "Masks/Mask_Data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>
#include <vector>

using namespace WhiskerToolbox::DataSynthesizer;

static std::shared_ptr<MaskData> runCircleMask(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("CircleMask", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<MaskData>>(*result);
}

static std::vector<Mask2D> getMasksAtTime(MaskData const & md, int frame) {
    auto range = md.getAtTime(TimeFrameIndex(frame));
    return {range.begin(), range.end()};
}

TEST_CASE("CircleMask produces correct number of frames", "[CircleMask]") {
    auto md = runCircleMask(R"({"image_width": 200, "image_height": 200, "center_x": 50, "center_y": 50, "radius": 5, "num_frames": 10})");
    REQUIRE(md->getTimeCount() == 10);
}

TEST_CASE("CircleMask produces non-empty masks", "[CircleMask]") {
    auto md = runCircleMask(R"({"image_width": 200, "image_height": 200, "center_x": 50, "center_y": 50, "radius": 10, "num_frames": 1})");
    auto masks = getMasksAtTime(*md, 0);
    REQUIRE(!masks.empty());
    REQUIRE(masks[0].size() > 0);
}

TEST_CASE("CircleMask pixel count approximates pi*r^2", "[CircleMask]") {
    float const radius = 20.0f;
    auto md = runCircleMask(R"({"image_width": 300, "image_height": 300, "center_x": 100, "center_y": 100, "radius": 20, "num_frames": 1})");
    auto masks = getMasksAtTime(*md, 0);
    REQUIRE(!masks.empty());

    auto const pixel_count = static_cast<float>(masks[0].size());
    float const expected_area = std::numbers::pi_v<float> * radius * radius;

    // Allow 5% tolerance for rasterization
    REQUIRE(pixel_count > expected_area * 0.95f);
    REQUIRE(pixel_count < expected_area * 1.05f);
}

TEST_CASE("CircleMask all frames have identical masks", "[CircleMask]") {
    auto md = runCircleMask(R"({"image_width": 200, "image_height": 200, "center_x": 50, "center_y": 50, "radius": 10, "num_frames": 5})");
    auto masks_0 = getMasksAtTime(*md, 0);
    auto masks_3 = getMasksAtTime(*md, 3);
    REQUIRE(!masks_0.empty());
    REQUIRE(!masks_3.empty());
    REQUIRE(masks_0[0].size() == masks_3[0].size());
}

TEST_CASE("CircleMask rejects invalid parameters", "[CircleMask]") {
    auto & reg = GeneratorRegistry::instance();
    REQUIRE_FALSE(reg.generate("CircleMask", R"({"image_width": 200, "image_height": 200, "center_x": 50, "center_y": 50, "radius": 10, "num_frames": 0})").has_value());
    REQUIRE_FALSE(reg.generate("CircleMask", R"({"image_width": 200, "image_height": 200, "center_x": 50, "center_y": 50, "radius": -5, "num_frames": 1})").has_value());
    REQUIRE_FALSE(reg.generate("CircleMask", R"({"image_width": 0, "image_height": 200, "center_x": 50, "center_y": 50, "radius": 10, "num_frames": 1})").has_value());
}
