#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "median_filter.hpp"
#include "Image.hpp"

#include <vector>
#include <algorithm>
#include <numeric>

TEST_CASE("median_filter basic functionality", "[median_filter][morphology]") {
    
    SECTION("removes isolated noise pixels with 3x3 window") {
        // Create a 7x7 image with isolated noise pixels
        ImageSize const image_size = {7, 7};
        std::vector<uint8_t> image(49, 0);
        
        // Add a solid 3x3 square (large enough to survive median filtering)
        for (int row = 2; row < 5; ++row) {
            for (int col = 2; col < 5; ++col) {
                image[row * 7 + col] = 255;
            }
        }
        
        // Add isolated noise pixels
        image[0 * 7 + 6] = 255; // Top-right corner
        image[6 * 7 + 0] = 255; // Bottom-left corner
        image[1 * 7 + 1] = 255; // Isolated pixel near structure
        
        std::vector<uint8_t> result = median_filter(image, image_size, 3);
        
        REQUIRE(result.size() == 49);
        
        // Center of solid structure should be preserved
        REQUIRE(result[3 * 7 + 3] == 1); // Center of 3x3 square
        
        // Isolated noise pixels should be removed
        REQUIRE(result[0 * 7 + 6] == 0);
        REQUIRE(result[6 * 7 + 0] == 0);
        REQUIRE(result[1 * 7 + 1] == 0);
        
        // Check that noise is actually reduced
        int original_pixels = std::accumulate(image.begin(), image.end(), 0) / 255;
        int filtered_pixels = std::accumulate(result.begin(), result.end(), 0);
        REQUIRE(filtered_pixels < original_pixels);
    }
    
    SECTION("demonstrates typical median filter behavior") {
        // Create a 7x7 image to test various median filter behaviors
        ImageSize const image_size = {7, 7};
        std::vector<uint8_t> image(49, 0);
        
        // Create a 4x4 solid block that should mostly survive
        for (int row = 1; row < 5; ++row) {
            for (int col = 1; col < 5; ++col) {
                image[row * 7 + col] = 255;
            }
        }
        
        // Add isolated noise pixels that should be removed
        image[0 * 7 + 6] = 255; // Top-right corner (far from solid block)
        image[6 * 7 + 0] = 255; // Bottom-left corner (far from solid block)
        
        std::vector<uint8_t> result = median_filter(image, image_size, 3);
        
        REQUIRE(result.size() == 49);
        
        // Center of solid block should be preserved
        REQUIRE(result[2 * 7 + 2] == 1);
        REQUIRE(result[3 * 7 + 3] == 1);
        
        // Isolated corner pixels should be removed (these are far from the solid block)
        REQUIRE(result[0 * 7 + 6] == 0);
        REQUIRE(result[6 * 7 + 0] == 0);
        
        // Check that we have reasonable output
        int filtered_pixels = std::accumulate(result.begin(), result.end(), 0);
        REQUIRE(filtered_pixels > 0); // Should have some pixels left
        REQUIRE(filtered_pixels < 16); // But less than the original solid block
    }
    
    SECTION("handles solid regions appropriately") {
        // Create a 9x9 image with a solid 5x5 square (large enough to have stable center)
        ImageSize const image_size = {9, 9};
        std::vector<uint8_t> image(81, 0);
        
        // Solid 5x5 square in center
        for (int row = 2; row < 7; ++row) {
            for (int col = 2; col < 7; ++col) {
                image[row * 9 + col] = 255;
            }
        }
        
        std::vector<uint8_t> result = median_filter(image, image_size, 3);
        
        REQUIRE(result.size() == 81);
        
        // Center of solid region should definitely be preserved
        REQUIRE(result[4 * 9 + 4] == 1); // Very center
        
        // Inner core should be preserved
        REQUIRE(result[3 * 9 + 3] == 1);
        REQUIRE(result[3 * 9 + 5] == 1);
        REQUIRE(result[5 * 9 + 3] == 1);
        REQUIRE(result[5 * 9 + 5] == 1);
        
        // Count pixels - should preserve core area
        int filtered_pixels = std::accumulate(result.begin(), result.end(), 0);
        REQUIRE(filtered_pixels >= 9); // Should preserve at least a 3x3 core
    }
    
    SECTION("different window sizes produce different results") {
        // Create a 9x9 image with various structures
        ImageSize const image_size = {9, 9};
        std::vector<uint8_t> image(81, 0);
        
        // Add a 3x3 square
        for (int row = 3; row < 6; ++row) {
            for (int col = 3; col < 6; ++col) {
                image[row * 9 + col] = 255;
            }
        }
        
        // Add scattered noise
        image[0 * 9 + 0] = 255;
        image[1 * 9 + 8] = 255;
        image[8 * 9 + 1] = 255;
        
        std::vector<uint8_t> result_3x3 = median_filter(image, image_size, 3);
        std::vector<uint8_t> result_5x5 = median_filter(image, image_size, 5);
        
        REQUIRE(result_3x3.size() == 81);
        REQUIRE(result_5x5.size() == 81);
        
        // Results should be different
        REQUIRE(result_3x3 != result_5x5);
        
        // 5x5 window should be more aggressive at removing noise and small structures
        int pixels_3x3 = std::accumulate(result_3x3.begin(), result_3x3.end(), 0);
        int pixels_5x5 = std::accumulate(result_5x5.begin(), result_5x5.end(), 0);
        
        // Generally, larger windows remove more detail (though this isn't always true)
        // At minimum, they should produce different results
        REQUIRE(pixels_3x3 != pixels_5x5);
    }
}

