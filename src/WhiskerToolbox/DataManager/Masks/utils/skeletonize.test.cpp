#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "skeletonize.hpp"
#include "Image.hpp"

#include <vector>
#include <algorithm>
#include <numeric>

TEST_CASE("fast_skeletonize happy path tests", "[skeletonize][morphology]") {
    
    SECTION("horizontal rectangle reduces to horizontal line") {
        // Create a 5x15 horizontal rectangle (5 rows, 15 columns)
        size_t const height = 5;
        size_t const width = 15;
        std::vector<uint8_t> image(height * width, 0);
        
        // Fill the rectangle with foreground pixels (value 255)
        std::fill(image.begin(), image.end(), 255);
        
        // Perform skeletonization
        std::vector<uint8_t> result = fast_skeletonize(image, height, width);
        
        // The result should have the same dimensions
        REQUIRE(result.size() == height * width);
        
        // Count non-zero pixels in each row
        std::vector<size_t> row_counts(height, 0);
        for (size_t row = 0; row < height; ++row) {
            for (size_t col = 0; col < width; ++col) {
                if (result[row * width + col] > 0) {
                    row_counts[row]++;
                }
            }
        }
        
        // The skeleton should be concentrated in the middle row(s)
        // For a 5-row rectangle, we expect the skeleton mainly in row 2 (middle)
        size_t middle_row = height / 2;
        REQUIRE(row_counts[middle_row] > 0);
        
        // The skeleton should span most of the width
        REQUIRE(row_counts[middle_row] >= width * 0.8); // At least 80% of the width
        
        // Edge rows should have fewer or no pixels
        REQUIRE(row_counts[0] <= row_counts[middle_row]);
        REQUIRE(row_counts[height - 1] <= row_counts[middle_row]);
        
        // Total number of skeleton pixels should be much less than original
        size_t total_skeleton_pixels = std::accumulate(result.begin(), result.end(), 0);
        size_t original_pixels = height * width * 255;
        REQUIRE(total_skeleton_pixels < original_pixels / 2);
    }
    
    SECTION("vertical rectangle reduces to vertical line") {
        // Create a 15x5 vertical rectangle (15 rows, 5 columns)
        size_t const height = 15;
        size_t const width = 5;
        std::vector<uint8_t> image(height * width, 0);
        
        // Fill the rectangle with foreground pixels (value 255)
        std::fill(image.begin(), image.end(), 255);
        
        // Perform skeletonization
        std::vector<uint8_t> result = fast_skeletonize(image, height, width);
        
        // The result should have the same dimensions
        REQUIRE(result.size() == height * width);
        
        // Count non-zero pixels in each column
        std::vector<size_t> col_counts(width, 0);
        for (size_t col = 0; col < width; ++col) {
            for (size_t row = 0; row < height; ++row) {
                if (result[row * width + col] > 0) {
                    col_counts[col]++;
                }
            }
        }
        
        // The skeleton should be concentrated in the middle column(s)
        // For a 5-column rectangle, we expect the skeleton mainly in column 2 (middle)
        size_t middle_col = width / 2;
        REQUIRE(col_counts[middle_col] > 0);
        
        // The skeleton should span most of the height
        REQUIRE(col_counts[middle_col] >= height * 0.8); // At least 80% of the height
        
        // Edge columns should have fewer or no pixels
        REQUIRE(col_counts[0] <= col_counts[middle_col]);
        REQUIRE(col_counts[width - 1] <= col_counts[middle_col]);
        
        // Total number of skeleton pixels should be much less than original
        size_t total_skeleton_pixels = std::accumulate(result.begin(), result.end(), 0);
        size_t original_pixels = height * width * 255;
        REQUIRE(total_skeleton_pixels < original_pixels / 2);
    }
    
    SECTION("single pixel remains unchanged") {
        // Test with a single pixel
        size_t const height = 1;
        size_t const width = 1;
        std::vector<uint8_t> image = {255};
        
        std::vector<uint8_t> result = fast_skeletonize(image, height, width);
        
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == 255); // Single pixel should remain
    }
}

