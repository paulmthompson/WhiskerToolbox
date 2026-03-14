/**
 * @file RectangleMaskGenerator.test.cpp
 * @brief Unit tests for the RectangleMask generator.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "Masks/Mask_Data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace WhiskerToolbox::DataSynthesizer;

static std::shared_ptr<MaskData> runRectangleMask(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("RectangleMask", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<MaskData>>(*result);
}

static std::vector<Mask2D> getMasksAtTime(MaskData const & md, int frame) {
    auto range = md.getAtTime(TimeFrameIndex(frame));
    return {range.begin(), range.end()};
}

TEST_CASE("RectangleMask produces correct number of frames", "[RectangleMask]") {
    auto md = runRectangleMask(R"({"image_width": 200, "image_height": 200, "center_x": 50, "center_y": 50, "width": 10, "height": 10, "num_frames": 7})");
    REQUIRE(md->getTimeCount() == 7);
}

TEST_CASE("RectangleMask produces non-empty masks", "[RectangleMask]") {
    auto md = runRectangleMask(R"({"image_width": 200, "image_height": 200, "center_x": 50, "center_y": 50, "width": 10, "height": 8, "num_frames": 1})");
    auto masks = getMasksAtTime(*md, 0);
    REQUIRE(!masks.empty());
    REQUIRE(masks[0].size() > 0);
}

TEST_CASE("RectangleMask pixel count approximates width*height", "[RectangleMask]") {
    auto md = runRectangleMask(R"({"image_width": 300, "image_height": 300, "center_x": 100, "center_y": 100, "width": 20, "height": 10, "num_frames": 1})");
    auto masks = getMasksAtTime(*md, 0);
    REQUIRE(!masks.empty());

    auto const pixel_count = masks[0].size();
    // Integer rasterization: floor(center ± half) → approx 21 * 11 = 231
    REQUIRE(pixel_count >= 200);
    REQUIRE(pixel_count <= 250);
}

TEST_CASE("RectangleMask all frames have identical masks", "[RectangleMask]") {
    auto md = runRectangleMask(R"({"image_width": 200, "image_height": 200, "center_x": 50, "center_y": 50, "width": 10, "height": 10, "num_frames": 4})");
    auto masks_0 = getMasksAtTime(*md, 0);
    auto masks_2 = getMasksAtTime(*md, 2);
    REQUIRE(!masks_0.empty());
    REQUIRE(!masks_2.empty());
    REQUIRE(masks_0[0].size() == masks_2[0].size());
}

TEST_CASE("RectangleMask rejects invalid parameters", "[RectangleMask]") {
    auto & reg = GeneratorRegistry::instance();
    REQUIRE_FALSE(reg.generate("RectangleMask", R"({"image_width": 200, "image_height": 200, "center_x": 50, "center_y": 50, "width": 10, "height": 10, "num_frames": 0})").has_value());
    REQUIRE_FALSE(reg.generate("RectangleMask", R"({"image_width": 200, "image_height": 200, "center_x": 50, "center_y": 50, "width": -5, "height": 10, "num_frames": 1})").has_value());
    REQUIRE_FALSE(reg.generate("RectangleMask", R"({"image_width": 200, "image_height": 200, "center_x": 50, "center_y": 50, "width": 10, "height": -5, "num_frames": 1})").has_value());
}

TEST_CASE("RectangleMask clips pixels at image boundary", "[RectangleMask]") {
    // Rectangle centered well inside: no clipping
    auto md_interior = runRectangleMask(R"({"image_width": 300, "image_height": 300, "center_x": 150, "center_y": 150, "width": 40, "height": 20, "num_frames": 1})");
    auto masks_interior = getMasksAtTime(*md_interior, 0);
    REQUIRE(!masks_interior.empty());
    auto const interior_count = masks_interior[0].size();

    // Rectangle extending past the bottom-right corner
    auto md_edge = runRectangleMask(R"({"image_width": 300, "image_height": 300, "center_x": 290, "center_y": 290, "width": 40, "height": 20, "num_frames": 1})");
    auto masks_edge = getMasksAtTime(*md_edge, 0);
    REQUIRE(!masks_edge.empty());
    REQUIRE(masks_edge[0].size() < interior_count);

    // Verify no pixel exceeds image bounds
    for (auto const & p: masks_edge[0]) {
        REQUIRE(p.x < 300);
        REQUIRE(p.y < 300);
    }
}
