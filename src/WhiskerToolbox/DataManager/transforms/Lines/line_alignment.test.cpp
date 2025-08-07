#include "transforms/Lines/line_alignment.hpp"
#include "CoreGeometry/points.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <vector>

TEST_CASE("FWHM displacement calculation - Core functionality", "[line][alignment][fwhm][transform]") {
    
    SECTION("Simple bright line detection") {
        // Create a simple test image with a bright horizontal line
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0); // All black initially
        
        // Create a bright horizontal line at y=50
        for (int x = 0; x < 100; ++x) {
            image_data[50 * 100 + x] = 255; // Bright white line
        }
        
        // Test vertex at (50, 50) with perpendicular direction pointing up
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, -1.0f}; // Pointing up
        
        Point2D<float> center_point = calculate_fwhm_center(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should find the bright line and return center point at (50, 50) (already on the line)
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(50.0f, 1.0f));
    }
    
    SECTION("Coordinate system verification") {
        // Create a test image to verify coordinate system
        ImageSize image_size{10, 10};
        std::vector<uint8_t> image_data(100, 0); // 10x10 = 100 pixels
        
        // Set pixel at (5, 3) to white
        image_data[3 * 10 + 5] = 255; // y=3, x=5
        
        // Test that get_pixel_value returns correct value
        Point2D<float> test_point{5.0f, 3.0f};
        uint8_t pixel_value = get_pixel_value(test_point, image_data, image_size);
        REQUIRE(pixel_value == 255);
        
        // Test that get_pixel_value returns 0 for other positions
        Point2D<float> test_point2{4.0f, 3.0f};
        uint8_t pixel_value2 = get_pixel_value(test_point2, image_data, image_size);
        REQUIRE(pixel_value2 == 0);
    }
    
    SECTION("Bright line detection with offset") {
        // Create a test image with a bright horizontal line
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0); // All black initially
        
        // Create a bright horizontal line at y=60
        for (int x = 0; x < 100; ++x) {
            image_data[60 * 100 + x] = 255; // Bright white line
        }
        
        // Test vertex at (50, 50) with perpendicular direction pointing up
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f}; // Pointing down
        
        Point2D<float> center_point = calculate_fwhm_center(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should find the bright line at y=60 and return center point at (50, 60)
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(60.0f, 1.0f));
    }
    
    SECTION("Bright line detection with offset and thickness") {
        // Create a test image with a bright horizontal line with thickness
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0); // All black initially
        
        // Create a bright horizontal line at y=60 with thickness of 3 pixels
        for (int x = 0; x < 100; ++x) {
            // Line spans from y=59 to y=61 (3 pixels thick)
            for (int y = 59; y <= 61; ++y) {
                if (y >= 0 && y < 100) {
                    image_data[y * 100 + x] = 255; // Bright white line
                }
            }
        }
        
        // Test vertex at (50, 50) with perpendicular direction pointing down
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f}; // Pointing down
        
        Point2D<float> center_point = calculate_fwhm_center(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should find the center of the thick bright line at y=60 and return center point at (50, 60)
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(60.0f, 1.0f));
    }
    
    SECTION("Bright line detection with varying thickness") {
        // Create a test image with a bright horizontal line with varying thickness
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0); // All black initially
        
        // Create a bright horizontal line at y=60 with thickness of 5 pixels
        for (int x = 0; x < 100; ++x) {
            // Line spans from y=58 to y=62 (5 pixels thick)
            for (int y = 58; y <= 62; ++y) {
                if (y >= 0 && y < 100) {
                    image_data[y * 100 + x] = 255; // Bright white line
                }
            }
        }
        
        // Test vertex at (50, 50) with perpendicular direction pointing down
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f}; // Pointing down
        
        Point2D<float> center_point = calculate_fwhm_center(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should find the center of the thick bright line at y=60 and return center point at (50, 60)
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(60.0f, 1.0f));
    }
    
    SECTION("Bright line detection with very thick line") {
        // Create a test image with a very thick bright horizontal line
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0); // All black initially
        
        // Create a bright horizontal line at y=60 with thickness of 9 pixels
        for (int x = 0; x < 100; ++x) {
            // Line spans from y=56 to y=64 (9 pixels thick)
            for (int y = 56; y <= 64; ++y) {
                if (y >= 0 && y < 100) {
                    image_data[y * 100 + x] = 255; // Bright white line
                }
            }
        }
        
        // Test vertex at (50, 50) with perpendicular direction pointing down
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f}; // Pointing down
        
        Point2D<float> center_point = calculate_fwhm_center(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should find the center of the thick bright line at y=60 and return center point at (50, 60)
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(60.0f, 1.0f));
    }
    
    SECTION("Bright line detection with diagonal perpendicular") {
        // Create a test image with a bright diagonal line
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0); // All black initially
        
        // Create a bright diagonal line
        for (int i = 0; i < 100; ++i) {
            int x = i;
            int y = i + 10; // Offset by 10 pixels
            if (x < 100 && y < 100) {
                image_data[y * 100 + x] = 255; // Bright white line
            }
        }
        
        // Test vertex at (50, 50) with perpendicular direction
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{-0.707f, 0.707f}; // Diagonal perpendicular
        
        Point2D<float> center_point = calculate_fwhm_center(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should find the bright line and return appropriate center point
        // The diagonal line is at y = x + 10, so at x=50, y=60
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 2.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(60.0f, 2.0f));
    }
    
    SECTION("Bright diagonal line detection with thickness") {
        // Create a test image with a bright diagonal line with thickness
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0); // All black initially
        
        // Create a bright diagonal line from (10,10) to (90,90) with thickness
        for (int i = 10; i <= 90; ++i) {
            // Line spans from (i,i) with thickness of 3 pixels perpendicular to the line
            for (int offset = -1; offset <= 1; ++offset) {
                int x = i + offset;
                int y = i + offset;
                if (x >= 0 && x < 100 && y >= 0 && y < 100) {
                    image_data[y * 100 + x] = 255; // Bright white line
                }
            }
        }
        
        // Test vertex at (50, 50) with perpendicular direction
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{-0.707f, 0.707f}; // Perpendicular to diagonal
        
        Point2D<float> center_point = calculate_fwhm_center(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should find the diagonal line and return appropriate center point
        // The exact value depends on the FWHM calculation, but should be reasonable
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 5.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(50.0f, 5.0f));
    }
    
    SECTION("No bright features - should return zero") {
        // Create a completely black image
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0); // All black
        
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f}; // Pointing down
        
        Point2D<float> center_point = calculate_fwhm_center(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should return original vertex when no bright features are found
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 0.001f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(50.0f, 0.001f));
    }
    
    SECTION("Bright spot detection") {
        // Create a test image with a bright spot
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0); // All black initially
        
        // Create a bright spot at (60, 50)
        for (int y = 45; y <= 55; ++y) {
            for (int x = 55; x <= 65; ++x) {
                if (x >= 0 && x < 100 && y >= 0 && y < 100) {
                    image_data[y * 100 + x] = 255; // Bright white spot
                }
            }
        }
        
        // Test vertex at (50, 50) with perpendicular direction pointing right
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{1.0f, 0.0f}; // Pointing right
        
        Point2D<float> center_point = calculate_fwhm_center(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should find the bright spot and return center point at (60, 50)
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(60.0f, 1.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(50.0f, 1.0f));
    }
    
    SECTION("Multiple bright lines - should find the closest") {
        // Create a test image with multiple bright horizontal lines
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0); // All black initially
        
        // Create bright horizontal lines at y=30 and y=70
        for (int x = 0; x < 100; ++x) {
            image_data[30 * 100 + x] = 255; // Bright white line
            image_data[70 * 100 + x] = 255; // Bright white line
        }
        
        // Test vertex at (50, 50) with perpendicular direction pointing up
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, -1.0f}; // Pointing up
        
        Point2D<float> center_point = calculate_fwhm_center(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should find the closer line at y=30 and return center point at (50, 30)
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(30.0f, 1.0f));
    }
    
    SECTION("Edge case - vertex at image boundary") {
        // Create a test image with a bright line
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0); // All black initially
        
        // Create a bright horizontal line at y=50
        for (int x = 0; x < 100; ++x) {
            image_data[50 * 100 + x] = 255; // Bright white line
        }
        
        // Test vertex at (0, 50) with perpendicular direction pointing right
        Point2D<float> vertex{0.0f, 50.0f};
        Point2D<float> perp_dir{1.0f, 0.0f}; // Pointing right
        
        Point2D<float> center_point = calculate_fwhm_center(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should handle boundary case gracefully
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(0.0f, 1.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(50.0f, 1.0f));
    }
    
    SECTION("Different perpendicular range values") {
        // Create a test image with a bright horizontal line
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0); // All black initially
        
        // Create a bright horizontal line at y=60
        for (int x = 0; x < 100; ++x) {
            image_data[60 * 100 + x] = 255; // Bright white line
        }
        
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f}; // Pointing down
        
        // Test with different perpendicular ranges
        Point2D<float> center_point1 = calculate_fwhm_center(
            vertex, perp_dir, 10, 20, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        Point2D<float> center_point2 = calculate_fwhm_center(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Both should find the same line and return similar center points
        REQUIRE_THAT(center_point1.x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(center_point1.y, Catch::Matchers::WithinAbs(60.0f, 1.0f));
        REQUIRE_THAT(center_point2.x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(center_point2.y, Catch::Matchers::WithinAbs(60.0f, 1.0f));
    }
    
    SECTION("Different width values") {
        // Create a test image with a bright horizontal line
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0); // All black initially
        
        // Create a bright horizontal line at y=60
        for (int x = 0; x < 100; ++x) {
            image_data[60 * 100 + x] = 255; // Bright white line
        }
        
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f}; // Pointing down
        
        // Test with different width values
        Point2D<float> center_point1 = calculate_fwhm_center(
            vertex, perp_dir, 5, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        Point2D<float> center_point2 = calculate_fwhm_center(
            vertex, perp_dir, 20, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Both should find the same line and return similar center points
        REQUIRE_THAT(center_point1.x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(center_point1.y, Catch::Matchers::WithinAbs(60.0f, 1.0f));
        REQUIRE_THAT(center_point2.x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(center_point2.y, Catch::Matchers::WithinAbs(60.0f, 1.0f));
    }
}

TEST_CASE("Perpendicular direction calculation - Core functionality", "[line][alignment][perpendicular][transform]") {
    
    SECTION("Horizontal line - perpendicular should be vertical") {
        // Create a horizontal line from (0,0) to (10,0)
        Line2D horizontal_line;
        horizontal_line.push_back(Point2D<float>{0.0f, 0.0f});
        horizontal_line.push_back(Point2D<float>{10.0f, 0.0f});
        
        // Test first vertex (should use direction to next vertex)
        Point2D<float> perp_dir_first = calculate_perpendicular_direction(horizontal_line, 0);
        REQUIRE_THAT(perp_dir_first.x, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(perp_dir_first.y, Catch::Matchers::WithinAbs(1.0f, 0.001f)); // Pointing down
        
        // Test last vertex (should use direction from previous vertex)
        Point2D<float> perp_dir_last = calculate_perpendicular_direction(horizontal_line, 1);
        REQUIRE_THAT(perp_dir_last.x, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(perp_dir_last.y, Catch::Matchers::WithinAbs(1.0f, 0.001f)); // Pointing down
    }
    
    SECTION("Vertical line - perpendicular should be horizontal") {
        // Create a vertical line from (0,0) to (0,10)
        Line2D vertical_line;
        vertical_line.push_back(Point2D<float>{0.0f, 0.0f});
        vertical_line.push_back(Point2D<float>{0.0f, 10.0f});
        
        // Test first vertex
        Point2D<float> perp_dir_first = calculate_perpendicular_direction(vertical_line, 0);
        REQUIRE_THAT(perp_dir_first.x, Catch::Matchers::WithinAbs(-1.0f, 0.001f)); // Pointing left
        REQUIRE_THAT(perp_dir_first.y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        
        // Test last vertex
        Point2D<float> perp_dir_last = calculate_perpendicular_direction(vertical_line, 1);
        REQUIRE_THAT(perp_dir_last.x, Catch::Matchers::WithinAbs(-1.0f, 0.001f)); // Pointing left
        REQUIRE_THAT(perp_dir_last.y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Diagonal line - perpendicular should be perpendicular") {
        // Create a diagonal line from (0,0) to (10,10)
        Line2D diagonal_line;
        diagonal_line.push_back(Point2D<float>{0.0f, 0.0f});
        diagonal_line.push_back(Point2D<float>{10.0f, 10.0f});
        
        // Test first vertex
        Point2D<float> perp_dir_first = calculate_perpendicular_direction(diagonal_line, 0);
        // Perpendicular to (10,10) - (0,0) = (10,10) should be (-10,10) normalized
        REQUIRE_THAT(perp_dir_first.x, Catch::Matchers::WithinAbs(-0.707f, 0.001f)); // -1/sqrt(2)
        REQUIRE_THAT(perp_dir_first.y, Catch::Matchers::WithinAbs(0.707f, 0.001f));  // 1/sqrt(2)
        
        // Test last vertex
        Point2D<float> perp_dir_last = calculate_perpendicular_direction(diagonal_line, 1);
        REQUIRE_THAT(perp_dir_last.x, Catch::Matchers::WithinAbs(-0.707f, 0.001f));
        REQUIRE_THAT(perp_dir_last.y, Catch::Matchers::WithinAbs(0.707f, 0.001f));
    }
    
    SECTION("Multi-segment line - middle vertices average perpendiculars") {
        // Create a line with three segments: horizontal, diagonal, vertical
        Line2D multi_line;
        multi_line.push_back(Point2D<float>{0.0f, 0.0f});   // Start
        multi_line.push_back(Point2D<float>{10.0f, 0.0f});  // Horizontal segment
        multi_line.push_back(Point2D<float>{20.0f, 10.0f}); // Diagonal segment
        multi_line.push_back(Point2D<float>{20.0f, 20.0f}); // Vertical segment
        
        // Test middle vertex (index 1) - should average perpendiculars from adjacent segments
        Point2D<float> perp_dir_middle = calculate_perpendicular_direction(multi_line, 1);
        
        // First segment: (10,0) - (0,0) = (10,0), perpendicular = (0,1)
        // Second segment: (20,10) - (10,0) = (10,10), perpendicular = (-10,10)/sqrt(200) = (-0.707, 0.707)
        // Average: (0 + (-0.707))/2 = -0.3535, (1 + 0.707)/2 = 0.8535
        // Normalized: approximately (-0.383, 0.924)
        REQUIRE_THAT(perp_dir_middle.x, Catch::Matchers::WithinAbs(-0.383f, 0.1f));
        REQUIRE_THAT(perp_dir_middle.y, Catch::Matchers::WithinAbs(0.924f, 0.1f));
    }
    
    SECTION("Line with fewer than 2 points - should return zero vector") {
        // Create a line with only 1 point
        Line2D short_line;
        short_line.push_back(Point2D<float>{0.0f, 0.0f});
        
        // Test with 1 point (invalid for perpendicular calculation)
        Point2D<float> perp_dir = calculate_perpendicular_direction(short_line, 0);
        REQUIRE_THAT(perp_dir.x, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(perp_dir.y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Line with zero-length segments - should return zero vector") {
        // Create a line with zero-length segments
        Line2D zero_line;
        zero_line.push_back(Point2D<float>{5.0f, 5.0f});
        zero_line.push_back(Point2D<float>{5.0f, 5.0f}); // Same point
        zero_line.push_back(Point2D<float>{5.0f, 5.0f}); // Same point
        
        // Test middle vertex
        Point2D<float> perp_dir = calculate_perpendicular_direction(zero_line, 1);
        REQUIRE_THAT(perp_dir.x, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(perp_dir.y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Normalized perpendicular vectors") {
        // Test that all perpendicular vectors are normalized (unit length)
        Line2D test_line;
        test_line.push_back(Point2D<float>{0.0f, 0.0f});
        test_line.push_back(Point2D<float>{3.0f, 4.0f}); // 3-4-5 triangle
        test_line.push_back(Point2D<float>{6.0f, 8.0f});
        
        // Test first vertex
        Point2D<float> perp_dir = calculate_perpendicular_direction(test_line, 0);
        float length = std::sqrt(perp_dir.x * perp_dir.x + perp_dir.y * perp_dir.y);
        REQUIRE_THAT(length, Catch::Matchers::WithinAbs(1.0f, 0.001f));
        
        // Test middle vertex
        Point2D<float> perp_dir_middle = calculate_perpendicular_direction(test_line, 1);
        float length_middle = std::sqrt(perp_dir_middle.x * perp_dir_middle.x + perp_dir_middle.y * perp_dir_middle.y);
        REQUIRE_THAT(length_middle, Catch::Matchers::WithinAbs(1.0f, 0.001f));
        
        // Test last vertex
        Point2D<float> perp_dir_last = calculate_perpendicular_direction(test_line, 2);
        float length_last = std::sqrt(perp_dir_last.x * perp_dir_last.x + perp_dir_last.y * perp_dir_last.y);
        REQUIRE_THAT(length_last, Catch::Matchers::WithinAbs(1.0f, 0.001f));
    }
}

TEST_CASE("FWHM center calculation - Edge cases and error handling", "[line][alignment][fwhm][edge]") {
    
    SECTION("Zero width parameter") {
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0);
        
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f};
        
        Point2D<float> center_point = calculate_fwhm_center(
            vertex, perp_dir, 0, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should return original vertex for invalid width
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 0.001f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(50.0f, 0.001f));
    }
    
    SECTION("Zero perpendicular direction") {
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0);
        
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 0.0f}; // Zero direction
        
        Point2D<float> center_point = calculate_fwhm_center(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should handle zero direction gracefully
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 0.001f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(50.0f, 0.001f));
    }
    
    SECTION("Empty image data") {
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data; // Empty
        
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f};
        
        Point2D<float> center_point = calculate_fwhm_center(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should handle empty data gracefully
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 0.001f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(50.0f, 0.001f));
    }
    
    SECTION("Vertex outside image bounds") {
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0);
        
        Point2D<float> vertex{150.0f, 150.0f}; // Outside bounds
        Point2D<float> perp_dir{0.0f, 1.0f};
        
        Point2D<float> center_point = calculate_fwhm_center(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should handle out-of-bounds vertex gracefully
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(150.0f, 0.001f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(150.0f, 0.001f));
    }
}

#if LINE_ALIGNMENT_DEBUG_MODE
TEST_CASE("FWHM profile extents calculation - Debug mode", "[line][alignment][fwhm][debug]") {
    
    SECTION("Debug mode profile extents for bright line") {
        // Create a test image with a bright horizontal line
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0); // All black initially
        
        // Create a bright horizontal line at y=60
        for (int x = 0; x < 100; ++x) {
            image_data[60 * 100 + x] = 255; // Bright white line
        }
        
        // Test vertex at (50, 50) with perpendicular direction pointing down
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f}; // Pointing down
        
        Line2D debug_line = calculate_fwhm_profile_extents(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should return a line with 3 points: [left_extent, max_point, right_extent]
        REQUIRE(debug_line.size() == 3);
        
        // First point should be left extent (around y=55-65)
        REQUIRE_THAT(debug_line[0].x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(debug_line[0].y, Catch::Matchers::WithinAbs(60.0f, 5.0f));
        
        // Second point should be max point (around y=60)
        REQUIRE_THAT(debug_line[1].x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(debug_line[1].y, Catch::Matchers::WithinAbs(60.0f, 1.0f));
        
        // Third point should be right extent (around y=55-65)
        REQUIRE_THAT(debug_line[2].x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(debug_line[2].y, Catch::Matchers::WithinAbs(60.0f, 5.0f));
    }
    
    SECTION("Debug mode with multiple maximum values") {
        // Create a test image with a bright horizontal line that has multiple maximum values
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0); // All black initially
        
        // Create a bright horizontal line at y=60 with some thickness
        for (int x = 0; x < 100; ++x) {
            // Create a line with multiple maximum values at different positions
            for (int y = 58; y <= 62; ++y) {
                if (y >= 0 && y < 100) {
                    image_data[y * 100 + x] = 255; // Bright white line
                }
            }
        }
        
        // Test vertex at (50, 50) with perpendicular direction pointing down
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f}; // Pointing down
        
        Line2D debug_line = calculate_fwhm_profile_extents(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should return a line with 3 points: [left_extent, max_point, right_extent]
        REQUIRE(debug_line.size() == 3);
        
        // First point should be left extent
        REQUIRE_THAT(debug_line[0].x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(debug_line[0].y, Catch::Matchers::WithinAbs(60.0f, 5.0f));
        
        // Second point should be max point (average of multiple max positions)
        REQUIRE_THAT(debug_line[1].x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(debug_line[1].y, Catch::Matchers::WithinAbs(60.0f, 2.0f)); // Should be around center of thick line
        
        // Third point should be right extent
        REQUIRE_THAT(debug_line[2].x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(debug_line[2].y, Catch::Matchers::WithinAbs(60.0f, 5.0f));
    }
}
#endif 