#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include "QuadTree.hpp"
#include <random>
#include <set>
#include <algorithm>
#include <chrono>

using namespace Catch;

// Helper function to generate random points
std::vector<QuadTreePoint<int>> generateRandomPoints(int count, float min_x, float min_y, float max_x, float max_y) {
    std::vector<QuadTreePoint<int>> points;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis_x(min_x, max_x);
    std::uniform_real_distribution<float> dis_y(min_y, max_y);
    
    for (int i = 0; i < count; ++i) {
        points.emplace_back(dis_x(gen), dis_y(gen), i);
    }
    return points;
}

// Helper function to brute force find nearest point
template<typename T>
const QuadTreePoint<T>* bruteForceNearest(const std::vector<QuadTreePoint<T>>& points, float x, float y, float max_distance) {
    const QuadTreePoint<T>* nearest = nullptr;
    float min_distance_sq = max_distance * max_distance;
    
    for (const auto& point : points) {
        float dx = point.x - x;
        float dy = point.y - y;
        float dist_sq = dx * dx + dy * dy;
        
        if (dist_sq < min_distance_sq) {
            min_distance_sq = dist_sq;
            nearest = &point;
        }
    }
    
    return nearest;
}

// Test class for simulating canvas resize scenarios
class MockCanvas {
public:
    BoundingBox bounds;
    QuadTree<int> quadTree;
    std::vector<QuadTreePoint<int>> allPoints;
    
    MockCanvas(float width, float height) 
        : bounds(0, 0, width, height), quadTree(bounds) {}
    
    void addPoint(float x, float y, int data) {
        allPoints.emplace_back(x, y, data);
        quadTree.insert(x, y, data);
    }
    
    void resize(float new_width, float new_height) {
        // Simulate canvas resize - need to rebuild quadtree with new bounds
        BoundingBox new_bounds(0, 0, new_width, new_height);
        QuadTree<int> new_tree(new_bounds);
        
        // Re-insert all points that are still within bounds
        for (const auto& point : allPoints) {
            if (new_bounds.contains(point.x, point.y)) {
                new_tree.insert(point.x, point.y, point.data);
            }
        }
        
        // Update our state
        bounds = new_bounds;
        quadTree = std::move(new_tree);
    }
    
    void clearAndResize(float new_width, float new_height) {
        // Alternative resize method that clears everything
        bounds = BoundingBox(0, 0, new_width, new_height);
        quadTree.clear();
        quadTree = QuadTree<int>(bounds);
        allPoints.clear();
    }
};

TEST_CASE("QuadTree Basic Operations", "[quadtree][basic]") {
    BoundingBox bounds(0, 0, 100, 100);
    QuadTree<int> tree(bounds);
    
    SECTION("Empty tree") {
        REQUIRE(tree.size() == 0);
        
        std::vector<QuadTreePoint<int>> results;
        tree.query(BoundingBox(0, 0, 50, 50), results);
        REQUIRE(results.empty());
        
        auto nearest = tree.findNearest(50, 50, 10);
        REQUIRE(nearest == nullptr);
    }
    
    SECTION("Single point insertion and query") {
        REQUIRE(tree.insert(25, 25, 1));
        REQUIRE(tree.size() == 1);
        
        std::vector<QuadTreePoint<int>> results;
        tree.query(BoundingBox(0, 0, 50, 50), results);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].x == 25);
        REQUIRE(results[0].y == 25);
        REQUIRE(results[0].data == 1);
    }
    
    SECTION("Out of bounds insertion") {
        REQUIRE_FALSE(tree.insert(-10, 50, 1));
        REQUIRE_FALSE(tree.insert(110, 50, 1));
        REQUIRE_FALSE(tree.insert(50, -10, 1));
        REQUIRE_FALSE(tree.insert(50, 110, 1));
        REQUIRE(tree.size() == 0);
    }
    
    SECTION("Boundary edge cases") {
        REQUIRE(tree.insert(0, 0, 1));
        REQUIRE(tree.insert(100, 100, 2));
        REQUIRE(tree.insert(0, 100, 3));
        REQUIRE(tree.insert(100, 0, 4));
        REQUIRE(tree.size() == 4);
    }
}

