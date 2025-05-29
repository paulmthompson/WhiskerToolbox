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