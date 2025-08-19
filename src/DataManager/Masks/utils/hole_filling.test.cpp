#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "hole_filling.hpp"
#include "CoreGeometry/Image.hpp"

#include <algorithm>
#include <numeric>
#include <vector>

TEST_CASE("fill_holes basic functionality", "[hole_filling][morphology]") {
    
    SECTION("fills simple rectangular hole") {
        // Create a 7x7 image with a rectangular object containing a hole
        ImageSize const image_size = {7, 7};
        std::vector<uint8_t> image(49, 0);
        
        // Create outer rectangle (border)
        for (int row = 1; row < 6; ++row) {
            for (int col = 1; col < 6; ++col) {
                if (row == 1 || row == 5 || col == 1 || col == 5) {
                    image[row * 7 + col] = 255;
                }
            }
        }
        
        // This creates a hollow rectangle with hole in the middle
        // Pattern:
        // 0000000
        // 0XXXXX0
        // 0X000X0
        // 0X000X0
        // 0X000X0
        // 0XXXXX0
        // 0000000
        
        std::vector<uint8_t> result = fill_holes(image, image_size);
        
        REQUIRE(result.size() == 49);
        
        // The hole should now be filled
        for (int row = 2; row < 5; ++row) {
            for (int col = 2; col < 5; ++col) {
                REQUIRE(result[row * 7 + col] == 1);
            }
        }
        
        // The outer border should remain filled
        for (int row = 1; row < 6; ++row) {
            for (int col = 1; col < 6; ++col) {
                if (row == 1 || row == 5 || col == 1 || col == 5) {
                    REQUIRE(result[row * 7 + col] == 1);
                }
            }
        }
        
        // The image boundary should remain background
        // Top and bottom rows
        for (int col = 0; col < 7; ++col) {
            REQUIRE(result[0 * 7 + col] == 0);
            REQUIRE(result[6 * 7 + col] == 0);
        }
        // Left and right columns
        for (int row = 0; row < 7; ++row) {
            REQUIRE(result[row * 7 + 0] == 0);
            REQUIRE(result[row * 7 + 6] == 0);
        }
    }
    
    SECTION("preserves background connected to image boundary") {
        // Create a 5x5 image with an object that doesn't touch the boundary
        ImageSize const image_size = {5, 5};
        std::vector<uint8_t> image(25, 0);
        
        // Create a 3x3 solid square in the center
        for (int row = 1; row < 4; ++row) {
            for (int col = 1; col < 4; ++col) {
                image[row * 5 + col] = 255;
            }
        }
        
        std::vector<uint8_t> result = fill_holes(image, image_size);
        
        REQUIRE(result.size() == 25);
        
        // The solid square should remain unchanged
        for (int row = 1; row < 4; ++row) {
            for (int col = 1; col < 4; ++col) {
                REQUIRE(result[row * 5 + col] == 1);
            }
        }
        
        // Background should remain unchanged (no holes to fill)
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (row == 0 || row == 4 || col == 0 || col == 4) {
                    REQUIRE(result[row * 5 + col] == 0);
                }
            }
        }
    }
    
    SECTION("handles complex shapes with multiple holes") {
        // Create a 9x9 image with a complex shape containing multiple holes
        ImageSize const image_size = {9, 9};
        std::vector<uint8_t> image(81, 0);
        
        // Create a large rectangular border
        for (int row = 1; row < 8; ++row) {
            for (int col = 1; col < 8; ++col) {
                if (row == 1 || row == 7 || col == 1 || col == 7) {
                    image[row * 9 + col] = 255;
                }
            }
        }
        
        // Add some internal structure to create multiple holes
        // Vertical divider
        for (int row = 2; row < 7; ++row) {
            image[row * 9 + 4] = 255;
        }
        // Horizontal divider  
        for (int col = 2; col < 7; ++col) {
            image[4 * 9 + col] = 255;
        }
        
        // This creates 4 separate holes in the 4 quadrants
        
        std::vector<uint8_t> result = fill_holes(image, image_size);
        
        REQUIRE(result.size() == 81);
        
        // All holes should be filled
        for (int row = 2; row < 7; ++row) {
            for (int col = 2; col < 7; ++col) {
                REQUIRE(result[row * 9 + col] == 1);
            }
        }
        
        // Boundary should remain background
        for (int col = 0; col < 9; ++col) {
            REQUIRE(result[0 * 9 + col] == 0);
            REQUIRE(result[8 * 9 + col] == 0);
        }
        for (int row = 0; row < 9; ++row) {
            REQUIRE(result[row * 9 + 0] == 0);
            REQUIRE(result[row * 9 + 8] == 0);
        }
    }
    
    SECTION("handles C-shaped object (no holes to fill)") {
        // Create a C-shaped object that's open to the boundary
        ImageSize const image_size = {6, 6};
        std::vector<uint8_t> image(36, 0);
        
        // C-shape: top, left, and bottom borders only
        for (int col = 1; col < 5; ++col) {
            image[1 * 6 + col] = 255;  // Top
            image[4 * 6 + col] = 255;  // Bottom
        }
        for (int row = 1; row < 5; ++row) {
            image[row * 6 + 1] = 255;  // Left side
        }
        
        std::vector<uint8_t> result = fill_holes(image, image_size);
        
        REQUIRE(result.size() == 36);
        
        // No holes should be filled since the interior is connected to boundary
        // Compare normalized values (0/1) instead of raw image values (0/255)
        std::vector<uint8_t> expected_normalized(36);
        for (size_t i = 0; i < 36; ++i) {
            expected_normalized[i] = (image[i] > 0) ? 1 : 0;
        }
        REQUIRE(result == expected_normalized);
    }
}