TEST_CASE("QuadTree Subdivision", "[quadtree][subdivision]") {
    BoundingBox bounds(0, 0, 100, 100);
    QuadTree<int> tree(bounds);
    
    SECTION("Force subdivision by exceeding MAX_POINTS_PER_NODE") {
        // Insert more points than MAX_POINTS_PER_NODE in same quadrant
        for (int i = 0; i < 20; ++i) {
            REQUIRE(tree.insert(10 + i * 0.1f, 10 + i * 0.1f, i));
        }
        
        REQUIRE(tree.size() == 20);
        
        // All points should still be queryable
        std::vector<QuadTreePoint<int>> results;
        tree.query(BoundingBox(0, 0, 50, 50), results);
        REQUIRE(results.size() == 20);
    }
    
    SECTION("Subdivision across quadrants") {
        // Insert points in different quadrants
        tree.insert(25, 25, 1); // NW
        tree.insert(75, 25, 2); // NE
        tree.insert(25, 75, 3); // SW
        tree.insert(75, 75, 4); // SE
        
        REQUIRE(tree.size() == 4);
        
        // Query each quadrant
        std::vector<QuadTreePoint<int>> results;
        
        tree.query(BoundingBox(0, 0, 50, 50), results);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].data == 1);
        results.clear();
        
        tree.query(BoundingBox(50, 0, 100, 50), results);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].data == 2);
        results.clear();
        
        tree.query(BoundingBox(0, 50, 50, 100), results);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].data == 3);
        results.clear();
        
        tree.query(BoundingBox(50, 50, 100, 100), results);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].data == 4);
    }
}

TEST_CASE("QuadTree Nearest Neighbor Search", "[quadtree][nearest]") {
    BoundingBox bounds(0, 0, 100, 100);
    QuadTree<int> tree(bounds);
    
    SECTION("Single point nearest") {
        tree.insert(50, 50, 1);
        
        auto nearest = tree.findNearest(52, 52, 10);
        REQUIRE(nearest != nullptr);
        REQUIRE(nearest->data == 1);
        
        nearest = tree.findNearest(70, 70, 10);
        REQUIRE(nearest == nullptr);
    }
    
    SECTION("Multiple points nearest") {
        tree.insert(25, 25, 1);
        tree.insert(75, 75, 2);
        tree.insert(50, 50, 3);
        
        auto nearest = tree.findNearest(26, 26, 10);
        REQUIRE(nearest != nullptr);
        REQUIRE(nearest->data == 1);
        
        nearest = tree.findNearest(51, 51, 10);
        REQUIRE(nearest != nullptr);
        REQUIRE(nearest->data == 3);
        
        nearest = tree.findNearest(74, 74, 10);
        REQUIRE(nearest != nullptr);
        REQUIRE(nearest->data == 2);
    }
    
    SECTION("Nearest accuracy test with random points") {
        std::vector<QuadTreePoint<int>> points = generateRandomPoints(100, 10, 10, 90, 90);
        
        for (const auto& point : points) {
            tree.insert(point.x, point.y, point.data);
        }
        
        // Test multiple query points
        for (int i = 0; i < 10; ++i) {
            float query_x = 20 + i * 6;
            float query_y = 20 + i * 6;
            float max_dist = 20;
            
            auto tree_nearest = tree.findNearest(query_x, query_y, max_dist);
            auto brute_nearest = bruteForceNearest(points, query_x, query_y, max_dist);
            
            if (brute_nearest == nullptr) {
                REQUIRE(tree_nearest == nullptr);
            } else {
                REQUIRE(tree_nearest != nullptr);
                REQUIRE(tree_nearest->data == brute_nearest->data);
            }
        }
    }
}

TEST_CASE("QuadTree Range Query", "[quadtree][query]") {
    BoundingBox bounds(0, 0, 100, 100);
    QuadTree<int> tree(bounds);
    
    SECTION("Empty range query") {
        std::vector<QuadTreePoint<int>> results;
        tree.query(BoundingBox(20, 20, 30, 30), results);
        REQUIRE(results.empty());
    }
    
    SECTION("Simple range query") {
        tree.insert(25, 25, 1);
        tree.insert(75, 75, 2);
        tree.insert(25, 75, 3);
        
        std::vector<QuadTreePoint<int>> results;
        tree.query(BoundingBox(20, 20, 30, 30), results);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].data == 1);
        
        results.clear();
        tree.query(BoundingBox(20, 20, 80, 80), results);
        REQUIRE(results.size() == 3);
    }
    
    SECTION("Complex range query with many points") {
        std::vector<QuadTreePoint<int>> all_points = generateRandomPoints(1000, 0, 0, 100, 100);
        
        for (const auto& point : all_points) {
            tree.insert(point.x, point.y, point.data);
        }
        
        // Test various query ranges
        BoundingBox query_bounds(25, 25, 75, 75);
        std::vector<QuadTreePoint<int>> tree_results;
        tree.query(query_bounds, tree_results);
        
        // Verify by brute force
        std::vector<QuadTreePoint<int>> brute_results;
        for (const auto& point : all_points) {
            if (query_bounds.contains(point.x, point.y)) {
                brute_results.push_back(point);
            }
        }
        
        REQUIRE(tree_results.size() == brute_results.size());
        
        // Sort both results by data for comparison
        std::sort(tree_results.begin(), tree_results.end(), 
                  [](const auto& a, const auto& b) { return a.data < b.data; });
        std::sort(brute_results.begin(), brute_results.end(), 
                  [](const auto& a, const auto& b) { return a.data < b.data; });
        
        for (size_t i = 0; i < tree_results.size(); ++i) {
            REQUIRE(tree_results[i].data == brute_results[i].data);
        }
    }
}

