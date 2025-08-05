#include "transforms/Lines/line_alignment.hpp"
#include "CoreGeometry/points.hpp"
#include "CoreGeometry/ImageSize.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
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
        
        float displacement = calculate_fwhm_displacement(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should find the bright line and return 0 displacement (already on the line)
        REQUIRE_THAT(displacement, Catch::Matchers::WithinAbs(0.0f, 1.0f));
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
        
        float displacement = calculate_fwhm_displacement(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should find the bright line at y=60 and return displacement of 10
        REQUIRE_THAT(displacement, Catch::Matchers::WithinAbs(10.0f, 1.0f));
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
        
        float displacement = calculate_fwhm_displacement(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should find the bright line and return appropriate displacement
        REQUIRE_THAT(displacement, Catch::Matchers::WithinAbs(7.07f, 2.0f)); // Approximately sqrt(50)
    }
    
    SECTION("No bright features - should return zero") {
        // Create a completely black image
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0); // All black
        
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f}; // Pointing down
        
        float displacement = calculate_fwhm_displacement(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should return 0 when no bright features are found
        REQUIRE_THAT(displacement, Catch::Matchers::WithinAbs(0.0f, 0.001f));
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
        
        float displacement = calculate_fwhm_displacement(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should find the bright spot and return displacement of 10
        REQUIRE_THAT(displacement, Catch::Matchers::WithinAbs(10.0f, 1.0f));
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
        
        float displacement = calculate_fwhm_displacement(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should find the closer line at y=30 and return displacement of -20
        REQUIRE_THAT(displacement, Catch::Matchers::WithinAbs(-20.0f, 1.0f));
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
        
        float displacement = calculate_fwhm_displacement(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should handle boundary case gracefully
        REQUIRE_THAT(displacement, Catch::Matchers::WithinAbs(0.0f, 1.0f));
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
        float displacement1 = calculate_fwhm_displacement(
            vertex, perp_dir, 10, 20, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        float displacement2 = calculate_fwhm_displacement(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Both should find the same line and return similar displacement
        REQUIRE_THAT(displacement1, Catch::Matchers::WithinAbs(10.0f, 1.0f));
        REQUIRE_THAT(displacement2, Catch::Matchers::WithinAbs(10.0f, 1.0f));
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
        float displacement1 = calculate_fwhm_displacement(
            vertex, perp_dir, 5, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        float displacement2 = calculate_fwhm_displacement(
            vertex, perp_dir, 20, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Both should find the same line and return similar displacement
        REQUIRE_THAT(displacement1, Catch::Matchers::WithinAbs(10.0f, 1.0f));
        REQUIRE_THAT(displacement2, Catch::Matchers::WithinAbs(10.0f, 1.0f));
    }
}

TEST_CASE("FWHM displacement calculation - Edge cases and error handling", "[line][alignment][fwhm][edge]") {
    
    SECTION("Zero width parameter") {
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0);
        
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f};
        
        float displacement = calculate_fwhm_displacement(
            vertex, perp_dir, 0, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should return 0 for invalid width
        REQUIRE_THAT(displacement, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Zero perpendicular direction") {
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0);
        
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 0.0f}; // Zero direction
        
        float displacement = calculate_fwhm_displacement(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should handle zero direction gracefully
        REQUIRE_THAT(displacement, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Empty image data") {
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data; // Empty
        
        Point2D<float> vertex{50.0f, 50.0f};
        Point2D<float> perp_dir{0.0f, 1.0f};
        
        float displacement = calculate_fwhm_displacement(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should handle empty data gracefully
        REQUIRE_THAT(displacement, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }
    
    SECTION("Vertex outside image bounds") {
        ImageSize image_size{100, 100};
        std::vector<uint8_t> image_data(100 * 100, 0);
        
        Point2D<float> vertex{150.0f, 150.0f}; // Outside bounds
        Point2D<float> perp_dir{0.0f, 1.0f};
        
        float displacement = calculate_fwhm_displacement(
            vertex, perp_dir, 10, 50, image_data, image_size, FWHMApproach::PEAK_WIDTH_HALF_MAX);
        
        // Should handle out-of-bounds vertex gracefully
        REQUIRE_THAT(displacement, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    }
} 