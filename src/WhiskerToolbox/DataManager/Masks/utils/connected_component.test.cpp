#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "connected_component.hpp"

#include <vector>
#include <algorithm>
#include <numeric>

TEST_CASE("remove_small_clusters happy path tests", "[connected_component][morphology]") {
    
    SECTION("removes small clusters while preserving large ones") {
        // Create a 10x10 image with different sized clusters
        ImageSize const image_size = {10, 10};
        std::vector<uint8_t> image(100, 0);
        
        // Large cluster: 3x3 square (9 pixels) at top-left
        for (int row = 1; row < 4; ++row) {
            for (int col = 1; col < 4; ++col) {
                image[row * 10 + col] = 255;
            }
        }
        
        // Small cluster: 2x1 rectangle (2 pixels) at top-right
        image[1 * 10 + 7] = 255;
        image[1 * 10 + 8] = 255;
        
        // Medium cluster: 2x2 square (4 pixels) at bottom-left
        for (int row = 7; row < 9; ++row) {
            for (int col = 1; col < 3; ++col) {
                image[row * 10 + col] = 255;
            }
        }
        
        // Very small cluster: single pixel at bottom-right
        image[8 * 10 + 8] = 255;
        
        // Remove clusters smaller than 4 pixels
        std::vector<uint8_t> result = remove_small_clusters(image, image_size, 4);
        
        REQUIRE(result.size() == 100);
        
        // Count pixels in each expected region
        int large_cluster_pixels = 0;
        for (int row = 1; row < 4; ++row) {
            for (int col = 1; col < 4; ++col) {
                if (result[row * 10 + col] > 0) {
                    large_cluster_pixels++;
                }
            }
        }
        
        int medium_cluster_pixels = 0;
        for (int row = 7; row < 9; ++row) {
            for (int col = 1; col < 3; ++col) {
                if (result[row * 10 + col] > 0) {
                    medium_cluster_pixels++;
                }
            }
        }
        
        // Large cluster (9 pixels) should be preserved
        REQUIRE(large_cluster_pixels == 9);
        
        // Medium cluster (4 pixels) should be preserved (exactly at threshold)
        REQUIRE(medium_cluster_pixels == 4);
        
        // Small clusters should be removed
        REQUIRE(result[1 * 10 + 7] == 0); // Small 2-pixel cluster
        REQUIRE(result[1 * 10 + 8] == 0);
        REQUIRE(result[8 * 10 + 8] == 0); // Single pixel cluster
        
        // Total preserved pixels should be 9 + 4 = 13
        int total_pixels = std::accumulate(result.begin(), result.end(), 0);
        REQUIRE(total_pixels == 13);
    }
    
    SECTION("preserves all clusters when threshold is 1") {
        // Create a 5x5 image with various small clusters
        ImageSize const image_size = {5, 5};
        std::vector<uint8_t> image(25, 0);
        
        // Single pixels scattered around
        image[0 * 5 + 0] = 255; // Top-left corner
        image[2 * 5 + 2] = 255; // Center
        image[4 * 5 + 4] = 255; // Bottom-right corner
        
        // 2-pixel cluster
        image[0 * 5 + 3] = 255;
        image[0 * 5 + 4] = 255;
        
        std::vector<uint8_t> result = remove_small_clusters(image, image_size, 1);
        
        REQUIRE(result.size() == 25);
        
        // All original pixels should be preserved
        REQUIRE(result[0 * 5 + 0] == 1);
        REQUIRE(result[2 * 5 + 2] == 1);
        REQUIRE(result[4 * 5 + 4] == 1);
        REQUIRE(result[0 * 5 + 3] == 1);
        REQUIRE(result[0 * 5 + 4] == 1);
        
        // Total should be 5 pixels
        int total_pixels = std::accumulate(result.begin(), result.end(), 0);
        REQUIRE(total_pixels == 5);
    }
    
    SECTION("handles L-shaped and complex cluster shapes") {
        // Create a 6x6 image with an L-shaped cluster
        ImageSize const image_size = {6, 6};
        std::vector<uint8_t> image(36, 0);
        
        // L-shaped cluster (7 pixels total):
        // XX....
        // X...O.
        // X...O.
        // XXX...
        // ......
        // ......
        
        image[0 * 6 + 0] = 255; image[0 * 6 + 1] = 255; // Top horizontal part
        image[1 * 6 + 0] = 255; // Vertical part
        image[2 * 6 + 0] = 255; // Vertical part  
        image[3 * 6 + 0] = 255; image[3 * 6 + 1] = 255; // Bottom horizontal part
        image[3 * 6 + 2] = 255;
        
        // Small separate cluster (2 pixels)
        image[1 * 6 + 4] = 255;
        image[2 * 6 + 4] = 255;
        
        std::vector<uint8_t> result = remove_small_clusters(image, image_size, 5);
        
        REQUIRE(result.size() == 36);
        
        // L-shaped cluster (7 pixels) should be preserved
        REQUIRE(result[0 * 6 + 0] == 1);
        REQUIRE(result[0 * 6 + 1] == 1);
        REQUIRE(result[1 * 6 + 0] == 1);
        REQUIRE(result[2 * 6 + 0] == 1);
        REQUIRE(result[3 * 6 + 0] == 1);
        REQUIRE(result[3 * 6 + 1] == 1);
        REQUIRE(result[3 * 6 + 2] == 1);
        
        // Small cluster should be removed
        REQUIRE(result[1 * 6 + 4] == 0);
        REQUIRE(result[2 * 6 + 4] == 0);
        
        int total_pixels = std::accumulate(result.begin(), result.end(), 0);
        REQUIRE(total_pixels == 7);
    }
}