TEST_CASE("QuadTree Clear Operation", "[quadtree][clear]") {
    BoundingBox bounds(0, 0, 100, 100);
    QuadTree<int> tree(bounds);
    
    SECTION("Clear empty tree") {
        tree.clear();
        REQUIRE(tree.size() == 0);
    }
    
    SECTION("Clear with points") {
        for (int i = 0; i < 100; ++i) {
            tree.insert(i % 10 * 10, i / 10 * 10, i);
        }
        
        REQUIRE(tree.size() == 100);
        
        tree.clear();
        REQUIRE(tree.size() == 0);
        
        std::vector<QuadTreePoint<int>> results;
        tree.query(BoundingBox(0, 0, 100, 100), results);
        REQUIRE(results.empty());
    }
    
    SECTION("Reuse after clear") {
        tree.insert(50, 50, 1);
        tree.clear();
        
        REQUIRE(tree.insert(25, 25, 2));
        REQUIRE(tree.size() == 1);
        
        std::vector<QuadTreePoint<int>> results;
        tree.query(BoundingBox(0, 0, 100, 100), results);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].data == 2);
    }
}

// Canvas resize simulation tests - this is where we test for the specific issue you mentioned
TEST_CASE("Canvas Resize Simulation", "[quadtree][resize]") {
    
    SECTION("Basic resize smaller") {
        MockCanvas canvas(100, 100);
        
        // Add some points
        canvas.addPoint(25, 25, 1);
        canvas.addPoint(75, 75, 2);
        canvas.addPoint(50, 50, 3);
        
        REQUIRE(canvas.quadTree.size() == 3);
        
        // Resize canvas smaller
        canvas.resize(60, 60);
        
        // Point at (75, 75) should be outside new bounds
        REQUIRE(canvas.quadTree.size() == 2);
        
        // Verify we can still query remaining points
        std::vector<QuadTreePoint<int>> results;
        canvas.quadTree.query(BoundingBox(0, 0, 60, 60), results);
        REQUIRE(results.size() == 2);
    }
    
    SECTION("Resize larger") {
        MockCanvas canvas(50, 50);
        
        canvas.addPoint(25, 25, 1);
        canvas.addPoint(40, 40, 2);
        
        REQUIRE(canvas.quadTree.size() == 2);
        
        // Resize larger
        canvas.resize(100, 100);
        
        // All points should still be there
        REQUIRE(canvas.quadTree.size() == 2);
        
        // Add new points in expanded area
        canvas.addPoint(75, 75, 3);
        REQUIRE(canvas.quadTree.size() == 3);
    }
    
    SECTION("Rapid resize sequence") {
        MockCanvas canvas(100, 100);
        
        // Add many points
        for (int i = 0; i < 50; ++i) {
            canvas.addPoint(i * 2, i * 2, i);
        }
        
        REQUIRE(canvas.quadTree.size() == 50);
        
        // Simulate rapid resize sequence
        std::vector<std::pair<float, float>> resize_sequence = {
            {80, 80}, {120, 120}, {50, 50}, {200, 200}, {75, 75}
        };
        
        for (const auto& [width, height] : resize_sequence) {
            canvas.resize(width, height);
            
            // Verify tree consistency
            REQUIRE(canvas.quadTree.getBounds().width() == width);
            REQUIRE(canvas.quadTree.getBounds().height() == height);
            
            // Verify all points in tree are within bounds
            std::vector<QuadTreePoint<int>> results;
            canvas.quadTree.query(canvas.bounds, results);
            
            for (const auto& point : results) {
                REQUIRE(canvas.bounds.contains(point.x, point.y));
            }
        }
    }
    
    SECTION("Resize to zero/negative dimensions") {
        MockCanvas canvas(100, 100);
        canvas.addPoint(50, 50, 1);
        
        // Test edge case: resize to very small dimensions
        canvas.resize(0.1f, 0.1f);
        REQUIRE(canvas.quadTree.getBounds().width() == Approx(0.1f));
        REQUIRE(canvas.quadTree.getBounds().height() == Approx(0.1f));
        
        // Point should be outside bounds now
        REQUIRE(canvas.quadTree.size() == 0);
    }
    
    SECTION("Performance test for large resize") {
        MockCanvas canvas(1000, 1000);
        
        // Add many points
        std::vector<QuadTreePoint<int>> points = generateRandomPoints(10000, 0, 0, 1000, 1000);
        for (const auto& point : points) {
            canvas.addPoint(point.x, point.y, point.data);
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        canvas.resize(800, 800);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // Resize should complete in reasonable time (less than 100ms for 10k points)
        REQUIRE(duration.count() < 100);
        
        // Verify we can still perform queries efficiently
        start = std::chrono::high_resolution_clock::now();
        auto nearest = canvas.quadTree.findNearest(400, 400, 50);
        end = std::chrono::high_resolution_clock::now();
        
        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        REQUIRE(duration_us.count() < 1000); // Should be very fast
    }
}

TEST_CASE("QuadTree Edge Cases and Robustness", "[quadtree][edge-cases]") {
    
    SECTION("Maximum depth reached") {
        BoundingBox bounds(0, 0, 1, 1);
        QuadTree<int> tree(bounds);
        
        // Insert many points in a very small area to force maximum depth
        for (int i = 0; i < 100; ++i) {
            tree.insert(0.5f + i * 0.0001f, 0.5f + i * 0.0001f, i);
        }
        
        REQUIRE(tree.size() == 100);
        
        // Should still be able to find all points
        std::vector<QuadTreePoint<int>> results;
        tree.query(BoundingBox(0.4f, 0.4f, 0.6f, 0.6f), results);
        REQUIRE(results.size() == 100);
    }
    
    SECTION("Duplicate point coordinates") {
        BoundingBox bounds(0, 0, 100, 100);
        QuadTree<int> tree(bounds);
        
        // Insert multiple points at same location
        tree.insert(50, 50, 1);
        tree.insert(50, 50, 2);
        tree.insert(50, 50, 3);
        
        REQUIRE(tree.size() == 3);
        
        // Should find all three points
        std::vector<QuadTreePoint<int>> results;
        tree.query(BoundingBox(49, 49, 51, 51), results);
        REQUIRE(results.size() == 3);
    }
    
    SECTION("Floating point precision") {
        BoundingBox bounds(0, 0, 1, 1);
        QuadTree<int> tree(bounds);
        
        // Test with very small floating point numbers
        tree.insert(0.0000001f, 0.0000001f, 1);
        tree.insert(0.0000002f, 0.0000002f, 2);
        
        REQUIRE(tree.size() == 2);
        
        auto nearest = tree.findNearest(0.0000001f, 0.0000001f, 0.000001f);
        REQUIRE(nearest != nullptr);
        REQUIRE(nearest->data == 1);
    }
    
    SECTION("Very large coordinates") {
        BoundingBox bounds(-1e6f, -1e6f, 1e6f, 1e6f);
        QuadTree<int> tree(bounds);
        
        tree.insert(-500000, -500000, 1);
        tree.insert(500000, 500000, 2);
        
        REQUIRE(tree.size() == 2);
        
        std::vector<QuadTreePoint<int>> results;
        tree.query(BoundingBox(-1e6f, -1e6f, 0, 0), results);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].data == 1);
    }
}

