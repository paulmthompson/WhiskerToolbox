#include "CoreGeometry/Image.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "Lines/Line_Data.hpp"
#include "Media/Media_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "../mocks/mock_media_data.hpp"
#include "line_alignment.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <type_traits>
#include <vector>

TEST_CASE("Data Transform: Line Alignment - FWHM displacement calculation - Core functionality", "[line][alignment][fwhm][transform]") {

    SECTION("Simple bright line detection") {
        // Create a simple test image with a bright horizontal line
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0);// All black initially

        // Create a bright horizontal line at y=50
        for (int x = 0; x < 100; ++x) {
            image_data[50 * 100 + x] = 255;// Bright white line
        }

        // Test vertex at (50, 50) with perpendicular direction pointing up
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, -1.0f};// Pointing up

        Point2D<float> center_point = calculate_fwhm_center(
                vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);

        // Should find the bright line and return center point at (50, 50) (already on the line)
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(50.0f, 1.0f));
    }

    SECTION("Coordinate system verification") {
        // Create a test image to verify coordinate system
        ImageSize image_size{10, 10};
        std::vector<uint8_t> image_data(100, 0);// 10x10 = 100 pixels

        // Set pixel at (5, 3) to white
        image_data[3 * 10 + 5] = 255;// y=3, x=5

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
        std::vector<uint8_t> image_data(100 * 100, 0);// All black initially

        // Create a bright horizontal line at y=60
        for (int x = 0; x < 100; ++x) {
            image_data[60 * 100 + x] = 255;// Bright white line
        }

        // Test vertex at (50, 50) with perpendicular direction pointing up
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f};// Pointing down

        Point2D<float> center_point = calculate_fwhm_center(
                vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);

        // Should find the bright line at y=60 and return center point at (50, 60)
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(60.0f, 1.0f));
    }

    SECTION("Bright line detection with offset and thickness") {
        // Create a test image with a bright horizontal line with thickness
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0);// All black initially

        // Create a bright horizontal line at y=60 with thickness of 3 pixels
        for (int x = 0; x < 100; ++x) {
            // Line spans from y=59 to y=61 (3 pixels thick)
            for (int y = 59; y <= 61; ++y) {
                if (y >= 0 && y < 100) {
                    image_data[y * 100 + x] = 255;// Bright white line
                }
            }
        }

        // Test vertex at (50, 50) with perpendicular direction pointing down
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f};// Pointing down

        Point2D<float> center_point = calculate_fwhm_center(
                vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);

        // Should find the center of the thick bright line at y=60 and return center point at (50, 60)
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(60.0f, 1.0f));
    }

    SECTION("Bright line detection with varying thickness") {
        // Create a test image with a bright horizontal line with varying thickness
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0);// All black initially

        // Create a bright horizontal line at y=60 with thickness of 5 pixels
        for (int x = 0; x < 100; ++x) {
            // Line spans from y=58 to y=62 (5 pixels thick)
            for (int y = 58; y <= 62; ++y) {
                if (y >= 0 && y < 100) {
                    image_data[y * 100 + x] = 255;// Bright white line
                }
            }
        }

        // Test vertex at (50, 50) with perpendicular direction pointing down
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f};// Pointing down

        Point2D<float> center_point = calculate_fwhm_center(
                vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);

        // Should find the center of the thick bright line at y=60 and return center point at (50, 60)
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(60.0f, 1.0f));
    }

    SECTION("Bright line detection with very thick line") {
        // Create a test image with a very thick bright horizontal line
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0);// All black initially

        // Create a bright horizontal line at y=60 with thickness of 9 pixels
        for (int x = 0; x < 100; ++x) {
            // Line spans from y=56 to y=64 (9 pixels thick)
            for (int y = 56; y <= 64; ++y) {
                if (y >= 0 && y < 100) {
                    image_data[y * 100 + x] = 255;// Bright white line
                }
            }
        }

        // Test vertex at (50, 50) with perpendicular direction pointing down
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f};// Pointing down

        Point2D<float> center_point = calculate_fwhm_center(
                vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);

        // Should find the center of the thick bright line at y=60 and return center point at (50, 60)
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(60.0f, 1.0f));
    }

    SECTION("Bright line detection with diagonal perpendicular") {
        // Create a test image with a bright diagonal line
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0);// All black initially

        // Create a bright diagonal line
        for (int i = 0; i < 100; ++i) {
            int x = i;
            int y = i + 10;// Offset by 10 pixels
            if (x < 100 && y < 100) {
                image_data[y * 100 + x] = 255;// Bright white line
            }
        }

        // Test vertex at (50, 50) with perpendicular direction
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{-0.707f, 0.707f};// Diagonal perpendicular

        Point2D<float> center_point = calculate_fwhm_center(
                vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);

        // Should find the bright line and return appropriate center point
        // This will be the nearest position of the diagonal line stretching from
        // (40, 50) to (50, 60) . This is (45, 55)
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(45.0f, 2.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(55.0f, 2.0f));
    }

    SECTION("Bright diagonal line detection with thickness") {
        // Create a test image with a bright diagonal line with thickness
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0);// All black initially

        // Create a bright diagonal line from (10,10) to (90,90) with thickness
        for (int i = 10; i <= 90; ++i) {
            // Line spans from (i,i) with thickness of 3 pixels perpendicular to the line
            for (int offset = -1; offset <= 1; ++offset) {
                int x = i + offset;
                int y = i + offset;
                if (x >= 0 && x < 100 && y >= 0 && y < 100) {
                    image_data[y * 100 + x] = 255;// Bright white line
                }
            }
        }

        // Test vertex at (50, 50) with perpendicular direction
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{-0.707f, 0.707f};// Perpendicular to diagonal

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
        std::vector<uint8_t> image_data(100 * 100, 0);// All black

        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f};// Pointing down

        Point2D<float> center_point = calculate_fwhm_center(
                vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);

        // Should return original vertex when no bright features are found
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 0.001f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(50.0f, 0.001f));
    }

    SECTION("Bright spot detection") {
        // Create a test image with a bright spot
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0);// All black initially

        // Create a bright spot at (60, 50)
        for (int y = 45; y <= 55; ++y) {
            for (int x = 55; x <= 65; ++x) {
                if (x >= 0 && x < 100 && y >= 0 && y < 100) {
                    image_data[y * 100 + x] = 255;// Bright white spot
                }
            }
        }

        // Test vertex at (50, 50) with perpendicular direction pointing right
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{1.0f, 0.0f};// Pointing right

        Point2D<float> center_point = calculate_fwhm_center(
                vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);

        // Should find the bright spot and return center point at (60, 50)
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(60.0f, 1.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(50.0f, 1.0f));
    }

    SECTION("Multiple bright lines - should find the closest") {
        // Create a test image with multiple bright horizontal lines
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0);// All black initially

        // Create bright horizontal lines at y=30 and y=70
        for (int x = 0; x < 100; ++x) {
            image_data[30 * 100 + x] = 255;// Bright white line
            image_data[80 * 100 + x] = 255;// Bright white line
        }

        // Test vertex at (50, 50) with perpendicular direction pointing up
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, -1.0f};// Pointing up

        Point2D<float> center_point = calculate_fwhm_center(
                vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);

        // Should find the closer line at y=30 and return center point at (50, 30)
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(30.0f, 1.0f));
    }

    SECTION("Edge case - vertex at image boundary") {
        // Create a test image with a bright line
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0);// All black initially

        // Create a bright horizontal line at y=50
        for (int x = 0; x < 100; ++x) {
            image_data[50 * 100 + x] = 255;// Bright white line
        }

        // Test vertex at (50, 0) with perpendicular direction pointing right
        Point2D<float> vertex{50.0f, 0.0f};
        Point2D<float> perp_dir{0.0f, 1.0f};// Pointing down

        Point2D<float> center_point = calculate_fwhm_center(
                vertex, perp_dir, 10, 102, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);

        // Should handle boundary case gracefully
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(50.0f, 1.0f));
    }

    SECTION("Different perpendicular range values") {
        // Create a test image with a bright horizontal line
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0);// All black initially

        // Create a bright horizontal line at y=60
        for (int x = 0; x < 100; ++x) {
            image_data[60 * 100 + x] = 255;// Bright white line
        }

        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f};// Pointing down

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
        std::vector<uint8_t> image_data(100 * 100, 0);// All black initially

        // Create a bright horizontal line at y=60
        for (int x = 0; x < 100; ++x) {
            image_data[60 * 100 + x] = 255;// Bright white line
        }

        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f};// Pointing down

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