TEST_CASE("remove_small_clusters edge cases and error handling", "[connected_component][morphology][edge-cases]") {
    
    SECTION("empty image remains empty") {
        ImageSize const image_size = {10, 10};
        std::vector<uint8_t> image(100, 0);
        
        std::vector<uint8_t> result = remove_small_clusters(image, image_size, 5);
        
        REQUIRE(result.size() == 100);
        REQUIRE(std::all_of(result.begin(), result.end(), [](uint8_t pixel) { return pixel == 0; }));
    }
    
    SECTION("all clusters too small - results in empty image") {
        ImageSize const image_size = {7, 7};
        std::vector<uint8_t> image(49, 0);
        
        // Create several small clusters (1-3 pixels each)
        image[1 * 7 + 1] = 255; // Single pixel
        
        image[3 * 7 + 3] = 255; image[3 * 7 + 4] = 255; // 2-pixel cluster
        
        image[5 * 7 + 1] = 255; image[5 * 7 + 2] = 255; // 3-pixel L-shape
        image[6 * 7 + 1] = 255;
        
        // Remove clusters smaller than 4 pixels
        std::vector<uint8_t> result = remove_small_clusters(image, image_size, 4);
        
        REQUIRE(result.size() == 49);
        // All pixels should be removed
        REQUIRE(std::all_of(result.begin(), result.end(), [](uint8_t pixel) { return pixel == 0; }));
    }
    
    SECTION("single large cluster fills most of image") {
        ImageSize const image_size = {5, 5};
        std::vector<uint8_t> image(25, 0);
        
        // Fill most of the image (leaving borders empty)
        for (int row = 1; row < 4; ++row) {
            for (int col = 1; col < 4; ++col) {
                image[row * 5 + col] = 255;
            }
        }
        // This creates a 3x3 = 9 pixel cluster
        
        std::vector<uint8_t> result = remove_small_clusters(image, image_size, 5);
        
        REQUIRE(result.size() == 25);
        
        // The large cluster should be preserved
        for (int row = 1; row < 4; ++row) {
            for (int col = 1; col < 4; ++col) {
                REQUIRE(result[row * 5 + col] == 1);
            }
        }
        
        int total_pixels = std::accumulate(result.begin(), result.end(), 0);
        REQUIRE(total_pixels == 9);
    }
    
    SECTION("minimal image dimensions") {
        // Test with 1x1 image
        ImageSize const image_size = {1, 1};
        std::vector<uint8_t> image = {255};
        
        std::vector<uint8_t> result = remove_small_clusters(image, image_size, 1);
        
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == 1); // Single pixel should be preserved if threshold is 1
        
        // Test with threshold larger than possible cluster
        result = remove_small_clusters(image, image_size, 2);
        REQUIRE(result[0] == 0); // Should be removed
    }
    
    SECTION("diagonal connectivity test") {
        // Test 8-connectivity (diagonals count as connected)
        ImageSize const image_size = {3, 3};
        std::vector<uint8_t> image(9, 0);
        
        // Create a diagonal line:
        // X..
        // .X.
        // ..X
        image[0 * 3 + 0] = 255;
        image[1 * 3 + 1] = 255;
        image[2 * 3 + 2] = 255;
        
        std::vector<uint8_t> result = remove_small_clusters(image, image_size, 3);
        
        REQUIRE(result.size() == 9);
        
        // All three pixels should be preserved as one connected component
        REQUIRE(result[0 * 3 + 0] == 1);
        REQUIRE(result[1 * 3 + 1] == 1);
        REQUIRE(result[2 * 3 + 2] == 1);
        
        int total_pixels = std::accumulate(result.begin(), result.end(), 0);
        REQUIRE(total_pixels == 3);
    }
    
    SECTION("high threshold removes everything") {
        ImageSize const image_size = {4, 4};
        std::vector<uint8_t> image(16, 255); // Fill entire image
        
        // Remove clusters smaller than 20 pixels (but image only has 16)
        std::vector<uint8_t> result = remove_small_clusters(image, image_size, 20);
        
        REQUIRE(result.size() == 16);
        REQUIRE(std::all_of(result.begin(), result.end(), [](uint8_t pixel) { return pixel == 0; }));
    }
} 