TEST_CASE("QuadTree Mouse Cursor Use Case", "[quadtree][mouse]") {
    
    SECTION("Typical mouse interaction scenario") {
        // Simulate a typical OpenGL canvas scenario
        BoundingBox canvas_bounds(0, 0, 1920, 1080);
        QuadTree<int> tree(canvas_bounds);
        
        // Add some scattered points like in a real application
        std::vector<std::pair<float, float>> points = {
            {100, 100}, {200, 150}, {300, 200}, {400, 250}, {500, 300},
            {600, 350}, {700, 400}, {800, 450}, {900, 500}, {1000, 550},
            {1100, 600}, {1200, 650}, {1300, 700}, {1400, 750}, {1500, 800}
        };
        
        for (size_t i = 0; i < points.size(); ++i) {
            tree.insert(points[i].first, points[i].second, static_cast<int>(i));
        }
        
        // Test mouse cursor at various positions
        struct TestCase {
            float mouse_x, mouse_y;
            float search_radius;
            int expected_point_idx;
        };
        
        std::vector<TestCase> test_cases = {
            {105, 105, 10, 0},     // Near first point
            {205, 155, 15, 1},     // Near second point
            {1505, 805, 20, 14},   // Near last point
            {50, 50, 100, 0},      // Far but within radius of first point
            {1600, 900, 50, -1},   // Too far from any point
        };
        
        for (const auto& test : test_cases) {
            auto result = tree.findNearest(test.mouse_x, test.mouse_y, test.search_radius);
            
            if (test.expected_point_idx == -1) {
                REQUIRE(result == nullptr);
            } else {
                REQUIRE(result != nullptr);
                REQUIRE(result->data == test.expected_point_idx);
            }
        }
    }
    
    SECTION("Performance test for mouse tracking") {
        BoundingBox canvas_bounds(0, 0, 1920, 1080);
        QuadTree<int> tree(canvas_bounds);
        
        // Add many points as in a complex visualization
        std::vector<QuadTreePoint<int>> points = generateRandomPoints(10000, 0, 0, 1920, 1080);
        for (const auto& point : points) {
            tree.insert(point.x, point.y, point.data);
        }
        
        // Simulate mouse movement across the canvas
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 1000; ++i) {
            float mouse_x = (i % 1920) * 1.0f;
            float mouse_y = ((i * 7) % 1080) * 1.0f;
            tree.findNearest(mouse_x, mouse_y, 50);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        // Should handle 1000 nearest neighbor queries in less than 50ms
        REQUIRE(duration.count() < 50);
    }
}