TEST_CASE("fast_skeletonize edge cases and error handling", "[skeletonize][morphology][edge-cases]") {
    
    SECTION("empty image") {
        // Test with all zero pixels
        size_t const height = 10;
        size_t const width = 10;
        std::vector<uint8_t> image(height * width, 0);
        
        std::vector<uint8_t> result = fast_skeletonize(image, height, width);
        
        REQUIRE(result.size() == height * width);
        // All pixels should remain zero
        REQUIRE(std::all_of(result.begin(), result.end(), [](uint8_t pixel) { return pixel == 0; }));
    }
    
    SECTION("minimal dimensions") {
        // Test with 1x1 image
        size_t const height = 1;
        size_t const width = 1;
        std::vector<uint8_t> image = {0};
        
        std::vector<uint8_t> result = fast_skeletonize(image, height, width);
        
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == 0);
    }
    
    SECTION("thin line already skeletal") {
        // Test with an already thin horizontal line
        size_t const height = 3;
        size_t const width = 10;
        std::vector<uint8_t> image(height * width, 0);
        
        // Set middle row to foreground
        for (size_t col = 0; col < width; ++col) {
            image[1 * width + col] = 255; // Middle row
        }
        
        std::vector<uint8_t> result = fast_skeletonize(image, height, width);
        
        REQUIRE(result.size() == height * width);
        
        // The line should be preserved (though might be slightly modified)
        size_t middle_row_pixels = 0;
        for (size_t col = 0; col < width; ++col) {
            if (result[1 * width + col] > 0) {
                middle_row_pixels++;
            }
        }
        
        // Most of the middle row should still be present
        REQUIRE(middle_row_pixels >= width * 0.7);
    }
    
    SECTION("disconnected components") {
        // Test with multiple separate rectangles
        size_t const height = 10;
        size_t const width = 20;
        std::vector<uint8_t> image(height * width, 0);
        
        // Create two separate 3x3 squares
        for (size_t row = 2; row < 5; ++row) {
            for (size_t col = 2; col < 5; ++col) {
                image[row * width + col] = 255;
            }
        }
        
        for (size_t row = 6; row < 9; ++row) {
            for (size_t col = 15; col < 18; ++col) {
                image[row * width + col] = 255;
            }
        }
        
        std::vector<uint8_t> result = fast_skeletonize(image, height, width);
        
        REQUIRE(result.size() == height * width);
        
        // Both components should contribute to the skeleton
        size_t total_skeleton_pixels = std::accumulate(result.begin(), result.end(), 0);
        REQUIRE(total_skeleton_pixels > 0);
        REQUIRE(total_skeleton_pixels < 18 * 255); // Should be less than original pixel count
    }
}

TEST_CASE("fast_skeletonize Image interface tests", "[skeletonize][morphology][Image]") {
    
    SECTION("horizontal rectangle reduces to horizontal line using Image interface") {
        // Create a 5x15 horizontal rectangle (5 height, 15 width)
        ImageSize size = {15, 5}; // width, height
        Image input_image(size);
        
        // Fill the rectangle with foreground pixels (value 255)
        for (int row = 0; row < size.height; ++row) {
            for (int col = 0; col < size.width; ++col) {
                input_image.set(row, col, 255);
            }
        }
        
        // Perform skeletonization using Image interface
        Image result = fast_skeletonize(input_image);
        
        // The result should have the same dimensions
        REQUIRE(result.size.width == size.width);
        REQUIRE(result.size.height == size.height);
        REQUIRE(result.pixel_count() == input_image.pixel_count());
        
        // Count non-zero pixels in each row
        std::vector<size_t> row_counts(size.height, 0);
        for (int row = 0; row < size.height; ++row) {
            for (int col = 0; col < size.width; ++col) {
                if (result.at(row, col) > 0) {
                    row_counts[row]++;
                }
            }
        }
        
        // The skeleton should be concentrated in the middle row(s)
        size_t middle_row = size.height / 2;
        REQUIRE(row_counts[middle_row] > 0);
        
        // The skeleton should span most of the width
        REQUIRE(row_counts[middle_row] >= size.width * 0.8);
        
        // Edge rows should have fewer or no pixels
        REQUIRE(row_counts[0] <= row_counts[middle_row]);
        REQUIRE(row_counts[size.height - 1] <= row_counts[middle_row]);
    }
    
    SECTION("consistency between vector and Image interfaces") {
        // Create the same test image using both interfaces
        ImageSize size = {10, 8}; // 10 width, 8 height
        
        // Create pattern using Image interface
        Image img_input(size);
        for (int row = 2; row < 6; ++row) {
            for (int col = 3; col < 7; ++col) {
                img_input.set(row, col, 255);
            }
        }
        
        // Create same pattern using vector interface
        std::vector<uint8_t> vec_input(size.width * size.height, 0);
        for (int row = 2; row < 6; ++row) {
            for (int col = 3; col < 7; ++col) {
                vec_input[row * size.width + col] = 255;
            }
        }
        
        // Run skeletonization with both interfaces
        Image img_result = fast_skeletonize(img_input);
        std::vector<uint8_t> vec_result = fast_skeletonize(vec_input, size.height, size.width);
        
        // Results should be identical
        REQUIRE(img_result.data == vec_result);
        REQUIRE(img_result.size.width == size.width);
        REQUIRE(img_result.size.height == size.height);
    }
} 