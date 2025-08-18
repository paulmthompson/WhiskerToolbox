#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Image.hpp"

#include <vector>
#include <algorithm>
#include <numeric>

TEST_CASE("Image struct construction and basic operations", "[Image][construction]") {
    
    SECTION("default constructor creates empty image") {
        Image img;
        
        REQUIRE(img.empty());
        REQUIRE(img.data.empty());
        REQUIRE(img.size.width == -1);
        REQUIRE(img.size.height == -1);
        REQUIRE(img.pixel_count() == 0);
    }
    
    SECTION("constructor with data and size") {
        std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6};
        ImageSize size = {3, 2}; // 3 width, 2 height
        
        Image img(data, size);
        
        REQUIRE_FALSE(img.empty());
        REQUIRE(img.data.size() == 6);
        REQUIRE(img.size.width == 3);
        REQUIRE(img.size.height == 2);
        REQUIRE(img.pixel_count() == 6);
        
        // Verify data was copied correctly
        REQUIRE(img.data[0] == 1);
        REQUIRE(img.data[5] == 6);
    }
    
    SECTION("constructor with move semantics") {
        std::vector<uint8_t> data = {10, 20, 30, 40};
        ImageSize size = {2, 2};
        
        Image img(std::move(data), size);
        
        REQUIRE_FALSE(img.empty());
        REQUIRE(img.data.size() == 4);
        REQUIRE(img.pixel_count() == 4);
        REQUIRE(img.data[0] == 10);
        REQUIRE(img.data[3] == 40);
        
        // Original data should be moved (empty)
        REQUIRE(data.empty());
    }
    
    SECTION("constructor with size only (zero-initialized)") {
        ImageSize size = {4, 3}; // 4 width, 3 height
        
        Image img(size);
        
        REQUIRE_FALSE(img.empty());
        REQUIRE(img.data.size() == 12);
        REQUIRE(img.pixel_count() == 12);
        REQUIRE(img.size.width == 4);
        REQUIRE(img.size.height == 3);
        
        // All pixels should be zero-initialized
        REQUIRE(std::all_of(img.data.begin(), img.data.end(), [](uint8_t pixel) { return pixel == 0; }));
    }
}

TEST_CASE("Image pixel access with row-major layout", "[Image][access][row-major]") {
    
    SECTION("at() method with row-major indexing") {
        // Create a 3x2 image (3 width, 2 height)
        // Layout should be: [row0_col0, row0_col1, row0_col2, row1_col0, row1_col1, row1_col2]
        std::vector<uint8_t> data = {10, 20, 30, 40, 50, 60};
        ImageSize size = {3, 2};
        Image img(data, size);
        
        // Test row-major access: row * width + col
        REQUIRE(img.at(0, 0) == 10); // data[0 * 3 + 0] = data[0]
        REQUIRE(img.at(0, 1) == 20); // data[0 * 3 + 1] = data[1]
        REQUIRE(img.at(0, 2) == 30); // data[0 * 3 + 2] = data[2]
        REQUIRE(img.at(1, 0) == 40); // data[1 * 3 + 0] = data[3]
        REQUIRE(img.at(1, 1) == 50); // data[1 * 3 + 1] = data[4]
        REQUIRE(img.at(1, 2) == 60); // data[1 * 3 + 2] = data[5]
    }
    
    SECTION("set() method with row-major indexing") {
        ImageSize size = {2, 3}; // 2 width, 3 height
        Image img(size); // Zero-initialized
        
        // Set specific pixels using row, col coordinates
        img.set(0, 0, 100);
        img.set(0, 1, 101);
        img.set(1, 0, 110);
        img.set(1, 1, 111);
        img.set(2, 0, 120);
        img.set(2, 1, 121);
        
        // Verify row-major layout in underlying data
        REQUIRE(img.data[0] == 100); // row=0, col=0 -> index 0
        REQUIRE(img.data[1] == 101); // row=0, col=1 -> index 1
        REQUIRE(img.data[2] == 110); // row=1, col=0 -> index 2
        REQUIRE(img.data[3] == 111); // row=1, col=1 -> index 3
        REQUIRE(img.data[4] == 120); // row=2, col=0 -> index 4
        REQUIRE(img.data[5] == 121); // row=2, col=1 -> index 5
        
        // Verify using at() method
        REQUIRE(img.at(0, 0) == 100);
        REQUIRE(img.at(0, 1) == 101);
        REQUIRE(img.at(1, 0) == 110);
        REQUIRE(img.at(1, 1) == 111);
        REQUIRE(img.at(2, 0) == 120);
        REQUIRE(img.at(2, 1) == 121);
    }
    
    SECTION("consistency with manual row-major indexing") {
        ImageSize size = {5, 4}; // 5 width, 4 height
        Image img(size);
        
        // Fill image with values that encode their position
        for (int row = 0; row < size.height; ++row) {
            for (int col = 0; col < size.width; ++col) {
                uint8_t value = static_cast<uint8_t>(row * 10 + col);
                img.set(row, col, value);
            }
        }
        
        // Verify that direct data access matches at() method
        for (int row = 0; row < size.height; ++row) {
            for (int col = 0; col < size.width; ++col) {
                int manual_index = row * size.width + col;
                uint8_t expected_value = static_cast<uint8_t>(row * 10 + col);
                
                REQUIRE(img.data[manual_index] == expected_value);
                REQUIRE(img.at(row, col) == expected_value);
                REQUIRE(img.data[manual_index] == img.at(row, col));
            }
        }
    }
}