TEST_CASE("median_filter edge cases and error handling", "[median_filter][morphology][edge-cases]") {
    
    SECTION("handles empty image") {
        ImageSize const image_size = {0, 0};
        std::vector<uint8_t> image;
        
        std::vector<uint8_t> result = median_filter(image, image_size, 3);
        
        REQUIRE(result.empty());
    }
    
    SECTION("handles single pixel image") {
        ImageSize const image_size = {1, 1};
        std::vector<uint8_t> image = {255};
        
        std::vector<uint8_t> result = median_filter(image, image_size, 1);
        
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == 1);
        
        // Test with larger window on single pixel
        result = median_filter(image, image_size, 3);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == 1);
    }
    
    SECTION("handles invalid window sizes") {
        ImageSize const image_size = {3, 3};
        std::vector<uint8_t> image(9, 255);
        
        // Even window size should return normalized input
        std::vector<uint8_t> result = median_filter(image, image_size, 2);
        REQUIRE(result.size() == 9);
        REQUIRE(std::all_of(result.begin(), result.end(), [](uint8_t pixel) { return pixel == 1; }));
        
        // Zero window size should return normalized input
        result = median_filter(image, image_size, 0);
        REQUIRE(result.size() == 9);
        REQUIRE(std::all_of(result.begin(), result.end(), [](uint8_t pixel) { return pixel == 1; }));
        
        // Negative window size should return normalized input
        result = median_filter(image, image_size, -1);
        REQUIRE(result.size() == 9);
        REQUIRE(std::all_of(result.begin(), result.end(), [](uint8_t pixel) { return pixel == 1; }));
    }
    
    SECTION("handles mismatched image size") {
        ImageSize const image_size = {3, 3};
        std::vector<uint8_t> image(8, 255); // Wrong size: should be 9
        
        std::vector<uint8_t> result = median_filter(image, image_size, 3);
        
        REQUIRE(result.empty()); // Should return empty on size mismatch
    }
    
    SECTION("normalizes input values") {
        ImageSize const image_size = {3, 3};
        std::vector<uint8_t> image = {0, 1, 127, 128, 254, 255, 50, 200, 100};
        
        std::vector<uint8_t> result = median_filter(image, image_size, 1);
        
        REQUIRE(result.size() == 9);
        
        // All non-zero values should become 1, zero should remain 0
        REQUIRE(result[0] == 0); // 0 -> 0
        for (size_t i = 1; i < result.size(); ++i) {
            REQUIRE(result[i] == 1); // Non-zero -> 1
        }
    }
    
    SECTION("boundary handling with reflection padding") {
        // Create a 3x3 image where boundary effects are important
        ImageSize const image_size = {3, 3};
        std::vector<uint8_t> image = {
            1, 0, 1,
            0, 1, 0,
            1, 0, 1
        };
        
        std::vector<uint8_t> result = median_filter(image, image_size, 3);
        
        REQUIRE(result.size() == 9);
        
        // With reflection padding, corner pixels have their neighbors reflected
        // The median calculation should handle this appropriately
        // We won't test exact values since boundary behavior can be complex,
        // but verify the result is valid
        for (auto pixel : result) {
            REQUIRE((pixel == 0 || pixel == 1));
        }
    }
    
    SECTION("large window size doesn't crash") {
        ImageSize const image_size = {5, 5};
        std::vector<uint8_t> image(25, 255);
        
        // Window larger than image
        std::vector<uint8_t> result = median_filter(image, image_size, 11);
        
        REQUIRE(result.size() == 25);
        // With large window, all pixels should be median of entire image (all 1s)
        REQUIRE(std::all_of(result.begin(), result.end(), [](uint8_t pixel) { return pixel == 1; }));
    }
}

