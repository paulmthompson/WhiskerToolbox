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

TEST_CASE("LineDisplayOptions - Default Values", "[LineDisplayOptions]") {
    LineDisplayOptions options;
    
    REQUIRE(options.line_thickness == DefaultDisplayValues::LINE_THICKNESS);
    REQUIRE(options.line_thickness == 2);
    REQUIRE(options.show_points == DefaultDisplayValues::SHOW_POINTS);
    REQUIRE(options.show_points == false);
    REQUIRE(options.edge_snapping == false);
    REQUIRE(options.hex_color == DefaultDisplayValues::COLOR);
    REQUIRE(options.alpha == DefaultDisplayValues::ALPHA);
    REQUIRE(options.is_visible == DefaultDisplayValues::VISIBLE);
}

TEST_CASE("LineDisplayOptions - Configurable Values", "[LineDisplayOptions]") {
    LineDisplayOptions options;
    
    SECTION("Line thickness configuration") {
        options.line_thickness = 1;
        REQUIRE(options.line_thickness == 1);
        
        options.line_thickness = 10;
        REQUIRE(options.line_thickness == 10);
        
        options.line_thickness = 20;
        REQUIRE(options.line_thickness == 20);
    }
    
    SECTION("Show points configuration") {
        options.show_points = true;
        REQUIRE(options.show_points == true);
        
        options.show_points = false;
        REQUIRE(options.show_points == false);
    }
    
    SECTION("Edge snapping configuration") {
        options.edge_snapping = true;
        REQUIRE(options.edge_snapping == true);
        
        options.edge_snapping = false;
        REQUIRE(options.edge_snapping == false);
    }
    
    SECTION("Position marker configuration") {
        options.show_position_marker = true;
        REQUIRE(options.show_position_marker == true);
        
        options.show_position_marker = false;
        REQUIRE(options.show_position_marker == false);
        
        options.position_percentage = 0;
        REQUIRE(options.position_percentage == 0);
        
        options.position_percentage = 50;
        REQUIRE(options.position_percentage == 50);
        
        options.position_percentage = 100;
        REQUIRE(options.position_percentage == 100);
    }
} 