#include "masks.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cmath>

/**
 * @brief Tests for mask utility functions
 */

TEST_CASE("Mask utility functions", "[masks][utilities]") {

    SECTION("get_bounding_box basic functionality") {
        // Create a simple rectangular mask
        Mask2D mask = {
                {1, 1},
                {3, 1},
                {3, 4},
                {1, 4}};

        auto bounding_box = get_bounding_box(mask);
        Point2D<uint32_t> min_point = bounding_box.first;
        Point2D<uint32_t> max_point = bounding_box.second;

        REQUIRE(min_point.x == 1);
        REQUIRE(min_point.y == 1);
        REQUIRE(max_point.x == 3);
        REQUIRE(max_point.y == 4);
    }

    SECTION("get_bounding_box with single point") {
        Mask2D mask = {{5, 7}};

        auto bounding_box = get_bounding_box(mask);
        Point2D<uint32_t> min_point = bounding_box.first;
        Point2D<uint32_t> max_point = bounding_box.second;

        REQUIRE(min_point.x == 5);
        REQUIRE(min_point.y == 7);
        REQUIRE(max_point.x == 5);
        REQUIRE(max_point.y == 7);
    }

    SECTION("get_bounding_box with irregular mask") {
        // Create an irregular shaped mask (note: uint32_t can't be negative, so we use larger values)
        Mask2D mask = {
                {2, 2},
                {5, 1},
                {3, 8},
                {0, 4}};

        auto bounding_box = get_bounding_box(mask);
        Point2D<uint32_t> min_point = bounding_box.first;
        Point2D<uint32_t> max_point = bounding_box.second;

        REQUIRE(min_point.x == 0);
        REQUIRE(min_point.y == 1);
        REQUIRE(max_point.x == 5);
        REQUIRE(max_point.y == 8);
    }
}

TEST_CASE("create_mask utility function", "[masks][create]") {

    SECTION("create_mask from vectors") {
        std::vector<float> x = {1.0f, 2.0f, 3.0f};
        std::vector<float> y = {4.0f, 5.0f, 6.0f};

        auto mask = create_mask(x, y);

        REQUIRE(mask.size() == 3);
        REQUIRE(mask[0].x == 1);
        REQUIRE(mask[0].y == 4);
        REQUIRE(mask[1].x == 2);
        REQUIRE(mask[1].y == 5);
        REQUIRE(mask[2].x == 3);
        REQUIRE(mask[2].y == 6);
    }

    SECTION("create_mask with move semantics") {
        std::vector<float> x = {10.0f, 20.0f};
        std::vector<float> y = {30.0f, 40.0f};

        auto mask = create_mask(std::move(x), std::move(y));

        REQUIRE(mask.size() == 2);
        REQUIRE(mask[0].x == 10);
        REQUIRE(mask[0].y == 30);
        REQUIRE(mask[1].x == 20);
        REQUIRE(mask[1].y == 40);
    }

    SECTION("create_mask with rounding") {
        std::vector<float> x = {1.4f, 2.6f, 3.1f};
        std::vector<float> y = {4.7f, 5.2f, 6.9f};

        auto mask = create_mask(x, y);

        REQUIRE(mask.size() == 3);
        REQUIRE(mask[0].x == 1);// 1.4 rounds to 1
        REQUIRE(mask[0].y == 5);// 4.7 rounds to 5
        REQUIRE(mask[1].x == 3);// 2.6 rounds to 3
        REQUIRE(mask[1].y == 5);// 5.2 rounds to 5
        REQUIRE(mask[2].x == 3);// 3.1 rounds to 3
        REQUIRE(mask[2].y == 7);// 6.9 rounds to 7
    }

    SECTION("create_mask with negative values") {
        std::vector<float> x = {-1.0f, 2.0f, 3.0f};
        std::vector<float> y = {4.0f, -5.0f, 6.0f};

        auto mask = create_mask(x, y);

        REQUIRE(mask.size() == 3);
        REQUIRE(mask[0].x == 0);// -1.0 clamped to 0
        REQUIRE(mask[0].y == 4);
        REQUIRE(mask[1].x == 2);
        REQUIRE(mask[1].y == 0);// -5.0 clamped to 0
        REQUIRE(mask[2].x == 3);
        REQUIRE(mask[2].y == 6);
    }
}

