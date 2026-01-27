/**
 * @file AnalogVertexCache.test.cpp
 * @brief Unit tests for the AnalogVertexCache class
 */

#include "AnalogVertexCache.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace DataViewer;
using Catch::Matchers::WithinAbs;

TEST_CASE("AnalogVertexCache: Basic initialization", "[AnalogVertexCache]") {
    AnalogVertexCache cache;
    
    SECTION("Default state") {
        CHECK_FALSE(cache.isInitialized());
        CHECK_FALSE(cache.isValid());
        CHECK(cache.size() == 0);
        CHECK(cache.capacity() == 0);
    }
    
    SECTION("After initialization") {
        cache.initialize(1000);
        CHECK(cache.isInitialized());
        CHECK_FALSE(cache.isValid());  // Still invalid until data is added
        CHECK(cache.capacity() == 1000);
        CHECK(cache.size() == 0);
    }
}

TEST_CASE("AnalogVertexCache: setVertices", "[AnalogVertexCache]") {
    AnalogVertexCache cache;
    cache.initialize(100);
    
    // Create test vertices
    std::vector<CachedAnalogVertex> vertices;
    for (int i = 0; i < 50; ++i) {
        vertices.push_back({static_cast<float>(i), static_cast<float>(i * 2), TimeFrameIndex{i}});
    }
    
    cache.setVertices(vertices, TimeFrameIndex{0}, TimeFrameIndex{50});
    
    CHECK(cache.isValid());
    CHECK(cache.size() == 50);
    CHECK(cache.cachedStart() == TimeFrameIndex{0});
    CHECK(cache.cachedEnd() == TimeFrameIndex{50});
}

TEST_CASE("AnalogVertexCache: covers", "[AnalogVertexCache]") {
    AnalogVertexCache cache;
    cache.initialize(100);
    
    std::vector<CachedAnalogVertex> vertices;
    for (int i = 10; i < 60; ++i) {
        vertices.push_back({static_cast<float>(i), static_cast<float>(i * 2), TimeFrameIndex{i}});
    }
    cache.setVertices(vertices, TimeFrameIndex{10}, TimeFrameIndex{60});
    
    SECTION("Full coverage") {
        CHECK(cache.covers(TimeFrameIndex{10}, TimeFrameIndex{60}));
        CHECK(cache.covers(TimeFrameIndex{20}, TimeFrameIndex{50}));
        CHECK(cache.covers(TimeFrameIndex{10}, TimeFrameIndex{11}));
    }
    
    SECTION("No coverage") {
        CHECK_FALSE(cache.covers(TimeFrameIndex{0}, TimeFrameIndex{10}));
        CHECK_FALSE(cache.covers(TimeFrameIndex{60}, TimeFrameIndex{70}));
        CHECK_FALSE(cache.covers(TimeFrameIndex{0}, TimeFrameIndex{100}));
    }
    
    SECTION("Partial coverage") {
        CHECK_FALSE(cache.covers(TimeFrameIndex{5}, TimeFrameIndex{30}));
        CHECK_FALSE(cache.covers(TimeFrameIndex{50}, TimeFrameIndex{70}));
    }
}

TEST_CASE("AnalogVertexCache: needsUpdate", "[AnalogVertexCache]") {
    AnalogVertexCache cache;
    cache.initialize(100);
    
    SECTION("Empty cache always needs update") {
        CHECK(cache.needsUpdate(TimeFrameIndex{0}, TimeFrameIndex{50}));
    }
    
    SECTION("Populated cache") {
        std::vector<CachedAnalogVertex> vertices;
        for (int i = 10; i < 60; ++i) {
            vertices.push_back({static_cast<float>(i), static_cast<float>(i * 2), TimeFrameIndex{i}});
        }
        cache.setVertices(vertices, TimeFrameIndex{10}, TimeFrameIndex{60});
        
        // Same range - no update needed
        CHECK_FALSE(cache.needsUpdate(TimeFrameIndex{10}, TimeFrameIndex{60}));
        
        // Subset - no update needed
        CHECK_FALSE(cache.needsUpdate(TimeFrameIndex{20}, TimeFrameIndex{50}));
        
        // Extends past start - update needed
        CHECK(cache.needsUpdate(TimeFrameIndex{5}, TimeFrameIndex{55}));
        
        // Extends past end - update needed
        CHECK(cache.needsUpdate(TimeFrameIndex{15}, TimeFrameIndex{65}));
    }
}

