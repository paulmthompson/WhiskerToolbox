#include "CoreGeometry/masks.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cmath>

/**
 * @brief Tests for mask utility functions
 */


TEST_CASE("resize_mask function", "[masks][resize]") {

    SECTION("Empty mask returns empty") {
        Mask2D empty_mask;
        ImageSize source_size{10, 10};
        ImageSize dest_size{20, 20};

        auto resized = resize_mask(empty_mask, source_size, dest_size);
        REQUIRE(resized.empty());
    }

    SECTION("Invalid source size returns empty") {
        Mask2D mask = {{1, 1}, {2, 2}};
        ImageSize invalid_source{0, 10};
        ImageSize dest_size{20, 20};

        auto resized = resize_mask(mask, invalid_source, dest_size);
        REQUIRE(resized.empty());
    }

    SECTION("Invalid destination size returns empty") {
        Mask2D mask = {{1, 1}, {2, 2}};
        ImageSize source_size{10, 10};
        ImageSize invalid_dest{20, -5};

        auto resized = resize_mask(mask, source_size, invalid_dest);
        REQUIRE(resized.empty());
    }

    SECTION("Same size returns copy") {
        Mask2D mask = {{1, 1}, {2, 2}, {5, 7}};
        ImageSize same_size{10, 10};

        auto resized = resize_mask(mask, same_size, same_size);
        REQUIRE(resized.size() == mask.size());

        // Should have same points
        for (size_t i = 0; i < mask.size(); ++i) {
            REQUIRE(resized[i].x == mask[i].x);
            REQUIRE(resized[i].y == mask[i].y);
        }
    }

    SECTION("Upscaling 2x preserves relative positions") {
        // Create a simple 2x2 mask in a 4x4 source image
        Mask2D mask = {{1, 1}, {2, 1}, {1, 2}, {2, 2}};
        ImageSize source_size{4, 4};
        ImageSize dest_size{8, 8};

        auto resized = resize_mask(mask, source_size, dest_size);

        // With 2x scaling, expect approximately 4x as many pixels (but could vary due to nearest neighbor)
        REQUIRE(resized.size() >= 4);

        // Check that scaled coordinates are roughly in the expected range
        // Original mask was in [1,2] x [1,2], scaled should be roughly in [2,5] x [2,5]
        // (allowing for nearest neighbor interpolation effects)
        for (auto const & point: resized) {
            REQUIRE(point.x >= 2);
            REQUIRE(point.x <= 5);// Relaxed from 4 to 5 to account for interpolation
            REQUIRE(point.y >= 2);
            REQUIRE(point.y <= 5);// Relaxed from 4 to 5 to account for interpolation
        }
    }

    SECTION("Downscaling preserves general mask area") {
        // Create a larger mask in a 20x20 source
        Mask2D mask;
        // Fill a 10x10 square from (5,5) to (14,14)
        for (uint32_t x = 5; x <= 14; ++x) {
            for (uint32_t y = 5; y <= 14; ++y) {
                mask.push_back({x, y});
            }
        }

        ImageSize source_size{20, 20};
        ImageSize dest_size{10, 10};

        auto resized = resize_mask(mask, source_size, dest_size);

        // Should have some pixels in the center area
        REQUIRE(!resized.empty());

        // With 0.5x scaling, the center square should be roughly [2,7] x [2,7]
        bool found_center_region = false;
        for (auto const & point: resized) {
            if (point.x >= 2 && point.x <= 7 && point.y >= 2 && point.y <= 7) {
                found_center_region = true;
                break;
            }
        }
        REQUIRE(found_center_region);
    }

    SECTION("Single pixel scaling") {
        Mask2D single_pixel = {{5, 5}};
        ImageSize source_size{10, 10};
        ImageSize dest_size{20, 20};

        auto resized = resize_mask(single_pixel, source_size, dest_size);

        // Should have at least one pixel
        REQUIRE(!resized.empty());

        // Scaled pixel should be roughly at (10, 10) with 2x scaling
        bool found_expected_region = false;
        for (auto const & point: resized) {
            if (point.x >= 9 && point.x <= 11 && point.y >= 9 && point.y <= 11) {
                found_expected_region = true;
                break;
            }
        }
        REQUIRE(found_expected_region);
    }

    SECTION("Aspect ratio change") {
        // Create a horizontal line in a square source, positioned to survive aspect ratio change
        // Use y=2 instead of y=5 so it maps better to the 5-pixel tall destination
        Mask2D horizontal_line = {{2, 2}, {3, 2}, {4, 2}, {5, 2}};
        ImageSize source_size{10, 10};
        ImageSize dest_size{20, 5};// Wide and short destination

        auto resized = resize_mask(horizontal_line, source_size, dest_size);

        // Should have pixels, roughly stretched horizontally and compressed vertically
        REQUIRE(!resized.empty());

        // Y coordinates should be compressed (y=2 in source maps to y=1 in dest)
        // X coordinates should be stretched (x=2-5 in source maps to roughly x=4-10 in dest)
        for (auto const & point: resized) {
            REQUIRE(point.y <= 2); // Should be in upper part due to compression
            REQUIRE(point.x >= 3); // Should be in central/right part due to stretching
            REQUIRE(point.x <= 12);// Upper bound for stretched coordinates
        }
    }

    SECTION("Out of bounds pixels are ignored") {
        // Create mask with points outside source bounds
        Mask2D mask = {{1, 1}, {15, 15}};// Second point is outside 10x10 source
        ImageSize source_size{10, 10};
        ImageSize dest_size{20, 20};

        auto resized = resize_mask(mask, source_size, dest_size);

        // Should only process the valid point (1,1)
        REQUIRE(!resized.empty());

        // All points should be within destination bounds
        for (auto const & point: resized) {
            REQUIRE(point.x < 20);
            REQUIRE(point.y < 20);
        }
    }
}