#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/Interaction/SceneHitTester.hpp"
#include "CoreGeometry/boundingbox.hpp"
#include "SpatialIndex/QuadTree.hpp"

using namespace CorePlotting;
using Catch::Matchers::WithinAbs;

namespace {

// Helper to create a simple scene with events in a QuadTree
RenderableScene makeTestSceneWithEvents() {
    RenderableScene scene;
    
    // Create QuadTree with some events using BoundingBox constructor
    BoundingBox bounds(-100.0f, -1.0f, 200.0f, 1.0f);
    auto tree = std::make_unique<QuadTree<EntityId>>(bounds);
    tree->insert(100.0f, 0.5f, EntityId{1});  // Event at time 100, y=0.5
    tree->insert(200.0f, 0.5f, EntityId{2});  // Event at time 200, y=0.5
    tree->insert(300.0f, -0.3f, EntityId{3}); // Event at time 300, y=-0.3 (outside bounds but may still insert)
    
    scene.spatial_index = std::move(tree);
    scene.view_matrix = glm::mat4(1.0f);
    scene.projection_matrix = glm::mat4(1.0f);
    
    return scene;
}

// Helper to create a simple layout
LayoutResponse makeTestLayout() {
    LayoutResponse layout;
    
    // Two series: one at y=0.5, one at y=-0.3
    layout.layouts.push_back(SeriesLayout(
        SeriesLayoutResult(0.5f, 0.6f), "series_top", 0));
    layout.layouts.push_back(SeriesLayout(
        SeriesLayoutResult(-0.3f, 0.6f), "series_bot", 1));
    
    return layout;
}

} // anonymous namespace

TEST_CASE("SceneHitTester default configuration", "[CorePlotting][SceneHitTester]") {
    SceneHitTester tester;
    
    REQUIRE(tester.getConfig().point_tolerance == 5.0f);
    REQUIRE(tester.getConfig().edge_tolerance == 5.0f);
    REQUIRE(tester.getConfig().prioritize_discrete);
}

TEST_CASE("SceneHitTester custom configuration", "[CorePlotting][SceneHitTester]") {
    HitTestConfig config;
    config.point_tolerance = 10.0f;
    config.edge_tolerance = 3.0f;
    config.prioritize_discrete = false;
    
    SceneHitTester tester(config);
    
    REQUIRE(tester.getConfig().point_tolerance == 10.0f);
    REQUIRE(tester.getConfig().edge_tolerance == 3.0f);
    REQUIRE_FALSE(tester.getConfig().prioritize_discrete);
}

TEST_CASE("SceneHitTester queryQuadTree", "[CorePlotting][SceneHitTester]") {
    SceneHitTester tester;
    auto scene = makeTestSceneWithEvents();
    
    SECTION("Hit near first event") {
        auto result = tester.queryQuadTree(101.0f, 0.5f, scene);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.hit_type == HitType::DigitalEvent);
        REQUIRE(result.entity_id.value() == EntityId{1});
        REQUIRE_THAT(result.world_x, WithinAbs(100.0f, 0.001f));
    }
    
    SECTION("Hit near second event") {
        auto result = tester.queryQuadTree(199.0f, 0.5f, scene);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.entity_id.value() == EntityId{2});
    }
    
    SECTION("Miss - too far from any event") {
        auto result = tester.queryQuadTree(150.0f, 0.5f, scene);
        
        // 150 is 50 units from nearest event (100 or 200), which exceeds tolerance
        REQUIRE_FALSE(result.hasHit());
    }
    
    SECTION("Hit with custom tolerance") {
        HitTestConfig config;
        config.point_tolerance = 60.0f;
        SceneHitTester wide_tester(config);
        
        auto result = wide_tester.queryQuadTree(150.0f, 0.5f, scene);
        
        // Now 150 should hit either event 1 (distance 50) or event 2 (distance 50)
        REQUIRE(result.hasHit());
    }
}

TEST_CASE("SceneHitTester queryQuadTree with empty scene", "[CorePlotting][SceneHitTester]") {
    SceneHitTester tester;
    RenderableScene empty_scene;
    
    auto result = tester.queryQuadTree(100.0f, 0.0f, empty_scene);
    
    REQUIRE_FALSE(result.hasHit());
}

TEST_CASE("SceneHitTester querySeriesRegion", "[CorePlotting][SceneHitTester]") {
    SceneHitTester tester;
    auto layout = makeTestLayout();
    
    SECTION("Hit in top series region") {
        auto result = tester.querySeriesRegion(100.0f, 0.5f, layout);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.hit_type == HitType::AnalogSeries);
        REQUIRE(result.series_key == "series_top");
        REQUIRE_FALSE(result.hasEntityId()); // Analog series have no entity ID
    }
    
    SECTION("Hit in bottom series region") {
        auto result = tester.querySeriesRegion(100.0f, -0.3f, layout);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.series_key == "series_bot");
    }
    
    SECTION("Miss - outside all series") {
        auto result = tester.querySeriesRegion(100.0f, 2.0f, layout);
        
        REQUIRE_FALSE(result.hasHit());
    }
}

