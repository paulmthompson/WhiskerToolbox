#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include "RTree.hpp"
#include "QuadTree.hpp" // For BoundingBox definition
#include <random>
#include <set>
#include <algorithm>
#include <chrono>

using namespace Catch;

// Helper function to generate random bounding boxes
std::vector<RTreeEntry<int>> generateRandomBoundingBoxes(int count, float canvas_width, float canvas_height, 
                                                         float min_size = 5.0f, float max_size = 50.0f) {
    std::vector<RTreeEntry<int>> boxes;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> size_dist(min_size, max_size);
    
    for (int i = 0; i < count; ++i) {
        float width = size_dist(gen);
        float height = size_dist(gen);
        
        std::uniform_real_distribution<float> x_dist(0, canvas_width - width);
        std::uniform_real_distribution<float> y_dist(0, canvas_height - height);
        
        float x = x_dist(gen);
        float y = y_dist(gen);
        
        boxes.emplace_back(x, y, x + width, y + height, i);
    }
    return boxes;
}

// Helper function to brute force find entries containing a point
template<typename T>
std::vector<const RTreeEntry<T>*> bruteForcePointQuery(const std::vector<RTreeEntry<T>>& entries, float x, float y) {
    std::vector<const RTreeEntry<T>*> results;
    for (const auto& entry : entries) {
        if (entry.contains(x, y)) {
            results.push_back(&entry);
        }
    }
    return results;
}

// Helper function to brute force find nearest entry
template<typename T>
const RTreeEntry<T>* bruteForceNearest(const std::vector<RTreeEntry<T>>& entries, float x, float y, float max_distance) {
    const RTreeEntry<T>* nearest = nullptr;
    float min_distance = max_distance;
    
    for (const auto& entry : entries) {
        float distance = entry.distanceToPoint(x, y);
        if (distance < min_distance) {
            min_distance = distance;
            nearest = &entry;
        }
    }
    
    return nearest;
}

// Helper function to find all entries at the minimum distance (for tie handling)
template<typename T>
std::vector<const RTreeEntry<T>*> bruteForceNearestAll(const std::vector<RTreeEntry<T>>& entries, float x, float y, float max_distance) {
    std::vector<const RTreeEntry<T>*> nearest_entries;
    float min_distance = max_distance;
    
    // First pass: find minimum distance
    for (const auto& entry : entries) {
        float distance = entry.distanceToPoint(x, y);
        if (distance < min_distance) {
            min_distance = distance;
        }
    }
    
    // Second pass: collect all entries at minimum distance
    for (const auto& entry : entries) {
        float distance = entry.distanceToPoint(x, y);
        if (std::abs(distance - min_distance) < 1e-6f) {
            nearest_entries.push_back(&entry);
        }
    }
    
    return nearest_entries;
}

TEST_CASE("RTree Basic Operations", "[rtree][basic]") {
    RTree<int> tree;
    
    SECTION("Empty tree") {
        REQUIRE(tree.size() == 0);
        
        std::vector<RTreeEntry<int>> results;
        tree.query(BoundingBox(0, 0, 50, 50), results);
        REQUIRE(results.empty());
        
        tree.queryPoint(25, 25, results);
        REQUIRE(results.empty());
        
        auto nearest = tree.findNearest(25, 25, 10);
        REQUIRE(nearest == nullptr);
    }
    
    SECTION("Single entry insertion and query") {
        REQUIRE(tree.insert(10, 10, 40, 40, 1));
        REQUIRE(tree.size() == 1);
        
        // Test bounding box query
        std::vector<RTreeEntry<int>> results;
        tree.query(BoundingBox(0, 0, 50, 50), results);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].data == 1);
        
        // Test point query - point inside
        results.clear();
        tree.queryPoint(25, 25, results);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].data == 1);
        
        // Test point query - point outside
        results.clear();
        tree.queryPoint(5, 5, results);
        REQUIRE(results.empty());
        
        // Test nearest neighbor
        auto nearest = tree.findNearest(25, 25, 100);
        REQUIRE(nearest != nullptr);
        REQUIRE(nearest->data == 1);
    }
    
    SECTION("Invalid bounding box insertion") {
        REQUIRE_FALSE(tree.insert(40, 40, 10, 10, 1)); // max < min
        REQUIRE_FALSE(tree.insert(10, 40, 40, 10, 1)); // max_y < min_y
        REQUIRE(tree.size() == 0);
    }
    
    SECTION("Multiple entries") {
        REQUIRE(tree.insert(10, 10, 30, 30, 1));
        REQUIRE(tree.insert(50, 50, 70, 70, 2));
        REQUIRE(tree.insert(20, 40, 60, 80, 3));
        REQUIRE(tree.size() == 3);
        
        // Test queries
        std::vector<RTreeEntry<int>> results;
        tree.query(BoundingBox(0, 0, 100, 100), results);
        REQUIRE(results.size() == 3);
        
        // Test point queries
        results.clear();
        tree.queryPoint(25, 25, results);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].data == 1);
        
        results.clear();
        tree.queryPoint(55, 55, results);
        REQUIRE(results.size() == 2); // Should find both entry 2 and 3
        
        std::set<int> found_data;
        for (const auto& entry : results) {
            found_data.insert(entry.data);
        }
        REQUIRE(found_data.count(2) == 1);
        REQUIRE(found_data.count(3) == 1);
    }
}