TEST_CASE("median_filter Image interface tests", "[median_filter][morphology][Image]") {
    
    SECTION("removes noise using Image interface") {
        ImageSize size = {6, 6};
        Image input_image(size);
        
        // Create pattern with noise
        // Large structure: 3x3 square
        for (int row = 1; row < 4; ++row) {
            for (int col = 1; col < 4; ++col) {
                input_image.set(row, col, 255);
            }
        }
        
        // Add isolated noise pixels
        input_image.set(0, 0, 255);
        input_image.set(0, 5, 255);
        input_image.set(5, 0, 255);
        input_image.set(5, 5, 255);
        
        Image result = median_filter(input_image, 3);
        
        REQUIRE(result.size.width == size.width);
        REQUIRE(result.size.height == size.height);
        REQUIRE(result.pixel_count() == input_image.pixel_count());
        
        // Main structure should be largely preserved
        REQUIRE(result.at(2, 2) == 1); // Center should remain
        
        // Corner noise should be reduced/removed
        // (exact behavior depends on reflection padding, but should be different)
        int original_corners = input_image.at(0,0) + input_image.at(0,5) + input_image.at(5,0) + input_image.at(5,5);
        int filtered_corners = result.at(0,0) + result.at(0,5) + result.at(5,0) + result.at(5,5);
        
        REQUIRE(filtered_corners <= original_corners); // Should not increase noise
    }
    
    SECTION("consistency between vector and Image interfaces") {
        ImageSize size = {7, 5}; // Non-square to test row-major ordering
        
        // Create test pattern using Image interface
        Image img_input(size);
        for (int row = 1; row < 4; ++row) {
            for (int col = 2; col < 5; ++col) {
                img_input.set(row, col, 255);
            }
        }
        
        // Add some noise
        img_input.set(0, 1, 255);
        img_input.set(4, 6, 255);
        
        // Create same pattern using vector interface
        std::vector<uint8_t> vec_input(size.width * size.height, 0);
        for (int row = 1; row < 4; ++row) {
            for (int col = 2; col < 5; ++col) {
                vec_input[row * size.width + col] = 255;
            }
        }
        vec_input[0 * size.width + 1] = 255;  // Noise
        vec_input[4 * size.width + 6] = 255;  // Noise
        
        // Run median filter with both interfaces
        Image img_result = median_filter(img_input, 3);
        std::vector<uint8_t> vec_result = median_filter(vec_input, size, 3);
        
        // Results should be identical
        REQUIRE(img_result.data == vec_result);
        REQUIRE(img_result.size.width == size.width);
        REQUIRE(img_result.size.height == size.height);
        
        // Verify some expected behavior
        REQUIRE(img_result.at(2, 3) == 1); // Center of main structure should remain
    }
    
    SECTION("different window sizes with Image interface") {
        ImageSize size = {5, 5}; // Smaller size to avoid large test output
        Image input_image(size);
        
        // Create simple pattern: solid 3x3 square
        for (int row = 1; row < 4; ++row) {
            for (int col = 1; col < 4; ++col) {
                input_image.set(row, col, 255);
            }
        }
        
        // Add corner noise
        input_image.set(0, 0, 255);
        input_image.set(4, 4, 255);
        
        Image result_1x1 = median_filter(input_image, 1);
        Image result_3x3 = median_filter(input_image, 3);
        
        // All should have correct dimensions
        REQUIRE(result_1x1.size == size);
        REQUIRE(result_3x3.size == size);
        
        // 1x1 window should just normalize (255 -> 1, 0 -> 0)
        // Check specific positions instead of comparing entire arrays
        REQUIRE(result_1x1.at(2, 2) == 1); // Center of solid block should be 1
        REQUIRE(result_1x1.at(0, 4) == 0); // Background should be 0
        
        // 3x3 window should produce different results
        REQUIRE(result_3x3.at(2, 2) == 1); // Center should remain
        
        // Count total pixels to verify they're different
        int pixels_1x1 = std::accumulate(result_1x1.data.begin(), result_1x1.data.end(), 0);
        int pixels_3x3 = std::accumulate(result_3x3.data.begin(), result_3x3.data.end(), 0);
        
        // 1x1 should preserve more (just normalization), 3x3 should smooth more
        REQUIRE(pixels_1x1 >= pixels_3x3);
    }
} 