// Additional tests specifically for the canvas resize bug
TEST_CASE("Canvas Resize Bug Investigation", "[quadtree][resize-bug]") {
    
    SECTION("Simulate exact resize scenario") {
        // Test the specific scenario that might be causing issues
        MockCanvas canvas(800, 600);
        
        // Add some points like in a real application
        for (int i = 0; i < 1000; ++i) {
            float x = (i % 40) * 20.0f;
            float y = (i / 40) * 15.0f;
            canvas.addPoint(x, y, i);
        }
        
        // Simulate mouse query before resize
        auto before_nearest = canvas.quadTree.findNearest(400, 300, 50);
        REQUIRE(before_nearest != nullptr);
        
        // Store the point data before resize (since pointer will become invalid)
        float before_x = before_nearest->x;
        float before_y = before_nearest->y;
        int before_data = before_nearest->data;
        
        // Resize canvas
        canvas.resize(1200, 900);
        
        // Same mouse query after resize should work
        auto after_nearest = canvas.quadTree.findNearest(400, 300, 50);
        REQUIRE(after_nearest != nullptr);
        
        // Should be the same point if it's still in bounds
        if (canvas.bounds.contains(before_x, before_y)) {
            REQUIRE(after_nearest->data == before_data);
        }
    }
    
    SECTION("Test for memory corruption during resize") {
        MockCanvas canvas(100, 100);
        
        // Add points
        for (int i = 0; i < 500; ++i) {
            canvas.addPoint(i % 100, i / 100 * 20, i);
        }
        
        // Perform many resizes to test for memory issues
        for (int cycle = 0; cycle < 10; ++cycle) {
            canvas.resize(50 + cycle * 10, 50 + cycle * 10);
            
            // Verify tree is still functional
            REQUIRE(canvas.quadTree.size() >= 0);
            
            // Test query functionality
            std::vector<QuadTreePoint<int>> results;
            canvas.quadTree.query(canvas.bounds, results);
            
            // All returned points should be within bounds
            for (const auto& point : results) {
                REQUIRE(canvas.bounds.contains(point.x, point.y));
            }
        }
    }
    
    SECTION("Test for invalid state after resize") {
        MockCanvas canvas(200, 200);
        
        // Add points with specific pattern
        for (int i = 0; i < 100; ++i) {
            canvas.addPoint(i * 2, i * 2, i);
        }
        
        // Resize to smaller size
        canvas.resize(100, 100);
        
        // Verify the tree is in a valid state
        REQUIRE(canvas.quadTree.size() <= 100);
        
        // All queries should return valid results
        for (int test = 0; test < 10; ++test) {
            float query_x = test * 10;
            float query_y = test * 10;
            
            auto nearest = canvas.quadTree.findNearest(query_x, query_y, 20);
            if (nearest) {
                REQUIRE(canvas.bounds.contains(nearest->x, nearest->y));
            }
            
            std::vector<QuadTreePoint<int>> results;
            canvas.quadTree.query(BoundingBox(query_x - 10, query_y - 10, query_x + 10, query_y + 10), results);
            
            for (const auto& point : results) {
                REQUIRE(canvas.bounds.contains(point.x, point.y));
            }
        }
    }
}