TEST_CASE("RTree Entry Distance Calculations", "[rtree][distance]") {
    RTreeEntry<int> entry(10, 10, 50, 50, 1);
    
    SECTION("Point inside bounding box") {
        REQUIRE(entry.distanceToPoint(25, 25) == Approx(0.0f));
        REQUIRE(entry.distanceToPointSquared(25, 25) == Approx(0.0f));
    }
    
    SECTION("Point outside bounding box") {
        // Point to the left
        REQUIRE(entry.distanceToPoint(5, 25) == Approx(5.0f));
        REQUIRE(entry.distanceToPointSquared(5, 25) == Approx(25.0f));
        
        // Point to the right
        REQUIRE(entry.distanceToPoint(55, 25) == Approx(5.0f));
        REQUIRE(entry.distanceToPointSquared(55, 25) == Approx(25.0f));
        
        // Point above
        REQUIRE(entry.distanceToPoint(25, 5) == Approx(5.0f));
        REQUIRE(entry.distanceToPointSquared(25, 5) == Approx(25.0f));
        
        // Point below
        REQUIRE(entry.distanceToPoint(25, 55) == Approx(5.0f));
        REQUIRE(entry.distanceToPointSquared(25, 55) == Approx(25.0f));
        
        // Point at corner
        REQUIRE(entry.distanceToPoint(5, 5) == Approx(std::sqrt(50.0f)));
        REQUIRE(entry.distanceToPointSquared(5, 5) == Approx(50.0f));
    }
}

TEST_CASE("RTree Point Containment Queries", "[rtree][point-query]") {
    RTree<int> tree;
    
    SECTION("Single overlapping entries") {
        tree.insert(10, 10, 50, 50, 1);
        tree.insert(30, 30, 70, 70, 2);
        tree.insert(0, 0, 100, 100, 3);
        
        // Point in overlap of all three
        std::vector<RTreeEntry<int>> results;
        tree.queryPoint(40, 40, results);
        REQUIRE(results.size() == 3);
        
        // Point in overlap of two
        results.clear();
        tree.queryPoint(20, 20, results);
        REQUIRE(results.size() == 2);
        
        // Point in only one
        results.clear();
        tree.queryPoint(5, 5, results);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].data == 3);
        
        // Point in none
        results.clear();
        tree.queryPoint(200, 200, results);
        REQUIRE(results.empty());
    }
    
    SECTION("Complex overlapping scenario") {
        // Create a grid of overlapping rectangles
        for (int i = 0; i < 5; ++i) {
            for (int j = 0; j < 5; ++j) {
                float x = i * 20.0f;
                float y = j * 20.0f;
                tree.insert(x, y, x + 30, y + 30, i * 5 + j);
            }
        }
        
        // Test point in center where many rectangles overlap
        std::vector<RTreeEntry<int>> results;
        tree.queryPoint(50, 50, results);
        REQUIRE(results.size() > 1);
        
        // Verify all returned entries actually contain the point
        for (const auto& entry : results) {
            REQUIRE(entry.contains(50, 50));
        }
    }
}

