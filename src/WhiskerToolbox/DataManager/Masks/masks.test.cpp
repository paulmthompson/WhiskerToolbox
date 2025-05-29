#include <catch2/catch_test_macros.hpp>
#include "masks.hpp"

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
            {1.0f, 4.0f}
        };
        
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
            {-2.0f, 4.0f}
        };
        
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
        for (auto const& point : outline) {
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
        bool found_max_x_min_y = false; // (5, 2)
        bool found_max_x_max_y = false; // (5, 4)
        bool found_min_x_min_y = false; // (1, 2)
        bool found_min_x_max_y = false; // (1, 4)
        
        for (auto const& point : outline) {
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
            {1.0f, 3.0f}, {2.0f, 3.0f},
            {1.0f, 2.0f},
            {1.0f, 1.0f}, {2.0f, 1.0f}, {3.0f, 1.0f}
        };
        
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
        
        for (auto const& point : outline) {
            if (point.x == 3.0f && point.y == 1.0f) found_3_1 = true;
            if (point.x == 1.0f && point.y == 3.0f) found_1_3 = true;
            if (point.x == 2.0f && point.y == 3.0f) found_2_3 = true;
        }
        
        REQUIRE(found_3_1);
        REQUIRE(found_1_3);
        REQUIRE(found_2_3);
    }
} 