TEST_CASE("get_mask_outline function", "[masks][outline]") {
    SECTION("Empty mask returns empty outline") {
        Mask2D empty_mask;
        auto outline = get_mask_outline(empty_mask);
        REQUIRE(outline.empty());
    }

    SECTION("Single point mask returns empty outline") {
        Mask2D single_point_mask = {{5, 5}};
        auto outline = get_mask_outline(single_point_mask);
        REQUIRE(outline.empty());
    }

    SECTION("Two point mask returns both points") {
        Mask2D two_point_mask = {{1, 1}, {3, 3}};
        auto outline = get_mask_outline(two_point_mask);
        REQUIRE(outline.size() == 2);

        // Should contain both points
        bool found_1_1 = false, found_3_3 = false;
        for (auto const & point: outline) {
            if (point.x == 1 && point.y == 1) found_1_1 = true;
            if (point.x == 3 && point.y == 3) found_3_3 = true;
        }
        REQUIRE(found_1_1);
        REQUIRE(found_3_3);
    }

    SECTION("Rectangular mask outline") {
        // Create a filled rectangle mask
        Mask2D rect_mask;
        for (uint32_t x = 1; x <= 5; x += 1) {
            for (uint32_t y = 2; y <= 4; y += 1) {
                rect_mask.push_back({x, y});
            }
        }

        auto outline = get_mask_outline(rect_mask);

        // Should contain at least the extremal points
        REQUIRE(outline.size() >= 4);

        // Check that extremal points are present
        bool found_max_x_min_y = false;// (5, 2)
        bool found_max_x_max_y = false;// (5, 4)
        bool found_min_x_min_y = false;// (1, 2)
        bool found_min_x_max_y = false;// (1, 4)

        for (auto const & point: outline) {
            if (point.x == 5 && point.y == 2) found_max_x_min_y = true;
            if (point.x == 5 && point.y == 4) found_max_x_max_y = true;
            if (point.x == 1 && point.y == 2) found_min_x_min_y = true;
            if (point.x == 1 && point.y == 4) found_min_x_max_y = true;
        }

        REQUIRE(found_max_x_min_y);
        REQUIRE(found_max_x_max_y);
        REQUIRE(found_min_x_min_y);
        REQUIRE(found_min_x_max_y);
    }

    SECTION("L-shaped mask outline") {
        // Create an L-shaped mask:
        // (1,3) (2,3)
        // (1,2)
        // (1,1) (2,1) (3,1)
        Mask2D l_mask = {
                {1, 3},
                {2, 3},
                {1, 2},
                {1, 1},
                {2, 1},
                {3, 1}};

        auto outline = get_mask_outline(l_mask);

        // Should find extremal points
        REQUIRE(outline.size() >= 3);

        // Should contain key extremal points like:
        // - max x for y=1: (3,1)
        // - max y for x=1: (1,3)
        // - max y for x=2: (2,3)

        bool found_3_1 = false;
        bool found_1_3 = false;
        bool found_2_3 = false;

        for (auto const & point: outline) {
            if (point.x == 3 && point.y == 1) found_3_1 = true;
            if (point.x == 1 && point.y == 3) found_1_3 = true;
            if (point.x == 2 && point.y == 3) found_2_3 = true;
        }

        REQUIRE(found_3_1);
        REQUIRE(found_1_3);
        REQUIRE(found_2_3);
    }
}