TEST_CASE("RTree Nearest Neighbor Search", "[rtree][nearest]") {
    RTree<int> tree;
    
    SECTION("Single entry nearest") {
        tree.insert(50, 50, 60, 60, 1);
        
        // Point inside - distance should be 0
        auto nearest = tree.findNearest(55, 55, 100);
        REQUIRE(nearest != nullptr);
        REQUIRE(nearest->data == 1);
        REQUIRE(nearest->distanceToPoint(55, 55) == Approx(0.0f));
        
        // Point outside but within range
        nearest = tree.findNearest(45, 55, 10);
        REQUIRE(nearest != nullptr);
        REQUIRE(nearest->data == 1);
        REQUIRE(nearest->distanceToPoint(45, 55) == Approx(5.0f));
        
        // Point outside and out of range
        nearest = tree.findNearest(30, 55, 10);
        REQUIRE(nearest == nullptr);
    }
    
    SECTION("Multiple entries nearest") {
        tree.insert(10, 10, 20, 20, 1);
        tree.insert(50, 50, 60, 60, 2);
        tree.insert(100, 100, 110, 110, 3);
        
        // Point nearest to first entry
        auto nearest = tree.findNearest(25, 15, 100);
        REQUIRE(nearest != nullptr);
        REQUIRE(nearest->data == 1);
        
        // Point nearest to second entry
        nearest = tree.findNearest(55, 45, 100);
        REQUIRE(nearest != nullptr);
        REQUIRE(nearest->data == 2);
        
        // Point nearest to third entry
        nearest = tree.findNearest(105, 95, 100);
        REQUIRE(nearest != nullptr);
        REQUIRE(nearest->data == 3);
    }
    
    SECTION("Nearest accuracy test with random entries") {
        std::vector<RTreeEntry<int>> entries = generateRandomBoundingBoxes(100, 200, 200, 5, 30);
        
        for (const auto& entry : entries) {
            tree.insert(entry.min_x, entry.min_y, entry.max_x, entry.max_y, entry.data);
        }
        
        // Test multiple query points
        for (int i = 0; i < 20; ++i) {
            float query_x = i * 10;
            float query_y = i * 10;
            float max_dist = 50;
            
            auto tree_nearest = tree.findNearest(query_x, query_y, max_dist);
            auto brute_nearest = bruteForceNearest(entries, query_x, query_y, max_dist);
            
            if (brute_nearest == nullptr) {
                REQUIRE(tree_nearest == nullptr);
            } else {
                REQUIRE(tree_nearest != nullptr);
                
                // Check if both results have the same distance (handles ties correctly)
                float tree_distance = tree_nearest->distanceToPoint(query_x, query_y);
                float brute_distance = brute_nearest->distanceToPoint(query_x, query_y);
                REQUIRE(tree_distance == Approx(brute_distance).epsilon(1e-6f));
                
                // Both should be within the max distance
                REQUIRE(tree_distance <= max_dist);
                REQUIRE(brute_distance <= max_dist);
            }
        }
    }
}