TEST_CASE("AnalogVertexCache: getMissingRanges", "[AnalogVertexCache]") {
    AnalogVertexCache cache;
    cache.initialize(100);
    
    std::vector<CachedAnalogVertex> vertices;
    for (int i = 20; i < 80; ++i) {
        vertices.push_back({static_cast<float>(i), static_cast<float>(i * 2), TimeFrameIndex{i}});
    }
    cache.setVertices(vertices, TimeFrameIndex{20}, TimeFrameIndex{80});
    
    SECTION("No missing ranges when fully covered") {
        auto missing = cache.getMissingRanges(TimeFrameIndex{30}, TimeFrameIndex{70});
        CHECK(missing.empty());
    }
    
    SECTION("Missing range at start (scrolling left)") {
        auto missing = cache.getMissingRanges(TimeFrameIndex{10}, TimeFrameIndex{70});
        REQUIRE(missing.size() == 1);
        CHECK(missing[0].start == TimeFrameIndex{10});
        CHECK(missing[0].end == TimeFrameIndex{20});
        CHECK(missing[0].prepend == true);
    }
    
    SECTION("Missing range at end (scrolling right)") {
        auto missing = cache.getMissingRanges(TimeFrameIndex{30}, TimeFrameIndex{90});
        REQUIRE(missing.size() == 1);
        CHECK(missing[0].start == TimeFrameIndex{80});
        CHECK(missing[0].end == TimeFrameIndex{90});
        CHECK(missing[0].prepend == false);
    }
    
    SECTION("Missing ranges at both ends") {
        auto missing = cache.getMissingRanges(TimeFrameIndex{10}, TimeFrameIndex{90});
        REQUIRE(missing.size() == 2);
        // Should have prepend range first
        CHECK(missing[0].prepend == true);
        CHECK(missing[1].prepend == false);
    }
}

TEST_CASE("AnalogVertexCache: appendVertices", "[AnalogVertexCache]") {
    AnalogVertexCache cache;
    cache.initialize(100);
    
    // Initial vertices
    std::vector<CachedAnalogVertex> initial;
    for (int i = 0; i < 50; ++i) {
        initial.push_back({static_cast<float>(i), static_cast<float>(i * 2), TimeFrameIndex{i}});
    }
    cache.setVertices(initial, TimeFrameIndex{0}, TimeFrameIndex{50});
    
    // Append more vertices
    std::vector<CachedAnalogVertex> append;
    for (int i = 50; i < 60; ++i) {
        append.push_back({static_cast<float>(i), static_cast<float>(i * 2), TimeFrameIndex{i}});
    }
    cache.appendVertices(append);
    
    CHECK(cache.size() == 60);
    CHECK(cache.cachedEnd() == TimeFrameIndex{60});
    CHECK(cache.cachedStart() == TimeFrameIndex{0});  // Start should remain
}

TEST_CASE("AnalogVertexCache: prependVertices", "[AnalogVertexCache]") {
    AnalogVertexCache cache;
    cache.initialize(100);
    
    // Initial vertices
    std::vector<CachedAnalogVertex> initial;
    for (int i = 50; i < 100; ++i) {
        initial.push_back({static_cast<float>(i), static_cast<float>(i * 2), TimeFrameIndex{i}});
    }
    cache.setVertices(initial, TimeFrameIndex{50}, TimeFrameIndex{100});
    
    // Prepend more vertices
    std::vector<CachedAnalogVertex> prepend;
    for (int i = 40; i < 50; ++i) {
        prepend.push_back({static_cast<float>(i), static_cast<float>(i * 2), TimeFrameIndex{i}});
    }
    cache.prependVertices(prepend);
    
    CHECK(cache.size() == 60);
    CHECK(cache.cachedStart() == TimeFrameIndex{40});
    CHECK(cache.cachedEnd() == TimeFrameIndex{100});  // End should remain
}