TEST_CASE("fill_holes edge cases and error handling", "[hole_filling][morphology][edge-cases]") {
    
    SECTION("empty image remains empty") {
        ImageSize const image_size = {5, 5};
        std::vector<uint8_t> image(25, 0);
        
        std::vector<uint8_t> result = fill_holes(image, image_size);
        
        REQUIRE(result.size() == 25);
        REQUIRE(std::all_of(result.begin(), result.end(), [](uint8_t pixel) { return pixel == 0; }));
    }
    
    SECTION("completely filled image remains filled") {
        ImageSize const image_size = {4, 4};
        std::vector<uint8_t> image(16, 255);
        
        std::vector<uint8_t> result = fill_holes(image, image_size);
        
        REQUIRE(result.size() == 16);
        REQUIRE(std::all_of(result.begin(), result.end(), [](uint8_t pixel) { return pixel == 1; }));
    }
    
    SECTION("single pixel object") {
        ImageSize const image_size = {3, 3};
        std::vector<uint8_t> image(9, 0);
        image[1 * 3 + 1] = 255;  // Center pixel
        
        std::vector<uint8_t> result = fill_holes(image, image_size);
        
        REQUIRE(result.size() == 9);
        REQUIRE(result[1 * 3 + 1] == 1);  // Center should remain
        
        // All other pixels should remain background
        for (int i = 0; i < 9; ++i) {
            if (i != 4) {  // Skip center pixel
                REQUIRE(result[i] == 0);
            }
        }
    }
    
    SECTION("handles invalid dimensions") {
        ImageSize const image_size = {0, 0};
        std::vector<uint8_t> image;
        
        std::vector<uint8_t> result = fill_holes(image, image_size);
        REQUIRE(result.empty());
    }
    
    SECTION("minimal image dimensions") {
        // Test with 1x1 image
        ImageSize const image_size = {1, 1};
        std::vector<uint8_t> image = {255};
        
        std::vector<uint8_t> result = fill_holes(image, image_size);
        
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == 1);
        
        // Test with 1x1 background
        image = {0};
        result = fill_holes(image, image_size);
        REQUIRE(result[0] == 0);
    }
    
    SECTION("object touching image boundary") {
        // Create an object that touches the image boundary
        ImageSize const image_size = {4, 4};
        std::vector<uint8_t> image(16, 0);
        
        // L-shaped object touching top and left boundaries
        image[0 * 4 + 0] = 255;  image[0 * 4 + 1] = 255;  // Top row
        image[1 * 4 + 0] = 255;  // Left column
        image[2 * 4 + 0] = 255;  // Left column
        
        std::vector<uint8_t> result = fill_holes(image, image_size);
        
        // Should remain unchanged since no holes exist
        for (int i = 0; i < 16; ++i) {
            REQUIRE(result[i] == ((image[i] > 0) ? 1 : 0));
        }
    }
}

TEST_CASE("fill_holes Image interface tests", "[hole_filling][morphology][Image]") {
    
    SECTION("fills holes using Image interface") {
        // Create a 6x6 image with a donut shape (outer ring with hole in center)
        ImageSize size = {6, 6}; // width, height
        Image input_image(size);
        
        // Create outer border
        for (int row = 1; row < 5; ++row) {
            for (int col = 1; col < 5; ++col) {
                if (row == 1 || row == 4 || col == 1 || col == 4) {
                    input_image.set(row, col, 255);
                }
            }
        }
        
        // This creates a hollow square (donut shape)
        
        Image result = fill_holes(input_image);
        
        REQUIRE(result.size.width == size.width);
        REQUIRE(result.size.height == size.height);
        REQUIRE(result.pixel_count() == input_image.pixel_count());
        
        // The hole in the center should be filled
        for (int row = 2; row < 4; ++row) {
            for (int col = 2; col < 4; ++col) {
                REQUIRE(result.at(row, col) == 1);
            }
        }
        
        // The border should remain filled
        for (int row = 1; row < 5; ++row) {
            for (int col = 1; col < 5; ++col) {
                if (row == 1 || row == 4 || col == 1 || col == 4) {
                    REQUIRE(result.at(row, col) == 1);
                }
            }
        }
        
        // The image boundary should remain background
        for (int col = 0; col < 6; ++col) {
            REQUIRE(result.at(0, col) == 0);
            REQUIRE(result.at(5, col) == 0);
        }
        for (int row = 0; row < 6; ++row) {
            REQUIRE(result.at(row, 0) == 0);
            REQUIRE(result.at(row, 5) == 0);
        }
    }
    
    SECTION("consistency between vector and Image interfaces") {
        // Create the same test image using both interfaces
        ImageSize size = {5, 5}; // 5 width, 5 height
        
        // Create pattern using Image interface - ring shape
        Image img_input(size);
        for (int row = 1; row < 4; ++row) {
            for (int col = 1; col < 4; ++col) {
                if (row == 1 || row == 3 || col == 1 || col == 3) {
                    img_input.set(row, col, 255);
                }
            }
        }
        
        // Create same pattern using vector interface
        std::vector<uint8_t> vec_input(size.width * size.height, 0);
        for (int row = 1; row < 4; ++row) {
            for (int col = 1; col < 4; ++col) {
                if (row == 1 || row == 3 || col == 1 || col == 3) {
                    vec_input[row * size.width + col] = 255;
                }
            }
        }
        
        // Run hole filling with both interfaces
        Image img_result = fill_holes(img_input);
        std::vector<uint8_t> vec_result = fill_holes(vec_input, size);
        
        // Results should be identical
        REQUIRE(img_result.data == vec_result);
        REQUIRE(img_result.size.width == size.width);
        REQUIRE(img_result.size.height == size.height);
        
        // Verify that the hole is filled
        REQUIRE(img_result.at(2, 2) == 1); // Center should be filled
    }
} 