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
                {1.0f, 1.0f},
                {3.0f, 1.0f},
                {3.0f, 4.0f},
                {1.0f, 4.0f}};

        auto bounding_box = get_bounding_box(mask);
        Point2D<float> min_point = bounding_box.first;
        Point2D<float> max_point = bounding_box.second;

        REQUIRE(min_point.x == 1.0f);
        REQUIRE(min_point.y == 1.0f);
        REQUIRE(max_point.x == 3.0f);
        REQUIRE(max_point.y == 4.0f);
    }

    SECTION("get_bounding_box with single point") {
        Mask2D mask = {{5.0f, 7.0f}};

        auto bounding_box = get_bounding_box(mask);
        Point2D<float> min_point = bounding_box.first;
        Point2D<float> max_point = bounding_box.second;

        REQUIRE(min_point.x == 5.0f);
        REQUIRE(min_point.y == 7.0f);
        REQUIRE(max_point.x == 5.0f);
        REQUIRE(max_point.y == 7.0f);
    }

    SECTION("get_bounding_box with irregular mask") {
        // Create an irregular shaped mask
        Mask2D mask = {
                {0.0f, 2.0f},
                {5.0f, 1.0f},
                {3.0f, 8.0f},
                {-2.0f, 4.0f}};

        auto bounding_box = get_bounding_box(mask);
        Point2D<float> min_point = bounding_box.first;
        Point2D<float> max_point = bounding_box.second;

        REQUIRE(min_point.x == -2.0f);
        REQUIRE(min_point.y == 1.0f);
        REQUIRE(max_point.x == 5.0f);
        REQUIRE(max_point.y == 8.0f);
    }
}

TEST_CASE("create_mask utility function", "[masks][create]") {

    SECTION("create_mask from vectors") {
        std::vector<float> x = {1.0f, 2.0f, 3.0f};
        std::vector<float> y = {4.0f, 5.0f, 6.0f};

        auto mask = create_mask(x, y);

        REQUIRE(mask.size() == 3);
        REQUIRE(mask[0].x == 1.0f);
        REQUIRE(mask[0].y == 4.0f);
        REQUIRE(mask[1].x == 2.0f);
        REQUIRE(mask[1].y == 5.0f);
        REQUIRE(mask[2].x == 3.0f);
        REQUIRE(mask[2].y == 6.0f);
    }

    SECTION("create_mask with move semantics") {
        std::vector<float> x = {10.0f, 20.0f};
        std::vector<float> y = {30.0f, 40.0f};

        auto mask = create_mask(std::move(x), std::move(y));

        REQUIRE(mask.size() == 2);
        REQUIRE(mask[0].x == 10.0f);
        REQUIRE(mask[0].y == 30.0f);
        REQUIRE(mask[1].x == 20.0f);
        REQUIRE(mask[1].y == 40.0f);
    }
}