TEST_CASE("AnalogVertexCache: getVerticesForRange", "[AnalogVertexCache]") {
    AnalogVertexCache cache;
    cache.initialize(100);
    
    std::vector<CachedAnalogVertex> vertices;
    for (int i = 0; i < 10; ++i) {
        vertices.push_back({static_cast<float>(i), static_cast<float>(i * 10), TimeFrameIndex{i}});
    }
    cache.setVertices(vertices, TimeFrameIndex{0}, TimeFrameIndex{10});
    
    SECTION("Extract full range") {
        auto flat = cache.getVerticesForRange(TimeFrameIndex{0}, TimeFrameIndex{10});
        REQUIRE(flat.size() == 20);  // 10 vertices * 2 floats each
        
        // Check first vertex
        CHECK_THAT(flat[0], WithinAbs(0.0f, 0.01f));
        CHECK_THAT(flat[1], WithinAbs(0.0f, 0.01f));
        
        // Check last vertex
        CHECK_THAT(flat[18], WithinAbs(9.0f, 0.01f));
        CHECK_THAT(flat[19], WithinAbs(90.0f, 0.01f));
    }
    
    SECTION("Extract partial range") {
        auto flat = cache.getVerticesForRange(TimeFrameIndex{2}, TimeFrameIndex{5});
        REQUIRE(flat.size() == 6);  // 3 vertices * 2 floats
        
        // Check first vertex (index 2)
        CHECK_THAT(flat[0], WithinAbs(2.0f, 0.01f));
        CHECK_THAT(flat[1], WithinAbs(20.0f, 0.01f));
        
        // Check last vertex (index 4)
        CHECK_THAT(flat[4], WithinAbs(4.0f, 0.01f));
        CHECK_THAT(flat[5], WithinAbs(40.0f, 0.01f));
    }
    
    SECTION("Uncached range returns empty") {
        auto flat = cache.getVerticesForRange(TimeFrameIndex{100}, TimeFrameIndex{200});
        CHECK(flat.empty());
    }
}

TEST_CASE("AnalogVertexCache: invalidate", "[AnalogVertexCache]") {
    AnalogVertexCache cache;
    cache.initialize(100);
    
    std::vector<CachedAnalogVertex> vertices;
    for (int i = 0; i < 50; ++i) {
        vertices.push_back({static_cast<float>(i), static_cast<float>(i * 2), TimeFrameIndex{i}});
    }
    cache.setVertices(vertices, TimeFrameIndex{0}, TimeFrameIndex{50});
    
    CHECK(cache.isValid());
    CHECK(cache.size() == 50);
    
    cache.invalidate();
    
    CHECK_FALSE(cache.isValid());
    CHECK(cache.size() == 0);
    CHECK(cache.isInitialized());  // Capacity should remain
}

TEST_CASE("AnalogVertexCache: capacity overflow", "[AnalogVertexCache]") {
    AnalogVertexCache cache;
    cache.initialize(50);  // Small capacity
    
    // Set initial vertices at capacity
    std::vector<CachedAnalogVertex> initial;
    for (int i = 0; i < 50; ++i) {
        initial.push_back({static_cast<float>(i), static_cast<float>(i * 2), TimeFrameIndex{i}});
    }
    cache.setVertices(initial, TimeFrameIndex{0}, TimeFrameIndex{50});
    
    CHECK(cache.size() == 50);
    
    // Append more - should push out old ones
    std::vector<CachedAnalogVertex> append;
    for (int i = 50; i < 60; ++i) {
        append.push_back({static_cast<float>(i), static_cast<float>(i * 2), TimeFrameIndex{i}});
    }
    cache.appendVertices(append);
    
    // Size should still be at capacity
    CHECK(cache.size() == 50);
    // Start should have moved forward (old data pushed out)
    CHECK(cache.cachedStart() == TimeFrameIndex{10});
    CHECK(cache.cachedEnd() == TimeFrameIndex{60});
}
