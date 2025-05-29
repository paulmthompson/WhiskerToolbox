#include <catch2/catch_test_macros.hpp>
#include "DisplayOptions.hpp"

/**
 * @brief Test file for point display options functionality
 */

TEST_CASE("PointDisplayOptions - Default Values", "[PointDisplayOptions]") {
    PointDisplayOptions options;
    
    REQUIRE(options.point_size == DefaultDisplayValues::POINT_SIZE);
    REQUIRE(options.point_size == 5);
    REQUIRE(options.marker_shape == DefaultDisplayValues::POINT_MARKER_SHAPE);
    REQUIRE(options.marker_shape == PointMarkerShape::Circle);
    REQUIRE(options.hex_color == DefaultDisplayValues::COLOR);
    REQUIRE(options.alpha == DefaultDisplayValues::ALPHA);
    REQUIRE(options.is_visible == DefaultDisplayValues::VISIBLE);
}

TEST_CASE("PointDisplayOptions - Configurable Values", "[PointDisplayOptions]") {
    PointDisplayOptions options;
    
    SECTION("Point size configuration") {
        options.point_size = 10;
        REQUIRE(options.point_size == 10);
        
        options.point_size = 1;
        REQUIRE(options.point_size == 1);
        
        options.point_size = 50;
        REQUIRE(options.point_size == 50);
    }
    
    SECTION("Marker shape configuration") {
        options.marker_shape = PointMarkerShape::Square;
        REQUIRE(options.marker_shape == PointMarkerShape::Square);
        
        options.marker_shape = PointMarkerShape::Triangle;
        REQUIRE(options.marker_shape == PointMarkerShape::Triangle);
        
        options.marker_shape = PointMarkerShape::Cross;
        REQUIRE(options.marker_shape == PointMarkerShape::Cross);
        
        options.marker_shape = PointMarkerShape::X;
        REQUIRE(options.marker_shape == PointMarkerShape::X);
        
        options.marker_shape = PointMarkerShape::Diamond;
        REQUIRE(options.marker_shape == PointMarkerShape::Diamond);
    }
}

TEST_CASE("PointMarkerShape Enum Values", "[PointMarkerShape]") {
    // Test that all enum values can be cast to integers correctly
    REQUIRE(static_cast<int>(PointMarkerShape::Circle) == 0);
    REQUIRE(static_cast<int>(PointMarkerShape::Square) == 1);
    REQUIRE(static_cast<int>(PointMarkerShape::Triangle) == 2);
    REQUIRE(static_cast<int>(PointMarkerShape::Cross) == 3);
    REQUIRE(static_cast<int>(PointMarkerShape::X) == 4);
    REQUIRE(static_cast<int>(PointMarkerShape::Diamond) == 5);
} 