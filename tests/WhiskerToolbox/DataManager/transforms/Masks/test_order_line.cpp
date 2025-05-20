#include "catch2/benchmark/catch_benchmark.hpp"
#include "catch2/catch_test_macros.hpp"

#include "DataManager/transforms/Masks/order_line.hpp"
#include "DataManager/transforms/Masks/order_line_optimized.hpp"
#include "ImageSize/ImageSize.hpp"
#include "Points/points.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

TEST_CASE("Benchmark order_line algorithms", "[benchmark]") {
    // Define different image sizes for benchmarking
    std::vector<std::pair<int, int>> imageSizes = {
        {100, 100},    // Small image
        {500, 500},    // Medium image
        {1000, 1000},  // Large image
    };
    
    // Define different point densities (percentage of pixels that are part of the line)
    std::vector<double> densities = {0.01, 0.05, 0.1};  // 1%, 5%, 10%
    
    // Define different subsample rates
    std::vector<int> subsampleRates = {1, 2, 5, 10};

    // Random number generator for consistent results
    std::mt19937 rng(42);
    
    for (const auto& [width, height] : imageSizes) {
        ImageSize imageSize{width, height};
        size_t totalPixels = width * height;
        
        for (double density : densities) {
            // Calculate number of line pixels based on density
            size_t linePixels = static_cast<size_t>(totalPixels * density);
            
            // Create a binary image with random line pixels
            std::vector<uint8_t> binaryImage(totalPixels, 0);
            std::vector<size_t> indices(totalPixels);
            std::iota(indices.begin(), indices.end(), 0);
            std::shuffle(indices.begin(), indices.end(), rng);
            
            for (size_t i = 0; i < linePixels; ++i) {
                binaryImage[indices[i]] = 1;
            }
            
            // Define a reference point in the center of the image
            Point2D<float> origin{static_cast<float>(width / 2), static_cast<float>(height / 2)};
            
            // Define different tolerance levels
            std::vector<float> tolerances = {5.0f, 10.0f, 20.0f};
            
            for (int subsample : subsampleRates) {
                for (float tolerance : tolerances) {
                    std::string benchName = "order_line " + 
                        std::to_string(width) + "x" + std::to_string(height) + 
                        ", " + std::to_string(linePixels) + " points" +
                        ", subsample=" + std::to_string(subsample) +
                        ", tolerance=" + std::to_string(tolerance);
                    
                    BENCHMARK(benchName + " (original)") {
                        return order_line(binaryImage, imageSize, origin, subsample, tolerance);
                    };
                    
                    BENCHMARK(benchName + " (optimized)") {
                        return order_line_optimized(binaryImage, imageSize, origin, subsample, tolerance);
                    };
                }
            }
        }
    }
}

// More targeted benchmarks to isolate specific parts of the algorithm
TEST_CASE("Benchmark order_line with worst-case scenario", "[benchmark]") {
    // Create a worst-case scenario where points are arranged in a way that
    // makes nearest-neighbor search inefficient
    
    // Create a spiral pattern which is challenging for nearest-neighbor searches
    // as each point's nearest neighbor can be far away in memory
    const int width = 500;
    const int height = 500;
    ImageSize imageSize{width, height};
    
    std::vector<uint8_t> binaryImage(width * height, 0);
    
    // Create spiral points
    const int centerX = width / 2;
    const int centerY = height / 2;
    const int numPoints = 5000;  // Number of points in the spiral
    
    for (int i = 0; i < numPoints; ++i) {
        double angle = 0.1 * i;
        double radius = 2.0 * angle;
        int x = centerX + static_cast<int>(radius * std::cos(angle));
        int y = centerY + static_cast<int>(radius * std::sin(angle));
        
        // Ensure the point is within image bounds
        if (x >= 0 && x < width && y >= 0 && y < height) {
            binaryImage[y * width + x] = 1;
        }
    }
    
    Point2D<float> center{static_cast<float>(centerX), static_cast<float>(centerY)};
    Point2D<float> edge{0.0f, 0.0f};  // Starting from corner is worse for spiral
    
    BENCHMARK("Spiral pattern - start from center (original)") {
        return order_line(binaryImage, imageSize, center, 1, 10.0f);
    };
    
    BENCHMARK("Spiral pattern - start from center (optimized)") {
        return order_line_optimized(binaryImage, imageSize, center, 1, 10.0f);
    };
    
    BENCHMARK("Spiral pattern - start from edge (original)") {
        return order_line(binaryImage, imageSize, edge, 1, 10.0f);
    };
    
    BENCHMARK("Spiral pattern - start from edge (optimized)") {
        return order_line_optimized(binaryImage, imageSize, edge, 1, 10.0f);
    };
}