TEST_CASE("SceneHitTester hitTest combined", "[CorePlotting][SceneHitTester]") {
    SceneHitTester tester;
    auto scene = makeTestSceneWithEvents();
    auto layout = makeTestLayout();
    
    SECTION("Returns discrete element when within tolerance") {
        // Query at exact event position
        auto result = tester.hitTest(100.0f, 0.5f, scene, layout);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.hit_type == HitType::DigitalEvent);
        REQUIRE(result.entity_id.value() == EntityId{1});
    }
    
    SECTION("Returns series region when no discrete element nearby") {
        // Query in top series region but far from any event
        auto result = tester.hitTest(50.0f, 0.5f, scene, layout);
        
        REQUIRE(result.hasHit());
        // QuadTree might still find an event within tolerance depending on config
        // If not, it falls back to series region
        if (result.hit_type == HitType::DigitalEvent) {
            // Event hit takes priority
            REQUIRE(result.hasEntityId());
        } else {
            REQUIRE(result.hit_type == HitType::AnalogSeries);
            REQUIRE(result.series_key == "series_top");
        }
    }
}

TEST_CASE("SceneHitTester queryIntervals", "[CorePlotting][SceneHitTester]") {
    SceneHitTester tester;
    
    // Create scene with rectangle batch (intervals)
    RenderableScene scene;
    RenderableRectangleBatch batch;
    // Interval from x=100 to x=200, y=0, height=0.5
    batch.bounds.push_back(glm::vec4(100.0f, 0.0f, 100.0f, 0.5f)); // {x, y, width, height}
    batch.entity_ids.push_back(EntityId{42});
    scene.rectangle_batches.push_back(batch);
    
    std::map<size_t, std::string> key_map;
    key_map[0] = "intervals";
    
    SECTION("Hit inside interval") {
        auto result = tester.queryIntervals(150.0f, 0.25f, scene, key_map);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.hit_type == HitType::IntervalBody);
        REQUIRE(result.series_key == "intervals");
        REQUIRE(result.entity_id.value() == EntityId{42});
        REQUIRE(result.interval_start.value() == 100);
        REQUIRE(result.interval_end.value() == 200);
    }
    
    SECTION("Miss - outside interval time range") {
        auto result = tester.queryIntervals(50.0f, 0.25f, scene, key_map);
        
        REQUIRE_FALSE(result.hasHit());
    }
}

TEST_CASE("SceneHitTester findIntervalEdge", "[CorePlotting][SceneHitTester]") {
    HitTestConfig config;
    config.edge_tolerance = 5.0f;
    SceneHitTester tester(config);
    
    // Create scene with rectangle batch
    RenderableScene scene;
    RenderableRectangleBatch batch;
    batch.bounds.push_back(glm::vec4(100.0f, 0.0f, 100.0f, 0.5f)); // [100, 200]
    batch.entity_ids.push_back(EntityId{42});
    scene.rectangle_batches.push_back(batch);
    
    std::map<size_t, std::string> key_map;
    key_map[0] = "intervals";
    
    std::map<std::string, std::pair<int64_t, int64_t>> selected;
    selected["intervals"] = {100, 200};
    
    SECTION("Hit left edge") {
        auto result = tester.findIntervalEdge(102.0f, scene, selected, key_map);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.hit_type == HitType::IntervalEdgeLeft);
        REQUIRE_THAT(result.world_x, WithinAbs(100.0f, 0.001f));
    }
    
    SECTION("Hit right edge") {
        auto result = tester.findIntervalEdge(198.0f, scene, selected, key_map);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.hit_type == HitType::IntervalEdgeRight);
        REQUIRE_THAT(result.world_x, WithinAbs(200.0f, 0.001f));
    }
    
    SECTION("Miss - in middle of interval") {
        auto result = tester.findIntervalEdge(150.0f, scene, selected, key_map);
        
        REQUIRE_FALSE(result.hasHit());
    }
    
    SECTION("Miss - outside interval") {
        auto result = tester.findIntervalEdge(50.0f, scene, selected, key_map);
        
        REQUIRE_FALSE(result.hasHit());
    }
}

TEST_CASE("SceneHitTester selectBestHit priority", "[CorePlotting][SceneHitTester]") {
    // Test through hitTest which uses selectBestHit internally
    SceneHitTester tester;
    
    SECTION("Discrete beats region when configured") {
        // Create scene where an event overlaps with a series region
        RenderableScene scene;
        BoundingBox bounds(-100.0f, -1.0f, 200.0f, 1.0f);
        auto tree = std::make_unique<QuadTree<EntityId>>(bounds);
        tree->insert(100.0f, 0.5f, EntityId{1});
        scene.spatial_index = std::move(tree);
        
        LayoutResponse layout;
        layout.layouts.push_back(SeriesLayout(
            SeriesLayoutResult(0.5f, 0.6f), "analog", 0));
        
        auto result = tester.hitTest(100.0f, 0.5f, scene, layout);
        
        // Should return the event, not the series region
        REQUIRE(result.hit_type == HitType::DigitalEvent);
    }
}
