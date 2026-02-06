/**
 * @file SceneBuilder.test.cpp
 * @brief Unit tests for SceneBuilder
 *
 * Tests the SceneBuilder class focusing on:
 * - Entity to series_key mapping for hit test enrichment
 * - Spatial index population
 * - Scene construction
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "CorePlotting/Mappers/MappedElement.hpp"
#include "CoreGeometry/boundingbox.hpp"

using namespace CorePlotting;
using Catch::Matchers::WithinAbs;

namespace {

// Helper to create MappedElements for testing
std::vector<MappedElement> createTestEvents(
    std::vector<std::pair<float, float>> const & positions,
    uint64_t start_entity_id = 1) {
    std::vector<MappedElement> elements;
    uint64_t id = start_entity_id;
    for (auto const & [x, y] : positions) {
        elements.push_back(MappedElement{x, y, EntityId{id++}});
    }
    return elements;
}

} // anonymous namespace

TEST_CASE("SceneBuilder entity_to_series_key mapping", "[CorePlotting][SceneBuilder]") {
    BoundingBox bounds(-100.0f, -1.0f, 400.0f, 1.0f);
    
    SECTION("addGlyphs populates entity_to_series_key mapping") {
        SceneBuilder builder;
        builder.setBounds(bounds);
        
        // Add events for trial_0
        auto trial0_events = createTestEvents({{100.0f, 0.5f}, {150.0f, 0.5f}}, 1);
        builder.addGlyphs("trial_0", trial0_events);
        
        // Add events for trial_1
        auto trial1_events = createTestEvents({{120.0f, -0.3f}, {180.0f, -0.3f}}, 100);
        builder.addGlyphs("trial_1", trial1_events);
        
        auto scene = builder.build();
        
        // Verify entity_to_series_key mapping is populated
        REQUIRE(scene.entity_to_series_key.size() == 4);
        
        // trial_0 entities
        REQUIRE(scene.entity_to_series_key.at(EntityId{1}) == "trial_0");
        REQUIRE(scene.entity_to_series_key.at(EntityId{2}) == "trial_0");
        
        // trial_1 entities  
        REQUIRE(scene.entity_to_series_key.at(EntityId{100}) == "trial_1");
        REQUIRE(scene.entity_to_series_key.at(EntityId{101}) == "trial_1");
    }
    
    SECTION("addRectangles populates entity_to_series_key mapping") {
        SceneBuilder builder;
        builder.setBounds(bounds);
        
        // Create rectangle elements (intervals)
        std::vector<MappedRectElement> intervals;
        intervals.push_back(MappedRectElement{50.0f, 0.0f, 100.0f, 0.5f, EntityId{42}});
        intervals.push_back(MappedRectElement{200.0f, 0.0f, 50.0f, 0.5f, EntityId{43}});
        
        builder.addRectangles("intervals", intervals);
        
        auto scene = builder.build();
        
        REQUIRE(scene.entity_to_series_key.size() == 2);
        REQUIRE(scene.entity_to_series_key.at(EntityId{42}) == "intervals");
        REQUIRE(scene.entity_to_series_key.at(EntityId{43}) == "intervals");
    }
    
    SECTION("Mixed glyphs and rectangles share entity_to_series_key map") {
        SceneBuilder builder;
        builder.setBounds(bounds);
        
        auto events = createTestEvents({{100.0f, 0.5f}}, 1);
        builder.addGlyphs("events_series", events);
        
        std::vector<MappedRectElement> intervals;
        intervals.push_back(MappedRectElement{200.0f, -0.5f, 100.0f, 0.5f, EntityId{99}});
        builder.addRectangles("intervals_series", intervals);
        
        auto scene = builder.build();
        
        REQUIRE(scene.entity_to_series_key.size() == 2);
        REQUIRE(scene.entity_to_series_key.at(EntityId{1}) == "events_series");
        REQUIRE(scene.entity_to_series_key.at(EntityId{99}) == "intervals_series");
    }
    
    SECTION("Empty builder produces empty entity_to_series_key map") {
        SceneBuilder builder;
        builder.setBounds(bounds);
        
        auto scene = builder.build();
        
        REQUIRE(scene.entity_to_series_key.empty());
    }
    
    SECTION("reset() clears entity_to_series_key mapping") {
        SceneBuilder builder;
        builder.setBounds(bounds);
        
        auto events = createTestEvents({{100.0f, 0.5f}}, 1);
        builder.addGlyphs("series", events);
        
        // Build first scene
        auto scene1 = builder.build();
        REQUIRE(scene1.entity_to_series_key.size() == 1);
        
        // Builder should be reset - build empty scene
        builder.setBounds(bounds);
        auto scene2 = builder.build();
        REQUIRE(scene2.entity_to_series_key.empty());
    }
}

TEST_CASE("SceneBuilder spatial index with entity_to_series_key", "[CorePlotting][SceneBuilder]") {
    BoundingBox bounds(-100.0f, -1.0f, 400.0f, 1.0f);
    
    SECTION("Spatial index is populated alongside entity_to_series_key") {
        SceneBuilder builder;
        builder.setBounds(bounds);
        
        auto events = createTestEvents({{100.0f, 0.5f}, {200.0f, 0.3f}}, 1);
        builder.addGlyphs("trial_0", events);
        
        auto scene = builder.build();
        
        // Verify spatial index was created
        REQUIRE(scene.spatial_index != nullptr);
        
        // Verify we can query the spatial index
        auto * nearest = scene.spatial_index->findNearest(100.0f, 0.5f, 1.0f);
        REQUIRE(nearest != nullptr);
        REQUIRE(nearest->data == EntityId{1});
        
        // And we can look up the series_key for this entity
        REQUIRE(scene.entity_to_series_key.at(nearest->data) == "trial_0");
    }
}

TEST_CASE("SceneBuilder glyph/rectangle batch key maps", "[CorePlotting][SceneBuilder]") {
    BoundingBox bounds(-100.0f, -1.0f, 400.0f, 1.0f);
    
    SECTION("Glyph batch key map is populated") {
        SceneBuilder builder;
        builder.setBounds(bounds);
        
        auto events1 = createTestEvents({{100.0f, 0.5f}}, 1);
        auto events2 = createTestEvents({{200.0f, -0.5f}}, 10);
        
        builder.addGlyphs("series_A", events1);
        builder.addGlyphs("series_B", events2);
        
        auto key_map = builder.getGlyphBatchKeyMap();
        
        REQUIRE(key_map.size() == 2);
        REQUIRE(key_map.at(0) == "series_A");
        REQUIRE(key_map.at(1) == "series_B");
    }
    
    SECTION("Rectangle batch key map is populated") {
        SceneBuilder builder;
        builder.setBounds(bounds);
        
        std::vector<MappedRectElement> intervals1;
        intervals1.push_back(MappedRectElement{50.0f, 0.0f, 100.0f, 0.5f, EntityId{1}});
        
        std::vector<MappedRectElement> intervals2;
        intervals2.push_back(MappedRectElement{200.0f, 0.0f, 50.0f, 0.5f, EntityId{2}});
        
        builder.addRectangles("intervals_A", intervals1);
        builder.addRectangles("intervals_B", intervals2);
        
        auto key_map = builder.getRectangleBatchKeyMap();
        
        REQUIRE(key_map.size() == 2);
        REQUIRE(key_map.at(0) == "intervals_A");
        REQUIRE(key_map.at(1) == "intervals_B");
    }
}