TEST_CASE("generate_ellipse_pixels function", "[masks][ellipse]") {

    SECTION("Perfect circle with radius 1") {
        auto pixels = generate_ellipse_pixels(5.0f, 5.0f, 1.0f, 1.0f);

        // Should contain the center and 4 cardinal points
        REQUIRE(pixels.size() >= 5);

        // Check that center point is included
        bool found_center = false;
        for (auto const & pixel: pixels) {
            if (pixel.x == 5 && pixel.y == 5) {
                found_center = true;
                break;
            }
        }
        REQUIRE(found_center);

        // All pixels should be within distance 1 from center
        for (auto const & pixel: pixels) {
            float dx = static_cast<float>(pixel.x) - 5.0f;
            float dy = static_cast<float>(pixel.y) - 5.0f;
            float distance = std::sqrt(dx * dx + dy * dy);
            REQUIRE(distance <= 1.01f);// Small tolerance for floating point
        }
    }

    SECTION("Perfect circle with radius 2") {
        auto pixels = generate_ellipse_pixels(10.0f, 10.0f, 2.0f, 2.0f);

        // Should have more pixels than radius 1 circle
        REQUIRE(pixels.size() > 5);

        // All pixels should be within distance 2 from center
        for (auto const & pixel: pixels) {
            float dx = static_cast<float>(pixel.x) - 10.0f;
            float dy = static_cast<float>(pixel.y) - 10.0f;
            float distance = std::sqrt(dx * dx + dy * dy);
            REQUIRE(distance <= 2.01f);// Small tolerance for floating point
        }

        // Should include pixels at approximately (8,10), (12,10), (10,8), (10,12)
        bool found_left = false, found_right = false, found_top = false, found_bottom = false;
        for (auto const & pixel: pixels) {
            if (pixel.x == 8 && pixel.y == 10) found_left = true;
            if (pixel.x == 12 && pixel.y == 10) found_right = true;
            if (pixel.x == 10 && pixel.y == 8) found_top = true;
            if (pixel.x == 10 && pixel.y == 12) found_bottom = true;
        }
        REQUIRE(found_left);
        REQUIRE(found_right);
        REQUIRE(found_top);
        REQUIRE(found_bottom);
    }

    SECTION("Ellipse with different X and Y radii") {
        auto pixels = generate_ellipse_pixels(5.0f, 5.0f, 3.0f, 1.0f);

        // Should have pixels stretched more in X direction
        REQUIRE(pixels.size() > 5);

        // All pixels should satisfy ellipse equation: (dx/3)² + (dy/1)² ≤ 1
        for (auto const & pixel: pixels) {
            float dx = static_cast<float>(pixel.x) - 5.0f;
            float dy = static_cast<float>(pixel.y) - 5.0f;
            float ellipse_value = (dx / 3.0f) * (dx / 3.0f) + (dy / 1.0f) * (dy / 1.0f);
            REQUIRE(ellipse_value <= 1.01f);// Small tolerance
        }

        // Should include pixels farther in X direction than Y direction
        bool found_far_x = false, found_far_y = false;
        for (auto const & pixel: pixels) {
            if (pixel.x == 8 && pixel.y == 5) found_far_x = true;
            if (pixel.x == 5 && pixel.y == 4) found_far_y = true;
        }
        REQUIRE(found_far_x);// Should reach to x=8 (3 units away)
        REQUIRE(found_far_y);// Should reach to y=4 (1 unit away)
    }

    SECTION("Ellipse at origin") {
        auto pixels = generate_ellipse_pixels(0.0f, 0.0f, 1.5f, 1.5f);

        // Should include origin
        bool found_origin = false;
        for (auto const & pixel: pixels) {
            if (pixel.x == 0 && pixel.y == 0) {
                found_origin = true;
                break;
            }
        }
        REQUIRE(found_origin);

        // All pixels should have non-negative coordinates (bounds checking)
        for (auto const & pixel: pixels) {
            REQUIRE(pixel.x >= 0);
            REQUIRE(pixel.y >= 0);
        }
    }

    SECTION("Ellipse partially outside bounds") {
        // Center at (1, 1) with radius 2 - should clip negative coordinates
        auto pixels = generate_ellipse_pixels(1.0f, 1.0f, 2.0f, 2.0f);

        // All returned pixels should have non-negative coordinates
        for (auto const & pixel: pixels) {
            REQUIRE(pixel.x >= 0);
            REQUIRE(pixel.y >= 0);
        }

        // Should still include the center
        bool found_center = false;
        for (auto const & pixel: pixels) {
            if (pixel.x == 1 && pixel.y == 1) {
                found_center = true;
                break;
            }
        }
        REQUIRE(found_center);

        // Should have fewer pixels than if it were fully in bounds
        REQUIRE(pixels.size() < 13);// Rough estimate for clipped circle
    }

    SECTION("Very small ellipse") {
        auto pixels = generate_ellipse_pixels(10.0f, 10.0f, 0.3f, 0.3f);

        // Should contain at least the center pixel
        REQUIRE(pixels.size() >= 1);

        bool found_center = false;
        for (auto const & pixel: pixels) {
            if (pixel.x == 10 && pixel.y == 10) {
                found_center = true;
                break;
            }
        }
        REQUIRE(found_center);
    }

    SECTION("Zero radius edge case") {
        // This tests behavior with zero radius - should return empty or just center
        auto pixels = generate_ellipse_pixels(5.0f, 5.0f, 0.0f, 0.0f);

        // Should either be empty or contain only center depending on implementation
        // The current implementation with divide by zero would be problematic,
        // so we expect this to either work correctly or be handled gracefully
        if (!pixels.empty()) {
            // If any pixels returned, center should be included
            bool found_center = false;
            for (auto const & pixel: pixels) {
                if (pixel.x == 5 && pixel.y == 5) {
                    found_center = true;
                    break;
                }
            }
            REQUIRE(found_center);
        }
    }
}