TEST_CASE("Data Transform: Line Alignment - Perpendicular direction calculation - Core functionality", "[line][alignment][perpendicular][transform]") {

    SECTION("Horizontal line - perpendicular should be vertical") {
        // Create a horizontal line from (0,0) to (10,0)
        Line2D horizontal_line;
        horizontal_line.push_back(Point2D<float>{0.0f, 0.0f});
        horizontal_line.push_back(Point2D<float>{10.0f, 0.0f});

        // Test first vertex (should use direction to next vertex)
        Point2D<float> perp_dir_first = calculate_perpendicular_direction(horizontal_line, 0);
        REQUIRE_THAT(perp_dir_first.x, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(perp_dir_first.y, Catch::Matchers::WithinAbs(1.0f, 0.001f));// Pointing down

        // Test last vertex (should use direction from previous vertex)
        Point2D<float> perp_dir_last = calculate_perpendicular_direction(horizontal_line, 1);
        REQUIRE_THAT(perp_dir_last.x, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(perp_dir_last.y, Catch::Matchers::WithinAbs(1.0f, 0.001f));// Pointing down
    }

    SECTION("Vertical line - perpendicular should be horizontal") {
        // Create a vertical line from (0,0) to (0,10)
        Line2D vertical_line;
        vertical_line.push_back(Point2D<float>{0.0f, 0.0f});
        vertical_line.push_back(Point2D<float>{0.0f, 10.0f});

        // Test first vertex
        Point2D<float> perp_dir_first = calculate_perpendicular_direction(vertical_line, 0);
        REQUIRE_THAT(perp_dir_first.x, Catch::Matchers::WithinAbs(-1.0f, 0.001f));// Pointing left
        REQUIRE_THAT(perp_dir_first.y, Catch::Matchers::WithinAbs(0.0f, 0.001f));

        // Test last vertex
        Point2D<float> perp_dir_last = calculate_perpendicular_direction(vertical_line, 1);
        REQUIRE_THAT(perp_dir_last.x, Catch::Matchers::WithinAbs(-1.0f, 0.001f));// Pointing left
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
        REQUIRE_THAT(perp_dir_first.x, Catch::Matchers::WithinAbs(-0.707f, 0.001f));// -1/sqrt(2)
        REQUIRE_THAT(perp_dir_first.y, Catch::Matchers::WithinAbs(0.707f, 0.001f)); // 1/sqrt(2)

        // Test last vertex
        Point2D<float> perp_dir_last = calculate_perpendicular_direction(diagonal_line, 1);
        REQUIRE_THAT(perp_dir_last.x, Catch::Matchers::WithinAbs(-0.707f, 0.001f));
        REQUIRE_THAT(perp_dir_last.y, Catch::Matchers::WithinAbs(0.707f, 0.001f));
    }

    SECTION("Multi-segment line - middle vertices average perpendiculars") {
        // Create a line with three segments: horizontal, diagonal, vertical
        Line2D multi_line;
        multi_line.push_back(Point2D<float>{0.0f, 0.0f});  // Start
        multi_line.push_back(Point2D<float>{10.0f, 0.0f}); // Horizontal segment
        multi_line.push_back(Point2D<float>{20.0f, 10.0f});// Diagonal segment
        multi_line.push_back(Point2D<float>{20.0f, 20.0f});// Vertical segment

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
        zero_line.push_back(Point2D<float>{5.0f, 5.0f});// Same point
        zero_line.push_back(Point2D<float>{5.0f, 5.0f});// Same point

        // Test middle vertex
        Point2D<float> perp_dir = calculate_perpendicular_direction(zero_line, 1);
        REQUIRE_THAT(perp_dir.x, Catch::Matchers::WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(perp_dir.y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }

    SECTION("Normalized perpendicular vectors") {
        // Test that all perpendicular vectors are normalized (unit length)
        Line2D test_line;
        test_line.push_back(Point2D<float>{0.0f, 0.0f});
        test_line.push_back(Point2D<float>{3.0f, 4.0f});// 3-4-5 triangle
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

TEST_CASE("Data Transform: Line Alignment - FWHM center calculation - Edge cases and error handling", "[line][alignment][fwhm][edge]") {

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
        Point2D<float> perp_dir{0.0f, 0.0f};// Zero direction

        Point2D<float> center_point = calculate_fwhm_center(
                vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);

        // Should handle zero direction gracefully
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(50.0f, 0.001f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(50.0f, 0.001f));
    }

    SECTION("Empty image data") {
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data;// Empty

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

        Point2D<float> vertex{150.0f, 150.0f};// Outside bounds
        Point2D<float> perp_dir{0.0f, 1.0f};

        Point2D<float> center_point = calculate_fwhm_center(
                vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);

        // Should handle out-of-bounds vertex gracefully
        REQUIRE_THAT(center_point.x, Catch::Matchers::WithinAbs(150.0f, 0.001f));
        REQUIRE_THAT(center_point.y, Catch::Matchers::WithinAbs(150.0f, 0.001f));
    }
}

#if LINE_ALIGNMENT_DEBUG_MODE
TEST_CASE("Data Transform: Line Alignment - FWHM profile extents calculation - Debug mode", "[line][alignment][fwhm][debug]") {

    SECTION("Debug mode profile extents for bright line") {
        // Create a test image with a bright horizontal line
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0);// All black initially

        // Create a bright horizontal line at y=60
        for (int x = 0; x < 100; ++x) {
            image_data[60 * 100 + x] = 255;// Bright white line
        }

        // Test vertex at (50, 50) with perpendicular direction pointing down
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f};// Pointing down

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
        std::vector<uint8_t> image_data(100 * 100, 0);// All black initially

        // Create a bright horizontal line at y=60 with some thickness
        for (int x = 0; x < 100; ++x) {
            // Create a line with multiple maximum values at different positions
            for (int y = 58; y <= 62; ++y) {
                if (y >= 0 && y < 100) {
                    image_data[y * 100 + x] = 255;// Bright white line
                }
            }
        }

        // Test vertex at (50, 50) with perpendicular direction pointing down
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f};// Pointing down

        Line2D debug_line = calculate_fwhm_profile_extents(
                vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);

        // Should return a line with 3 points: [left_extent, max_point, right_extent]
        REQUIRE(debug_line.size() == 3);

        // First point should be left extent
        REQUIRE_THAT(debug_line[0].x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(debug_line[0].y, Catch::Matchers::WithinAbs(60.0f, 5.0f));

        // Second point should be max point (average of multiple max positions)
        REQUIRE_THAT(debug_line[1].x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(debug_line[1].y, Catch::Matchers::WithinAbs(60.0f, 2.0f));// Should be around center of thick line

        // Third point should be right extent
        REQUIRE_THAT(debug_line[2].x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
        REQUIRE_THAT(debug_line[2].y, Catch::Matchers::WithinAbs(60.0f, 5.0f));
    }
}
#endif

#include "DataManager.hpp"
#include "IO/LoaderRegistry.hpp"
#include "Lines/Line_Data.hpp"
#include "Media/Media_Data.hpp"
#include "transforms/TransformPipeline.hpp"
#include "transforms/TransformRegistry.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

TEST_CASE("Data Transform: Line Alignment - JSON pipeline", "[transforms][line_alignment][json]") {
    // Create DataManager and populate it with LineData and MediaData in code
    DataManager dm;

    // Create a TimeFrame for our data
    auto time_frame = std::make_shared<TimeFrame>();
    dm.setTime(TimeKey("default"), time_frame);

    // Create test line data - a simple horizontal line
    Line2D test_line;
    test_line.push_back(Point2D<float>{10.0f, 50.0f});
    test_line.push_back(Point2D<float>{50.0f, 50.0f});
    test_line.push_back(Point2D<float>{90.0f, 50.0f});

    auto test_line_data = std::make_shared<LineData>();
    test_line_data->setTimeFrame(time_frame);
    test_line_data->addAtTime(TimeFrameIndex(0), test_line);

    // Store the line data in DataManager with a known key
    dm.setData("test_line", test_line_data, TimeKey("default"));

    // Create test media data with a bright horizontal line at y=60
    auto media_data = std::make_shared<MockMediaData>();
    media_data->setTimeFrame(time_frame);

    // Create a test image with a bright horizontal line at y=60
    ImageSize image_size{100, 100};
    std::vector<uint8_t> image_data(100 * 100, 0);// All black initially

    // Create a bright horizontal line at y=60
    for (int x = 0; x < 100; ++x) {
        image_data[60 * 100 + x] = 255;// Bright white line
    }

    // Add the image to media data
    media_data->addImage(image_data, image_size);

    // Store the media data in DataManager with a known key
    dm.setData("test_media", media_data, TimeKey("default"));

    // Create JSON configuration for transformation pipeline using unified format
    char const * json_config =
            "[\n"
            "{\n"
            "    \"transformations\": {\n"
            "        \"metadata\": {\n"
            "            \"name\": \"Line Alignment Pipeline\",\n"
            "            \"description\": \"Test line alignment to bright features in media\",\n"
            "            \"version\": \"1.0\"\n"
            "        },\n"
            "        \"steps\": [\n"
            "            {\n"
            "                \"step_id\": \"1\",\n"
            "                \"transform_name\": \"Line Alignment to Bright Features\",\n"
            "                \"phase\": \"analysis\",\n"
            "                \"input_key\": \"test_line\",\n"
            "                \"output_key\": \"aligned_line\",\n"
            "                \"parameters\": {\n"
            "                    \"media_data\": \"test_media\",\n"
            "                    \"width\": 20,\n"
            "                    \"perpendicular_range\": 50,\n"
            "                    \"use_processed_data\": true,\n"
            "                    \"approach\": \"PEAK_WIDTH_HALF_MAX\",\n"
            "                    \"output_mode\": \"ALIGNED_VERTICES\"\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";

    // Create temporary directory and write JSON config to file
    std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "line_alignment_pipeline_test";
    std::filesystem::create_directories(test_dir);

    std::filesystem::path json_filepath = test_dir / "pipeline_config.json";
    {
        std::ofstream json_file(json_filepath);
        REQUIRE(json_file.is_open());
        json_file << json_config;
        json_file.close();
    }

    // Execute the transformation pipeline using load_data_from_json_config
    auto data_info_list = load_data_from_json_config(&dm, json_filepath.string());

    // Verify the transformation was executed and results are available
    auto result_line = dm.getData<LineData>("aligned_line");
    REQUIRE(result_line != nullptr);

    // Verify the line alignment results
    // The original line was at y=50, but there's a bright line at y=60
    // The aligned line should have vertices moved to y=60
    auto aligned_lines = result_line->getAtTime(TimeFrameIndex(0));
    REQUIRE(aligned_lines.size() == 1);
    auto aligned_vertices = aligned_lines[0];
    REQUIRE(aligned_vertices.size() == 3);

    // Check that vertices were aligned to the bright line at y=60
    REQUIRE_THAT(aligned_vertices[0].x, Catch::Matchers::WithinAbs(10.0f, 1.0f));
    REQUIRE_THAT(aligned_vertices[0].y, Catch::Matchers::WithinAbs(60.0f, 1.0f));
    REQUIRE_THAT(aligned_vertices[1].x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
    REQUIRE_THAT(aligned_vertices[1].y, Catch::Matchers::WithinAbs(60.0f, 1.0f));
    REQUIRE_THAT(aligned_vertices[2].x, Catch::Matchers::WithinAbs(90.0f, 1.0f));
    REQUIRE_THAT(aligned_vertices[2].y, Catch::Matchers::WithinAbs(60.0f, 1.0f));

    // Test another pipeline with different parameters (different width and range)
    char const * json_config_different_params =
            "[\n"
            "{\n"
            "    \"transformations\": {\n"
            "        \"metadata\": {\n"
            "            \"name\": \"Line Alignment with Different Parameters\",\n"
            "            \"description\": \"Test line alignment with different width and range\",\n"
            "            \"version\": \"1.0\"\n"
            "        },\n"
            "        \"steps\": [\n"
            "            {\n"
            "                \"step_id\": \"1\",\n"
            "                \"transform_name\": \"Line Alignment to Bright Features\",\n"
            "                \"phase\": \"analysis\",\n"
            "                \"input_key\": \"test_line\",\n"
            "                \"output_key\": \"aligned_line_different_params\",\n"
            "                \"parameters\": {\n"
            "                    \"media_data\": \"test_media\",\n"
            "                    \"width\": 10,\n"
            "                    \"perpendicular_range\": 30,\n"
            "                    \"use_processed_data\": true,\n"
            "                    \"approach\": \"PEAK_WIDTH_HALF_MAX\",\n"
            "                    \"output_mode\": \"ALIGNED_VERTICES\"\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";

    std::filesystem::path json_filepath_different_params = test_dir / "pipeline_config_different_params.json";
    {
        std::ofstream json_file(json_filepath_different_params);
        REQUIRE(json_file.is_open());
        json_file << json_config_different_params;
        json_file.close();
    }

    // Execute the different parameters pipeline
    auto data_info_list_different_params = load_data_from_json_config(&dm, json_filepath_different_params.string());

    // Verify the different parameters results
    auto result_line_different_params = dm.getData<LineData>("aligned_line_different_params");
    REQUIRE(result_line_different_params != nullptr);

    auto aligned_lines_different_params = result_line_different_params->getAtTime(TimeFrameIndex(0));
    REQUIRE(aligned_lines_different_params.size() == 1);
    auto aligned_vertices_different_params = aligned_lines_different_params[0];
    REQUIRE(aligned_vertices_different_params.size() == 3);

    // Should still align to the bright line at y=60
    REQUIRE_THAT(aligned_vertices_different_params[0].x, Catch::Matchers::WithinAbs(10.0f, 1.0f));
    REQUIRE_THAT(aligned_vertices_different_params[0].y, Catch::Matchers::WithinAbs(60.0f, 1.0f));
    REQUIRE_THAT(aligned_vertices_different_params[1].x, Catch::Matchers::WithinAbs(50.0f, 1.0f));
    REQUIRE_THAT(aligned_vertices_different_params[1].y, Catch::Matchers::WithinAbs(60.0f, 1.0f));
    REQUIRE_THAT(aligned_vertices_different_params[2].x, Catch::Matchers::WithinAbs(90.0f, 1.0f));
    REQUIRE_THAT(aligned_vertices_different_params[2].y, Catch::Matchers::WithinAbs(60.0f, 1.0f));

    // Test FWHM profile extents output mode
    char const * json_config_fwhm_extents =
            "[\n"
            "{\n"
            "    \"transformations\": {\n"
            "        \"metadata\": {\n"
            "            \"name\": \"Line Alignment FWHM Extents\",\n"
            "            \"description\": \"Test line alignment with FWHM profile extents output\",\n"
            "            \"version\": \"1.0\"\n"
            "        },\n"
            "        \"steps\": [\n"
            "            {\n"
            "                \"step_id\": \"1\",\n"
            "                \"transform_name\": \"Line Alignment to Bright Features\",\n"
            "                \"phase\": \"analysis\",\n"
            "                \"input_key\": \"test_line\",\n"
            "                \"output_key\": \"fwhm_extents_line\",\n"
            "                \"parameters\": {\n"
            "                    \"media_data\": \"test_media\",\n"
            "                    \"width\": 20,\n"
            "                    \"perpendicular_range\": 50,\n"
            "                    \"use_processed_data\": true,\n"
            "                    \"approach\": \"PEAK_WIDTH_HALF_MAX\",\n"
            "                    \"output_mode\": \"FWHM_PROFILE_EXTENTS\"\n"
            "                }\n"
            "            }\n"
            "        ]\n"
            "    }\n"
            "}\n"
            "]";

    std::filesystem::path json_filepath_fwhm_extents = test_dir / "pipeline_config_fwhm_extents.json";
    {
        std::ofstream json_file(json_filepath_fwhm_extents);
        REQUIRE(json_file.is_open());
        json_file << json_config_fwhm_extents;
        json_file.close();
    }

    // Execute the FWHM extents pipeline
    auto data_info_list_fwhm_extents = load_data_from_json_config(&dm, json_filepath_fwhm_extents.string());

    // Verify the FWHM extents results
    auto result_line_fwhm_extents = dm.getData<LineData>("fwhm_extents_line");
    REQUIRE(result_line_fwhm_extents != nullptr);

    auto fwhm_extents_lines = result_line_fwhm_extents->getAtTime(TimeFrameIndex(0));
    REQUIRE(fwhm_extents_lines.size() == 3);// One line per vertex
    auto fwhm_extents_vertices = fwhm_extents_lines[0];
    // Should have 3 points per vertex: [left_extent, max_point, right_extent]
    // For 3 vertices, this should be 9 points total
    REQUIRE(fwhm_extents_vertices.size() == 3);

    // Cleanup
    try {
        std::filesystem::remove_all(test_dir);
    } catch (std::exception const & e) {
        std::cerr << "Warning: Cleanup failed: " << e.what() << std::endl;
    }
}

TEST_CASE("Data Transform: Line Alignment - Line alignment with 8-bit and 32-bit media data", "[line][alignment][media][transform]") {

    SECTION("8-bit media data alignment") {
        // Create a test image with a bright horizontal line
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0);// All black initially

        // Create a bright horizontal line at y=60
        for (int x = 0; x < 100; ++x) {
            image_data[60 * 100 + x] = 255;// Bright white line
        }

        // Create mock media data with 8-bit data
        auto media_data = std::make_shared<MockMediaData>(MediaData::BitDepth::Bit8);
        media_data->addImage8(image_data, image_size);

        // Create test line data
        auto line_data = std::make_shared<LineData>();
        line_data->setImageSize(image_size);

        // Create a test line with vertices that should align to the bright line
        Line2D test_line;
        test_line.push_back(Point2D<float>{30.0f, 50.0f});// Should move to y=60
        test_line.push_back(Point2D<float>{50.0f, 55.0f});// Should move to y=60
        test_line.push_back(Point2D<float>{70.0f, 45.0f});// Should move to y=60

        line_data->addAtTime(TimeFrameIndex(0), test_line, false);

        // Test the line alignment function
        auto aligned_line_data = line_alignment(
                line_data.get(),
                media_data.get(),
                20,   // width
                50,   // perpendicular_range
                false,// use_processed_data
                FWHMApproach::PEAK_WIDTH_HALF_MAX,
                LineAlignmentOutputMode::ALIGNED_VERTICES);

        REQUIRE(aligned_line_data != nullptr);

        // Check that the lines were aligned
        auto aligned_lines = aligned_line_data->getAtTime(TimeFrameIndex(0));
        REQUIRE(aligned_lines.size() == 1);

        auto const & aligned_line = aligned_lines[0];
        REQUIRE(aligned_line.size() == 3);

        // All vertices should be aligned to y=60 (the bright line)
        for (auto const & vertex: aligned_line) {
            REQUIRE_THAT(vertex.y, Catch::Matchers::WithinAbs(60.0f, 2.0f));
        }
    }

    SECTION("32-bit media data alignment") {
        // Create a test image with a bright horizontal line using 32-bit float data
        ImageSize image_size{100, 100};
        std::vector<float> image_data(100 * 100, 0.0f);// All black initially

        // Create a bright horizontal line at y=60 with float intensity
        for (int x = 0; x < 100; ++x) {
            image_data[60 * 100 + x] = 1.0f;// Bright white line (normalized to 0-1)
        }

        // Create mock media data with 32-bit data
        auto media_data = std::make_shared<MockMediaData>(MediaData::BitDepth::Bit32);
        media_data->addImage32(image_data, image_size);

        // Create test line data
        auto line_data = std::make_shared<LineData>();
        line_data->setImageSize(image_size);

        // Create a test line with vertices that should align to the bright line
        Line2D test_line;
        test_line.push_back(Point2D<float>{30.0f, 50.0f});// Should move to y=60
        test_line.push_back(Point2D<float>{50.0f, 55.0f});// Should move to y=60
        test_line.push_back(Point2D<float>{70.0f, 45.0f});// Should move to y=60

        line_data->addAtTime(TimeFrameIndex(0), test_line, false);

        // Test the line alignment function
        auto aligned_line_data = line_alignment(
                line_data.get(),
                media_data.get(),
                20,   // width
                50,   // perpendicular_range
                false,// use_processed_data
                FWHMApproach::PEAK_WIDTH_HALF_MAX,
                LineAlignmentOutputMode::ALIGNED_VERTICES);

        REQUIRE(aligned_line_data != nullptr);

        // Check that the lines were aligned
        auto aligned_lines = aligned_line_data->getAtTime(TimeFrameIndex(0));
        REQUIRE(aligned_lines.size() == 1);

        auto const & aligned_line = aligned_lines[0];
        REQUIRE(aligned_line.size() == 3);

        // All vertices should be aligned to y=60 (the bright line)
        for (auto const & vertex: aligned_line) {
            REQUIRE_THAT(vertex.y, Catch::Matchers::WithinAbs(60.0f, 2.0f));
        }
    }

    SECTION("Compare 8-bit vs 32-bit alignment results") {
        ImageSize image_size{100, 100};

        // Create identical bright horizontal lines in both formats
        std::vector<uint8_t> image_data_8bit(100 * 100, 0);
        std::vector<float> image_data_32bit(100 * 100, 0.0f);

        for (int x = 0; x < 100; ++x) {
            image_data_8bit[60 * 100 + x] = 255;
            image_data_32bit[60 * 100 + x] = 1.0f;
        }

        // Create mock media data for both bit depths
        auto media_data_8bit = std::make_shared<MockMediaData>(MediaData::BitDepth::Bit8);
        media_data_8bit->addImage8(image_data_8bit, image_size);

        auto media_data_32bit = std::make_shared<MockMediaData>(MediaData::BitDepth::Bit32);
        media_data_32bit->addImage32(image_data_32bit, image_size);

        // Create identical test line data for both tests
        auto line_data = std::make_shared<LineData>();
        line_data->setImageSize(image_size);

        Line2D test_line;
        test_line.push_back(Point2D<float>{30.0f, 50.0f});
        test_line.push_back(Point2D<float>{50.0f, 55.0f});
        test_line.push_back(Point2D<float>{70.0f, 45.0f});

        line_data->addAtTime(TimeFrameIndex(0), test_line, false);

        // Test alignment with 8-bit data
        auto aligned_line_data_8bit = line_alignment(
                line_data.get(),
                media_data_8bit.get(),
                20, 50, false,
                FWHMApproach::PEAK_WIDTH_HALF_MAX,
                LineAlignmentOutputMode::ALIGNED_VERTICES);

        // Test alignment with 32-bit data
        auto aligned_line_data_32bit = line_alignment(
                line_data.get(),
                media_data_32bit.get(),
                20, 50, false,
                FWHMApproach::PEAK_WIDTH_HALF_MAX,
                LineAlignmentOutputMode::ALIGNED_VERTICES);

        REQUIRE(aligned_line_data_8bit != nullptr);
        REQUIRE(aligned_line_data_32bit != nullptr);

        // Get aligned lines from both results
        auto aligned_lines_8bit = aligned_line_data_8bit->getAtTime(TimeFrameIndex(0));
        auto aligned_lines_32bit = aligned_line_data_32bit->getAtTime(TimeFrameIndex(0));

        REQUIRE(aligned_lines_8bit.size() == 1);
        REQUIRE(aligned_lines_32bit.size() == 1);

        auto const & aligned_line_8bit = aligned_lines_8bit[0];
        auto const & aligned_line_32bit = aligned_lines_32bit[0];

        REQUIRE(aligned_line_8bit.size() == 3);
        REQUIRE(aligned_line_32bit.size() == 3);

        // Results should be very similar between 8-bit and 32-bit
        for (size_t i = 0; i < 3; ++i) {
            REQUIRE_THAT(aligned_line_8bit[i].x, Catch::Matchers::WithinAbs(aligned_line_32bit[i].x, 1.0f));
            REQUIRE_THAT(aligned_line_8bit[i].y, Catch::Matchers::WithinAbs(aligned_line_32bit[i].y, 1.0f));
        }
    }
}