TEST_CASE("Image utility methods", "[Image][utilities]") {
    
    SECTION("empty() method") {
        // Empty image
        Image empty_img;
        REQUIRE(empty_img.empty());
        
        // Image with zero width
        ImageSize zero_width = {0, 5};
        Image zero_width_img(zero_width);
        REQUIRE(zero_width_img.empty());
        
        // Image with zero height
        ImageSize zero_height = {5, 0};
        Image zero_height_img(zero_height);
        REQUIRE(zero_height_img.empty());
        
        // Image with negative dimensions
        ImageSize negative = {-1, -1};
        Image negative_img(std::vector<uint8_t>{}, negative);
        REQUIRE(negative_img.empty());
        
        // Valid non-empty image
        ImageSize valid = {3, 3};
        Image valid_img(valid);
        REQUIRE_FALSE(valid_img.empty());
    }
    
    SECTION("pixel_count() method") {
        ImageSize size1 = {1, 1};
        Image img1(size1);
        REQUIRE(img1.pixel_count() == 1);
        
        ImageSize size2 = {4, 3};
        Image img2(size2);
        REQUIRE(img2.pixel_count() == 12);
        
        ImageSize size3 = {10, 20};
        Image img3(size3);
        REQUIRE(img3.pixel_count() == 200);
        
        // Edge case: empty image
        Image empty_img;
        REQUIRE(empty_img.pixel_count() == 0);
    }
}

TEST_CASE("Image data layout verification for integration", "[Image][integration][row-major]") {
    
    SECTION("verify compatibility with existing algorithms") {
        // Create a test pattern that would be obvious if row/column ordering is wrong
        ImageSize size = {4, 3}; // 4 width, 3 height
        Image img(size);
        
        // Create a pattern where row=0 has values 1,2,3,4
        //                      row=1 has values 5,6,7,8  
        //                      row=2 has values 9,10,11,12
        for (int row = 0; row < size.height; ++row) {
            for (int col = 0; col < size.width; ++col) {
                img.set(row, col, static_cast<uint8_t>(row * size.width + col + 1));
            }
        }
        
        // Expected linear data layout (row-major): [1,2,3,4,5,6,7,8,9,10,11,12]
        std::vector<uint8_t> expected = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
        
        REQUIRE(img.data == expected);
        
        // Verify this matches what existing functions expect
        // (they use row * width + col indexing)
        for (int row = 0; row < size.height; ++row) {
            for (int col = 0; col < size.width; ++col) {
                int expected_index = row * size.width + col;
                uint8_t expected_value = static_cast<uint8_t>(expected_index + 1);
                
                REQUIRE(img.data[expected_index] == expected_value);
            }
        }
    }
    
    SECTION("binary image pattern for morphological operations") {
        // Create a binary cross pattern to test with morphological operations
        ImageSize size = {5, 5};
        Image img(size);
        
        // Create cross pattern:
        //   0 0 1 0 0
        //   0 0 1 0 0  
        //   1 1 1 1 1
        //   0 0 1 0 0
        //   0 0 1 0 0
        
        // Vertical line
        for (int row = 0; row < 5; ++row) {
            img.set(row, 2, 255);
        }
        
        // Horizontal line  
        for (int col = 0; col < 5; ++col) {
            img.set(2, col, 255);
        }
        
        // Verify center pixel
        REQUIRE(img.at(2, 2) == 255);
        
        // Verify the pattern is as expected in linear data
        // Position (2,2) should be at index 2*5+2 = 12
        REQUIRE(img.data[12] == 255);
        
        // Position (0,2) should be at index 0*5+2 = 2
        REQUIRE(img.data[2] == 255);
        
        // Position (2,0) should be at index 2*5+0 = 10
        REQUIRE(img.data[10] == 255);
        
        // Count total foreground pixels
        int foreground_count = std::count_if(img.data.begin(), img.data.end(), 
                                           [](uint8_t pixel) { return pixel > 0; });
        REQUIRE(foreground_count == 9); // 5 + 5 - 1 (center counted once)
    }
} 