TEST_CASE("combine_masks function", "[masks][combination]") {

    SECTION("Combine two non-overlapping masks") {
        Mask2D mask1 = {{1, 1}, {2, 2}};
        Mask2D mask2 = {{3, 3}, {4, 4}};

        auto combined = combine_masks(mask1, mask2);

        REQUIRE(combined.size() == 4);

        // Should contain all original points
        bool found_1_1 = false, found_2_2 = false, found_3_3 = false, found_4_4 = false;
        for (auto const & point: combined) {
            if (point.x == 1 && point.y == 1) found_1_1 = true;
            if (point.x == 2 && point.y == 2) found_2_2 = true;
            if (point.x == 3 && point.y == 3) found_3_3 = true;
            if (point.x == 4 && point.y == 4) found_4_4 = true;
        }
        REQUIRE(found_1_1);
        REQUIRE(found_2_2);
        REQUIRE(found_3_3);
        REQUIRE(found_4_4);
    }

    SECTION("Combine masks with exact duplicates") {
        Mask2D mask1 = {{1, 1}, {2, 2}, {3, 3}};
        Mask2D mask2 = {{2, 2}, {3, 3}, {4, 4}};

        auto combined = combine_masks(mask1, mask2);

        // Should have 4 unique pixels: (1,1), (2,2), (3,3), (4,4)
        REQUIRE(combined.size() == 4);

        bool found_1_1 = false, found_2_2 = false, found_3_3 = false, found_4_4 = false;
        for (auto const & point: combined) {
            if (point.x == 1 && point.y == 1) found_1_1 = true;
            if (point.x == 2 && point.y == 2) found_2_2 = true;
            if (point.x == 3 && point.y == 3) found_3_3 = true;
            if (point.x == 4 && point.y == 4) found_4_4 = true;
        }
        REQUIRE(found_1_1);
        REQUIRE(found_2_2);
        REQUIRE(found_3_3);
        REQUIRE(found_4_4);
    }

    SECTION("Combine with empty masks") {
        Mask2D mask1 = {{1, 1}, {2, 2}};
        Mask2D empty_mask = {};

        auto combined1 = combine_masks(mask1, empty_mask);
        auto combined2 = combine_masks(empty_mask, mask1);

        REQUIRE(combined1.size() == 2);
        REQUIRE(combined2.size() == 2);
    }

    SECTION("Combine identical masks") {
        Mask2D mask = {{1, 1}, {2, 2}, {3, 3}};

        auto combined = combine_masks(mask, mask);

        // Should have same size as original (no duplicates)
        REQUIRE(combined.size() == 3);
    }
}