// Test the impact of different data structures and algorithms
TEST_CASE("Benchmark with different point distributions", "[benchmark]") {
    const int width = 500;
    const int height = 500;
    ImageSize imageSize{width, height};
    
    // Create different point distributions
    
    // 1. Uniform random distribution
    std::vector<uint8_t> randomImage(width * height, 0);
    {
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> xDist(0, width - 1);
        std::uniform_int_distribution<int> yDist(0, height - 1);
        
        const int numPoints = 2000;
        for (int i = 0; i < numPoints; ++i) {
            int x = xDist(rng);
            int y = yDist(rng);
            randomImage[y * width + x] = 1;
        }
    }
    
    // 2. Clustered distribution (multiple small clusters)
    std::vector<uint8_t> clusteredImage(width * height, 0);
    {
        std::mt19937 rng(42);
        const int numClusters = 10;
        const int pointsPerCluster = 200;
        
        for (int c = 0; c < numClusters; ++c) {
            // Generate random cluster center
            int centerX = rng() % width;
            int centerY = rng() % height;
            
            // Generate points around the center with Gaussian distribution
            std::normal_distribution<float> dist(0.0f, 30.0f);
            
            for (int i = 0; i < pointsPerCluster; ++i) {
                int x = static_cast<int>(centerX + dist(rng));
                int y = static_cast<int>(centerY + dist(rng));
                
                // Ensure point is within bounds
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    clusteredImage[y * width + x] = 1;
                }
            }
        }
    }
    
    // 3. Line-like structure (e.g., simulating a whisker)
    std::vector<uint8_t> lineImage(width * height, 0);
    {
        // Create a curved line
        const int numPoints = 2000;
        const float centerX = width / 2.0f;
        const float centerY = height / 2.0f;
        
        for (int i = 0; i < numPoints; ++i) {
            float t = static_cast<float>(i) / numPoints;
            // Parametric equation for a curved line
            int x = static_cast<int>(centerX + 200.0f * std::sin(t * 3.14159f));
            int y = static_cast<int>(centerY + 200.0f * t - 100.0f);
            
            // Ensure point is within bounds
            if (x >= 0 && x < width && y >= 0 && y < height) {
                lineImage[y * width + x] = 1;
                
                // Add some noise around the line (thickness)
                for (int dx = -1; dx <= 1; ++dx) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                            lineImage[ny * width + nx] = 1;
                        }
                    }
                }
            }
        }
    }
    
    Point2D<float> origin{0.0f, 0.0f};  // Start from corner
    
    BENCHMARK("Random distribution (original)") {
        return order_line(randomImage, imageSize, origin, 1, 10.0f);
    };
    
    BENCHMARK("Random distribution (optimized)") {
        return order_line_optimized(randomImage, imageSize, origin, 1, 10.0f);
    };
    
    BENCHMARK("Clustered distribution (original)") {
        return order_line(clusteredImage, imageSize, origin, 1, 10.0f);
    };
    
    BENCHMARK("Clustered distribution (optimized)") {
        return order_line_optimized(clusteredImage, imageSize, origin, 1, 10.0f);
    };
    
    BENCHMARK("Line-like structure (original)") {
        return order_line(lineImage, imageSize, origin, 1, 10.0f);
    };
    
    BENCHMARK("Line-like structure (optimized)") {
        return order_line_optimized(lineImage, imageSize, origin, 1, 10.0f);
    };
}

// Correctness test to ensure the optimized version produces equivalent results
TEST_CASE("Test order_line correctness", "[correctness]") {
    // Create a simple test pattern
    const int width = 100;
    const int height = 100;
    ImageSize imageSize{width, height};
    
    std::vector<uint8_t> binaryImage(width * height, 0);
    
    // Create a simple curved pattern
    for (int i = 0; i < 50; ++i) {
        int x = 50 + static_cast<int>(20.0 * std::sin(i * 0.1));
        int y = 20 + i;
        if (x >= 0 && x < width && y >= 0 && y < height) {
            binaryImage[y * width + x] = 1;
        }
    }
    
    Point2D<float> origin{50.0f, 20.0f};  // Start from the beginning of the curve
    
    // Get results from both implementations
    auto originalResult = order_line(binaryImage, imageSize, origin, 1, 5.0f);
    auto optimizedResult = order_line_optimized(binaryImage, imageSize, origin, 1, 5.0f);
    
    // Check that both implementations return the same number of points
    REQUIRE(originalResult.size() == optimizedResult.size());
    
    // Check that the first point is the same (should be closest to origin)
    if (!originalResult.empty() && !optimizedResult.empty()) {
        REQUIRE(originalResult[0].x == Approx(optimizedResult[0].x));
        REQUIRE(originalResult[0].y == Approx(optimizedResult[0].y));
    }
    
    // Check overall ordering - the tolerance for different ordering is relatively relaxed
    // since nearest neighbor paths can slightly differ but still be equally valid
    if (originalResult.size() > 1) {
        // Check the total path length as a measure of overall quality
        float originalPathLength = 0.0f;
        float optimizedPathLength = 0.0f;
        
        for (size_t i = 1; i < originalResult.size(); ++i) {
            originalPathLength += std::sqrt(
                std::pow(originalResult[i].x - originalResult[i-1].x, 2) + 
                std::pow(originalResult[i].y - originalResult[i-1].y, 2));
        }
        
        for (size_t i = 1; i < optimizedResult.size(); ++i) {
            optimizedPathLength += std::sqrt(
                std::pow(optimizedResult[i].x - optimizedResult[i-1].x, 2) + 
                std::pow(optimizedResult[i].y - optimizedResult[i-1].y, 2));
        }
        
        // The optimized path should be equal or shorter than the original
        // Allow a small tolerance for floating point differences
        REQUIRE(optimizedPathLength <= originalPathLength * 1.05f);
    }
} 