TEST_CASE("get_mask_outline function", "[masks][outline]") {
    SECTION("Empty mask returns empty outline") {
        Mask2D empty_mask;
        auto outline = get_mask_outline(empty_mask);
        REQUIRE(outline.empty());
    }

    SECTION("Single point mask returns empty outline") {
        Mask2D single_point_mask = {{5.0f, 5.0f}};
        auto outline = get_mask_outline(single_point_mask);
        REQUIRE(outline.empty());
    }

    SECTION("Two point mask returns both points") {
        Mask2D two_point_mask = {{1.0f, 1.0f}, {3.0f, 3.0f}};
        auto outline = get_mask_outline(two_point_mask);
        REQUIRE(outline.size() == 2);

        // Should contain both points
        bool found_1_1 = false, found_3_3 = false;
        for (auto const & point: outline) {
            if (point.x == 1.0f && point.y == 1.0f) found_1_1 = true;
            if (point.x == 3.0f && point.y == 3.0f) found_3_3 = true;
        }
        REQUIRE(found_1_1);
        REQUIRE(found_3_3);
    }

    SECTION("Rectangular mask outline") {
        // Create a filled rectangle mask
        Mask2D rect_mask;
        for (float x = 1.0f; x <= 5.0f; x += 1.0f) {
            for (float y = 2.0f; y <= 4.0f; y += 1.0f) {
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
            if (point.x == 5.0f && point.y == 2.0f) found_max_x_min_y = true;
            if (point.x == 5.0f && point.y == 4.0f) found_max_x_max_y = true;
            if (point.x == 1.0f && point.y == 2.0f) found_min_x_min_y = true;
            if (point.x == 1.0f && point.y == 4.0f) found_min_x_max_y = true;
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
                {1.0f, 3.0f},
                {2.0f, 3.0f},
                {1.0f, 2.0f},
                {1.0f, 1.0f},
                {2.0f, 1.0f},
                {3.0f, 1.0f}};

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
            if (point.x == 3.0f && point.y == 1.0f) found_3_1 = true;
            if (point.x == 1.0f && point.y == 3.0f) found_1_3 = true;
            if (point.x == 2.0f && point.y == 3.0f) found_2_3 = true;
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
            if (pixel.x == 5.0f && pixel.y == 5.0f) {
                found_center = true;
                break;
            }
        }
        REQUIRE(found_center);

        // All pixels should be within distance 1 from center
        for (auto const & pixel: pixels) {
            float dx = pixel.x - 5.0f;
            float dy = pixel.y - 5.0f;
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
            float dx = pixel.x - 10.0f;
            float dy = pixel.y - 10.0f;
            float distance = std::sqrt(dx * dx + dy * dy);
            REQUIRE(distance <= 2.01f);// Small tolerance for floating point
        }

        // Should include pixels at approximately (8,10), (12,10), (10,8), (10,12)
        bool found_left = false, found_right = false, found_top = false, found_bottom = false;
        for (auto const & pixel: pixels) {
            if (std::abs(pixel.x - 8.0f) < 0.1f && std::abs(pixel.y - 10.0f) < 0.1f) found_left = true;
            if (std::abs(pixel.x - 12.0f) < 0.1f && std::abs(pixel.y - 10.0f) < 0.1f) found_right = true;
            if (std::abs(pixel.x - 10.0f) < 0.1f && std::abs(pixel.y - 8.0f) < 0.1f) found_top = true;
            if (std::abs(pixel.x - 10.0f) < 0.1f && std::abs(pixel.y - 12.0f) < 0.1f) found_bottom = true;
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
            float dx = pixel.x - 5.0f;
            float dy = pixel.y - 5.0f;
            float ellipse_value = (dx / 3.0f) * (dx / 3.0f) + (dy / 1.0f) * (dy / 1.0f);
            REQUIRE(ellipse_value <= 1.01f);// Small tolerance
        }

        // Should include pixels farther in X direction than Y direction
        bool found_far_x = false, found_far_y = false;
        for (auto const & pixel: pixels) {
            if (std::abs(pixel.x - 8.0f) < 0.1f && std::abs(pixel.y - 5.0f) < 0.1f) found_far_x = true;
            if (std::abs(pixel.x - 5.0f) < 0.1f && std::abs(pixel.y - 4.0f) < 0.1f) found_far_y = true;
        }
        REQUIRE(found_far_x);// Should reach to x=8 (3 units away)
        REQUIRE(found_far_y);// Should reach to y=4 (1 unit away)
    }

    SECTION("Ellipse at origin") {
        auto pixels = generate_ellipse_pixels(0.0f, 0.0f, 1.5f, 1.5f);

        // Should include origin
        bool found_origin = false;
        for (auto const & pixel: pixels) {
            if (pixel.x == 0.0f && pixel.y == 0.0f) {
                found_origin = true;
                break;
            }
        }
        REQUIRE(found_origin);

        // All pixels should have non-negative coordinates (bounds checking)
        for (auto const & pixel: pixels) {
            REQUIRE(pixel.x >= 0.0f);
            REQUIRE(pixel.y >= 0.0f);
        }
    }

    SECTION("Ellipse partially outside bounds") {
        // Center at (1, 1) with radius 2 - should clip negative coordinates
        auto pixels = generate_ellipse_pixels(1.0f, 1.0f, 2.0f, 2.0f);

        // All returned pixels should have non-negative coordinates
        for (auto const & pixel: pixels) {
            REQUIRE(pixel.x >= 0.0f);
            REQUIRE(pixel.y >= 0.0f);
        }

        // Should still include the center
        bool found_center = false;
        for (auto const & pixel: pixels) {
            if (pixel.x == 1.0f && pixel.y == 1.0f) {
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
            if (pixel.x == 10.0f && pixel.y == 10.0f) {
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
                if (pixel.x == 5.0f && pixel.y == 5.0f) {
                    found_center = true;
                    break;
                }
            }
            REQUIRE(found_center);
        }
    }
}