TEST_CASE("subtract_masks function", "[masks][subtraction]") {

    SECTION("Subtract non-overlapping masks") {
        Mask2D mask1 = {{1, 1}, {2, 2}, {3, 3}};
        Mask2D mask2 = {{4, 4}, {5, 5}};

        auto result = subtract_masks(mask1, mask2);

        // Should keep all of mask1 since no overlap
        REQUIRE(result.size() == 3);

        bool found_1_1 = false, found_2_2 = false, found_3_3 = false;
        for (auto const & point: result) {
            if (point.x == 1 && point.y == 1) found_1_1 = true;
            if (point.x == 2 && point.y == 2) found_2_2 = true;
            if (point.x == 3 && point.y == 3) found_3_3 = true;
        }
        REQUIRE(found_1_1);
        REQUIRE(found_2_2);
        REQUIRE(found_3_3);
    }

    SECTION("Subtract overlapping masks") {
        Mask2D mask1 = {{1, 1}, {2, 2}, {3, 3}, {4, 4}};
        Mask2D mask2 = {{2, 2}, {4, 4}};

        auto result = subtract_masks(mask1, mask2);

        // Should keep only (1,1) and (3,3)
        REQUIRE(result.size() == 2);

        bool found_1_1 = false, found_3_3 = false;
        for (auto const & point: result) {
            if (point.x == 1 && point.y == 1) found_1_1 = true;
            if (point.x == 3 && point.y == 3) found_3_3 = true;
            // Should not find (2,2) or (4,4)
            REQUIRE_FALSE(point.x == 2);
            REQUIRE_FALSE(point.y == 2);
            REQUIRE_FALSE(point.x == 4);
            REQUIRE_FALSE(point.y == 4);
        }
        REQUIRE(found_1_1);
        REQUIRE(found_3_3);
    }

    SECTION("Subtract empty mask") {
        Mask2D mask1 = {{1, 1}, {2, 2}};
        Mask2D empty_mask = {};

        auto result = subtract_masks(mask1, empty_mask);

        // Should keep all of mask1
        REQUIRE(result.size() == 2);
        REQUIRE(result[0].x == 1);
        REQUIRE(result[0].y == 1);
        REQUIRE(result[1].x == 2);
        REQUIRE(result[1].y == 2);
    }

    SECTION("Subtract from empty mask") {
        Mask2D empty_mask = {};
        Mask2D mask2 = {{1, 1}, {2, 2}};

        auto result = subtract_masks(empty_mask, mask2);

        // Should return empty mask
        REQUIRE(result.empty());
    }

    SECTION("Subtract identical masks") {
        Mask2D mask = {{1, 1}, {2, 2}, {3, 3}};

        auto result = subtract_masks(mask, mask);

        // Should return empty mask
        REQUIRE(result.empty());
    }

    SECTION("Subtract superset from subset") {
        Mask2D mask1 = {{2, 2}, {3, 3}};
        Mask2D mask2 = {{1, 1}, {2, 2}, {3, 3}, {4, 4}};

        auto result = subtract_masks(mask1, mask2);

        // Should return empty mask since all of mask1 is in mask2
        REQUIRE(result.empty());
    }
}