TEST_CASE("RTree Range Query", "[rtree][range-query]") {
    RTree<int> tree;
    
    SECTION("Simple range query") {
        tree.insert(10, 10, 30, 30, 1);
        tree.insert(50, 50, 70, 70, 2);
        tree.insert(20, 60, 40, 80, 3);
        
        // Query that should find first entry
        std::vector<RTreeEntry<int>> results;
        tree.query(BoundingBox(0, 0, 35, 35), results);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].data == 1);
        
        // Query that should find multiple entries
        results.clear();
        tree.query(BoundingBox(0, 0, 100, 100), results);
        REQUIRE(results.size() == 3);
        
        // Query that should find no entries
        results.clear();
        tree.query(BoundingBox(200, 200, 300, 300), results);
        REQUIRE(results.empty());
    }
    
    SECTION("Complex range query with many entries") {
        std::vector<RTreeEntry<int>> all_entries = generateRandomBoundingBoxes(1000, 500, 500, 10, 50);
        
        for (const auto& entry : all_entries) {
            tree.insert(entry.min_x, entry.min_y, entry.max_x, entry.max_y, entry.data);
        }
        
        // Test various query ranges
        BoundingBox query_bounds(100, 100, 200, 200);
        std::vector<RTreeEntry<int>> tree_results;
        tree.query(query_bounds, tree_results);
        
        // Verify by brute force
        std::vector<RTreeEntry<int>> brute_results;
        for (const auto& entry : all_entries) {
            if (entry.intersects(query_bounds)) {
                brute_results.push_back(entry);
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

TEST_CASE("RTree Pointer Queries", "[rtree][pointer-queries]") {
    RTree<int> tree;
    
    SECTION("Query pointers for range") {
        tree.insert(10, 10, 30, 30, 1);
        tree.insert(50, 50, 70, 70, 2);
        tree.insert(20, 60, 40, 80, 3);
        
        std::vector<const RTreeEntry<int>*> results;
        tree.queryPointers(BoundingBox(0, 0, 100, 100), results);
        REQUIRE(results.size() == 3);
        
        // Verify pointers are valid
        for (const auto* entry : results) {
            REQUIRE(entry != nullptr);
            REQUIRE(entry->data >= 1);
            REQUIRE(entry->data <= 3);
        }
    }
    
    SECTION("Query pointers for point") {
        tree.insert(10, 10, 50, 50, 1);
        tree.insert(30, 30, 70, 70, 2);
        
        std::vector<const RTreeEntry<int>*> results;
        tree.queryPointPointers(40, 40, results);
        REQUIRE(results.size() == 2);
        
        // Verify all returned entries contain the point
        for (const auto* entry : results) {
            REQUIRE(entry->contains(40, 40));
        }
    }
}

TEST_CASE("RTree Clear Operation", "[rtree][clear]") {
    RTree<int> tree;
    
    SECTION("Clear empty tree") {
        tree.clear();
        REQUIRE(tree.size() == 0);
    }
    
    SECTION("Clear with entries") {
        for (int i = 0; i < 100; ++i) {
            float x = i * 10.0f;
            float y = i * 10.0f;
            tree.insert(x, y, x + 20, y + 20, i);
        }
        
        REQUIRE(tree.size() == 100);
        
        tree.clear();
        REQUIRE(tree.size() == 0);
        
        std::vector<RTreeEntry<int>> results;
        tree.query(BoundingBox(0, 0, 1000, 1000), results);
        REQUIRE(results.empty());
    }
    
    SECTION("Reuse after clear") {
        tree.insert(10, 10, 30, 30, 1);
        tree.clear();
        
        REQUIRE(tree.insert(50, 50, 70, 70, 2));
        REQUIRE(tree.size() == 1);
        
        std::vector<RTreeEntry<int>> results;
        tree.query(BoundingBox(0, 0, 100, 100), results);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].data == 2);
    }
}

TEST_CASE("RTree Performance Tests", "[rtree][performance]") {
    
    SECTION("Large dataset insertion and query") {
        RTree<int> tree;
        
        // Insert many entries
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 10000; ++i) {
            float x = (i % 100) * 5.0f;
            float y = (i / 100) * 5.0f;
            tree.insert(x, y, x + 10, y + 10, i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        REQUIRE(duration.count() < 1000); // Should insert 10k entries in less than 1 second
        
        REQUIRE(tree.size() == 10000);
        
        // Test query performance
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; ++i) {
            std::vector<RTreeEntry<int>> results;
            tree.query(BoundingBox(i * 0.5f, i * 0.5f, i * 0.5f + 50, i * 0.5f + 50), results);
        }
        end = std::chrono::high_resolution_clock::now();
        
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        REQUIRE(duration.count() < 100); // Should handle 1000 queries in less than 100ms
    }
    
    SECTION("Point query performance") {
        RTree<int> tree;
        
        // Insert random bounding boxes
        std::vector<RTreeEntry<int>> entries = generateRandomBoundingBoxes(5000, 1000, 1000, 5, 50);
        for (const auto& entry : entries) {
            tree.insert(entry.min_x, entry.min_y, entry.max_x, entry.max_y, entry.data);
        }
        
        // Test point query performance
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; ++i) {
            std::vector<RTreeEntry<int>> results;
            tree.queryPoint(i * 0.5f, i * 0.5f, results);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        REQUIRE(duration.count() < 50); // Should handle 1000 point queries very quickly
    }
    
    SECTION("Nearest neighbor query performance") {
        RTree<int> tree;
        
        // Insert random bounding boxes
        std::vector<RTreeEntry<int>> entries = generateRandomBoundingBoxes(5000, 1000, 1000, 5, 50);
        for (const auto& entry : entries) {
            tree.insert(entry.min_x, entry.min_y, entry.max_x, entry.max_y, entry.data);
        }
        
        // Test nearest neighbor performance
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; ++i) {
            tree.findNearest(i * 0.5f, i * 0.5f, 100);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        REQUIRE(duration.count() < 100); // Should handle 1000 nearest queries efficiently
    }
}

TEST_CASE("RTree Use Cases", "[rtree][use-cases]") {
    
    SECTION("Line bounding boxes scenario") {
        RTree<std::string> tree;
        
        // Simulate line bounding boxes
        tree.insert(10, 10, 100, 15, "horizontal_line_1");
        tree.insert(20, 20, 25, 80, "vertical_line_1");
        tree.insert(50, 50, 150, 55, "horizontal_line_2");
        tree.insert(75, 30, 80, 90, "vertical_line_2");
        
        // Find lines near a point
        auto nearest = tree.findNearest(60, 52, 20);
        REQUIRE(nearest != nullptr);
        REQUIRE(nearest->data == "horizontal_line_2");
        
        // Find all lines in a region
        std::vector<RTreeEntry<std::string>> results;
        tree.query(BoundingBox(70, 25, 85, 95), results);
        REQUIRE(results.size() == 2); // Should find both lines that intersect this region
    }
    
    SECTION("Mask regions scenario") {
        RTree<int> tree; // Using int as mask ID
        
        // Insert several mask regions
        tree.insert(0, 0, 100, 100, 1);    // Large background mask
        tree.insert(20, 20, 80, 80, 2);    // Medium mask
        tree.insert(40, 40, 60, 60, 3);    // Small mask
        tree.insert(10, 90, 30, 110, 4);   // Partially outside main area
        
        // Query for all masks at a specific point
        std::vector<RTreeEntry<int>> results;
        tree.queryPoint(50, 50, results);
        
        // Should find all three overlapping masks
        REQUIRE(results.size() == 3);
        
        std::set<int> mask_ids;
        for (const auto& entry : results) {
            mask_ids.insert(entry.data);
        }
        
        REQUIRE(mask_ids.count(1) == 1);
        REQUIRE(mask_ids.count(2) == 1);
        REQUIRE(mask_ids.count(3) == 1);
        
        // Query at edge should find fewer masks
        results.clear();
        tree.queryPoint(25, 25, results);
        REQUIRE(results.size() == 2); // Should find masks 1 and 2
    }
    
    SECTION("Canvas resize simulation") {
        RTree<int> tree;
        
        // Add many bounding boxes
        for (int i = 0; i < 1000; ++i) {
            float x = (i % 50) * 10.0f;
            float y = (i / 50) * 10.0f;
            tree.insert(x, y, x + 20, y + 20, i);
        }
        
        REQUIRE(tree.size() == 1000);
        
        // Simulate canvas resize by querying only visible area
        BoundingBox visible_area(0, 0, 300, 300);
        std::vector<RTreeEntry<int>> visible_entries;
        tree.query(visible_area, visible_entries);
        
        // Should efficiently cull entries outside visible area
        REQUIRE(visible_entries.size() < 1000);
        
        // All returned entries should intersect visible area
        for (const auto& entry : visible_entries) {
            REQUIRE(entry.intersects(visible_area));
        }
    }
}

TEST_CASE("RTree Edge Cases", "[rtree][edge-cases]") {
    
    SECTION("Zero-area bounding boxes") {
        RTree<int> tree;
        
        // Insert point-like bounding boxes
        tree.insert(10, 10, 10, 10, 1);
        tree.insert(20, 20, 20, 20, 2);
        
        REQUIRE(tree.size() == 2);
        
        // Should be able to find them
        std::vector<RTreeEntry<int>> results;
        tree.queryPoint(10, 10, results);
        REQUIRE(results.size() == 1);
        REQUIRE(results[0].data == 1);
    }
    
    SECTION("Large bounding boxes") {
        RTree<int> tree;
        
        // Insert very large bounding box
        tree.insert(-1000, -1000, 1000, 1000, 1);
        tree.insert(10, 10, 20, 20, 2);
        
        // Point query should find both
        std::vector<RTreeEntry<int>> results;
        tree.queryPoint(15, 15, results);
        REQUIRE(results.size() == 2);
    }
    
    SECTION("Overlapping identical bounding boxes") {
        RTree<int> tree;
        
        // Insert multiple identical bounding boxes
        tree.insert(10, 10, 50, 50, 1);
        tree.insert(10, 10, 50, 50, 2);
        tree.insert(10, 10, 50, 50, 3);
        
        REQUIRE(tree.size() == 3);
        
        // Should find all of them
        std::vector<RTreeEntry<int>> results;
        tree.queryPoint(25, 25, results);
        REQUIRE(results.size() == 3);
    }
    
    SECTION("Move semantics") {
        RTree<int> tree1;
        tree1.insert(10, 10, 30, 30, 1);
        tree1.insert(50, 50, 70, 70, 2);
        REQUIRE(tree1.size() == 2);
        
        // Move constructor
        RTree<int> tree2 = std::move(tree1);
        REQUIRE(tree2.size() == 2);
        REQUIRE(tree1.size() == 0);
        
        // Move assignment
        RTree<int> tree3;
        tree3 = std::move(tree2);
        REQUIRE(tree3.size() == 2);
        REQUIRE(tree2.size() == 0);
        
        // Verify tree3 works correctly
        std::vector<RTreeEntry<int>> results;
        tree3.query(BoundingBox(0, 0, 100, 100), results);
        REQUIRE(results.size() == 2);
    }
}
