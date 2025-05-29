#include <catch2/catch_test_macros.hpp>
#include "lines.hpp"

/**
 * @brief Test file for lines utility functions
 */

TEST_CASE("get_position_at_percentage - Basic functionality", "[get_position_at_percentage]") {
    SECTION("Empty line") {
        Line2D empty_line;
        auto result = get_position_at_percentage(empty_line, 0.5f);
        REQUIRE(result.x == 0.0f);
        REQUIRE(result.y == 0.0f);
    }
    
    SECTION("Single point line") {
        Line2D single_point = {Point2D<float>{5.0f, 10.0f}};
        auto result = get_position_at_percentage(single_point, 0.5f);
        REQUIRE(result.x == 5.0f);
        REQUIRE(result.y == 10.0f);
    }
    
    SECTION("Two point line") {
        Line2D two_points = {
            Point2D<float>{0.0f, 0.0f},
            Point2D<float>{10.0f, 0.0f}
        };
        
        // Test at 0% (start)
        auto result_start = get_position_at_percentage(two_points, 0.0f);
        REQUIRE(result_start.x == 0.0f);
        REQUIRE(result_start.y == 0.0f);
        
        // Test at 50% (middle)
        auto result_middle = get_position_at_percentage(two_points, 0.5f);
        REQUIRE(result_middle.x == 5.0f);
        REQUIRE(result_middle.y == 0.0f);
        
        // Test at 100% (end)
        auto result_end = get_position_at_percentage(two_points, 1.0f);
        REQUIRE(result_end.x == 10.0f);
        REQUIRE(result_end.y == 0.0f);
    }
}

TEST_CASE("get_position_at_percentage - Complex line", "[get_position_at_percentage]") {
    // Create a right triangle: (0,0) -> (3,0) -> (3,4)
    // Total length = 3 + 4 = 7
    Line2D triangle = {
        Point2D<float>{0.0f, 0.0f},  // Start
        Point2D<float>{3.0f, 0.0f},  // Corner
        Point2D<float>{3.0f, 4.0f}   // End
    };
    
    SECTION("At 0% - should be at start") {
        auto result = get_position_at_percentage(triangle, 0.0f);
        REQUIRE(result.x == 0.0f);
        REQUIRE(result.y == 0.0f);
    }
    
    SECTION("At ~43% - should be at corner (3/7)") {
        auto result = get_position_at_percentage(triangle, 3.0f/7.0f);
        REQUIRE(result.x == 3.0f);
        REQUIRE(result.y == 0.0f);
    }
    
    SECTION("At 100% - should be at end") {
        auto result = get_position_at_percentage(triangle, 1.0f);
        REQUIRE(result.x == 3.0f);
        REQUIRE(result.y == 4.0f);
    }
}

TEST_CASE("get_position_at_percentage - Edge cases", "[get_position_at_percentage]") {
    Line2D line = {
        Point2D<float>{0.0f, 0.0f},
        Point2D<float>{10.0f, 10.0f}
    };
    
    SECTION("Percentage below 0 should clamp to 0") {
        auto result = get_position_at_percentage(line, -0.5f);
        REQUIRE(result.x == 0.0f);
        REQUIRE(result.y == 0.0f);
    }
    
    SECTION("Percentage above 1 should clamp to 1") {
        auto result = get_position_at_percentage(line, 1.5f);
        REQUIRE(result.x == 10.0f);
        REQUIRE(result.y == 10.0f);
    }
} 