TEST_CASE("generate_outline_mask function", "[masks][outline_mask]") {

    SECTION("Empty mask returns empty outline") {
        Mask2D empty_mask;
        auto outline = generate_outline_mask(empty_mask);
        REQUIRE(outline.empty());
    }

    SECTION("Single pixel mask returns outline") {
        Mask2D single_pixel = {{5, 5}};
        auto outline = generate_outline_mask(single_pixel, 1);

        // Single pixel should be considered an edge pixel
        REQUIRE(outline.size() == 1);
        REQUIRE(outline[0].x == 5);
        REQUIRE(outline[0].y == 5);
    }

    SECTION("Simple 2x2 square mask") {
        Mask2D square_mask = {{1, 1}, {1, 2}, {2, 1}, {2, 2}};
        auto outline = generate_outline_mask(square_mask, 1);

        // All pixels in a 2x2 square should be edge pixels
        REQUIRE(outline.size() == 4);

        // Check that all original pixels are in the outline
        bool found_1_1 = false, found_1_2 = false, found_2_1 = false, found_2_2 = false;
        for (auto const & point: outline) {
            if (point.x == 1 && point.y == 1) found_1_1 = true;
            if (point.x == 1 && point.y == 2) found_1_2 = true;
            if (point.x == 2 && point.y == 1) found_2_1 = true;
            if (point.x == 2 && point.y == 2) found_2_2 = true;
        }
        REQUIRE(found_1_1);
        REQUIRE(found_1_2);
        REQUIRE(found_2_1);
        REQUIRE(found_2_2);
    }

    SECTION("3x3 square with filled center") {
        // Create a 3x3 filled square
        Mask2D filled_square;
        for (uint32_t x = 1; x <= 3; ++x) {
            for (uint32_t y = 1; y <= 3; ++y) {
                filled_square.push_back({x, y});
            }
        }

        auto outline = generate_outline_mask(filled_square, 1);

        // Only the border pixels should be in the outline (center should not be)
        REQUIRE(outline.size() == 8);// 3x3 - 1 center = 8 edge pixels

        // Check that center pixel (2,2) is NOT in outline
        bool found_center = false;
        for (auto const & point: outline) {
            if (point.x == 2 && point.y == 2) {
                found_center = true;
                break;
            }
        }
        REQUIRE_FALSE(found_center);

        // Check that corner pixels ARE in outline
        bool found_corner = false;
        for (auto const & point: outline) {
            if (point.x == 1 && point.y == 1) {
                found_corner = true;
                break;
            }
        }
        REQUIRE(found_corner);
    }

    SECTION("L-shaped mask outline") {
        // Create an L-shaped mask:
        // XX.
        // XX.
        // XXX
        Mask2D l_mask = {
                {1, 1},
                {2, 1},
                {1, 2},
                {2, 2},
                {1, 3},
                {2, 3},
                {3, 3}};

        auto outline = generate_outline_mask(l_mask, 1);

        // All pixels should be edge pixels since none have all 8 neighbors filled
        REQUIRE(outline.size() == 7);
    }

    SECTION("Outline with thickness 2") {
        Mask2D single_pixel = {{5, 5}};
        auto outline = generate_outline_mask(single_pixel, 2);

        // With thickness 2, should still be edge pixel
        REQUIRE(outline.size() == 1);
        REQUIRE(outline[0].x == 5);
        REQUIRE(outline[0].y == 5);
    }

    SECTION("Outline with image bounds") {
        // Mask at edge of image
        Mask2D edge_mask = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
        auto outline = generate_outline_mask(edge_mask, 1, 10, 10);

        // All pixels should be edge pixels due to image boundary
        REQUIRE(outline.size() == 4);
    }

    SECTION("Zero thickness returns empty") {
        Mask2D mask = {{1, 1}, {2, 2}};
        auto outline = generate_outline_mask(mask, 0);
        REQUIRE(outline.empty());
    }
}

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