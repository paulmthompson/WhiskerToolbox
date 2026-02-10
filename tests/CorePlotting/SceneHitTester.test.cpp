#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/Interaction/SceneHitTester.hpp"
#include "CorePlotting/Layout/LayoutTransform.hpp"
#include "CoreGeometry/boundingbox.hpp"
#include "SpatialIndex/QuadTree.hpp"

using namespace CorePlotting;
using Catch::Matchers::WithinAbs;

namespace {

// Helper to create a simple scene with events in a QuadTree
RenderableScene makeTestSceneWithEvents() {
    RenderableScene scene;
    
    // Create QuadTree with some events using BoundingBox constructor
    // Bounds must contain all points to be inserted (precondition of insert)
    BoundingBox bounds(-100.0f, -1.0f, 400.0f, 1.0f);
    auto tree = std::make_unique<QuadTree<EntityId>>(bounds);
    tree->insert(100.0f, 0.5f, EntityId{1});  // Event at time 100, y=0.5
    tree->insert(200.0f, 0.5f, EntityId{2});  // Event at time 200, y=0.5
    tree->insert(300.0f, -0.3f, EntityId{3}); // Event at time 300, y=-0.3
    
    scene.spatial_index = std::move(tree);
    scene.view_matrix = glm::mat4(1.0f);
    scene.projection_matrix = glm::mat4(1.0f);
    
    return scene;
}

// Helper to create a simple layout
LayoutResponse makeTestLayout() {
    LayoutResponse layout;
    
    // Two series: one at y=0.5, one at y=-0.3
    // y_transform: offset=center, gain=half_height
    layout.layouts.push_back(SeriesLayout(
        "series_top", LayoutTransform(0.5f, 0.3f), 0));  // center=0.5, half_height=0.3
    layout.layouts.push_back(SeriesLayout(
        "series_bot", LayoutTransform(-0.3f, 0.3f), 1)); // center=-0.3, half_height=0.3
    
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

TEST_CASE("SceneHitTester queryQuadTree with entity_to_series_key mapping", "[CorePlotting][SceneHitTester]") {
    SceneHitTester tester;
    
    // Create a scene with events AND entity_to_series_key mapping
    RenderableScene scene;
    BoundingBox bounds(-100.0f, -1.0f, 400.0f, 1.0f);
    auto tree = std::make_unique<QuadTree<EntityId>>(bounds);
    
    // Insert events from two different "trials"
    tree->insert(100.0f, 0.5f, EntityId{1});  // trial_0
    tree->insert(200.0f, 0.2f, EntityId{2});  // trial_0
    tree->insert(150.0f, -0.3f, EntityId{3}); // trial_1
    
    scene.spatial_index = std::move(tree);
    
    // Add entity → series_key mappings
    scene.entity_to_series_key[EntityId{1}] = "trial_0";
    scene.entity_to_series_key[EntityId{2}] = "trial_0";
    scene.entity_to_series_key[EntityId{3}] = "trial_1";
    
    SECTION("Returns correct series_key for trial_0 event") {
        auto result = tester.queryQuadTree(101.0f, 0.5f, scene);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.hit_type == HitType::DigitalEvent);
        REQUIRE(result.entity_id.value() == EntityId{1});
        REQUIRE(result.series_key == "trial_0");
    }
    
    SECTION("Returns correct series_key for trial_1 event") {
        auto result = tester.queryQuadTree(151.0f, -0.3f, scene);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.hit_type == HitType::DigitalEvent);
        REQUIRE(result.entity_id.value() == EntityId{3});
        REQUIRE(result.series_key == "trial_1");
    }
    
    SECTION("Returns empty series_key when mapping is missing") {
        // Create scene without the mapping
        RenderableScene scene_no_mapping;
        BoundingBox bounds2(-100.0f, -1.0f, 400.0f, 1.0f);
        auto tree2 = std::make_unique<QuadTree<EntityId>>(bounds2);
        tree2->insert(100.0f, 0.5f, EntityId{99});
        scene_no_mapping.spatial_index = std::move(tree2);
        
        auto result = tester.queryQuadTree(100.0f, 0.5f, scene_no_mapping);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.entity_id.value() == EntityId{99});
        REQUIRE(result.series_key.empty()); // No mapping, so empty
    }
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
            "analog", LayoutTransform(0.5f, 0.3f), 0));  // center=0.5, half_height=0.3
        
        auto result = tester.hitTest(100.0f, 0.5f, scene, layout);
        
        // Should return the event, not the series region
        REQUIRE(result.hit_type == HitType::DigitalEvent);
    }
}

// ===========================================================================
// EventPlotWidget Click Selection Tests
// ===========================================================================
// These tests simulate the logic in EventPlotOpenGLWidget::handleClickSelection
// which uses SceneHitTester to find events when the user clicks on the raster plot.

namespace {

/**
 * @brief Helper to create a raster plot scene similar to EventPlotWidget
 * 
 * Creates a scene with events organized by trials (like EventPlotWidget does).
 * Each trial has its own Y position and events at various X (time) positions.
 * 
 * @param num_trials Number of trials (rows) in the raster plot
 * @param events_per_trial Number of events in each trial
 * @param time_range Range of time values for events (relative to alignment)
 * @return RenderableScene with spatial index and entity_to_series_key mapping
 */
RenderableScene makeRasterPlotScene(
    int num_trials,
    int events_per_trial,
    float time_range_min = -500.0f,
    float time_range_max = 500.0f) {
    
    RenderableScene scene;
    
    // Create QuadTree with bounds matching typical raster plot
    BoundingBox bounds(time_range_min, -1.0f, time_range_max - time_range_min, 2.0f);
    auto tree = std::make_unique<QuadTree<EntityId>>(bounds);
    
    uint64_t entity_counter = 1;
    float y_spacing = 2.0f / static_cast<float>(num_trials);
    
    for (int trial = 0; trial < num_trials; ++trial) {
        // Y position for this trial (from -1 to +1)
        float trial_y = -1.0f + y_spacing * (static_cast<float>(trial) + 0.5f);
        std::string series_key = "trial_" + std::to_string(trial);
        
        for (int event = 0; event < events_per_trial; ++event) {
            // Spread events across the time range
            float event_time = time_range_min + 
                (time_range_max - time_range_min) * 
                static_cast<float>(event) / static_cast<float>(events_per_trial);
            
            EntityId entity_id{entity_counter++};
            tree->insert(event_time, trial_y, entity_id);
            scene.entity_to_series_key[entity_id] = series_key;
        }
    }
    
    scene.spatial_index = std::move(tree);
    scene.view_matrix = glm::mat4(1.0f);
    scene.projection_matrix = glm::mat4(1.0f);
    
    return scene;
}

/**
 * @brief Extract trial index from series_key (format: "trial_N")
 * 
 * This mirrors the logic in EventPlotOpenGLWidget::handleClickSelection
 */
std::optional<int> extractTrialIndex(std::string const & series_key) {
    if (series_key.starts_with("trial_")) {
        try {
            return std::stoi(series_key.substr(6));
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

} // anonymous namespace

TEST_CASE("EventPlotWidget click selection simulation", "[CorePlotting][SceneHitTester][EventPlotWidget]") {
    // Create a raster plot scene with 10 trials, each with 5 events
    auto scene = makeRasterPlotScene(10, 5, -500.0f, 500.0f);
    
    // Configure hit tester similar to EventPlotWidget
    // (typical tolerance for 1000px wide widget with ±500ms view)
    HitTestConfig config;
    config.point_tolerance = 10.0f;  // ~10 pixels in world units
    config.prioritize_discrete = true;
    SceneHitTester tester(config);
    
    // Create layout that matches the scene
    LayoutResponse layout;
    for (int i = 0; i < 10; ++i) {
        float center_y = -1.0f + 0.2f * (static_cast<float>(i) + 0.5f);
        layout.layouts.push_back(SeriesLayout(
            "trial_" + std::to_string(i),
            LayoutTransform(center_y, 0.1f),
            i));
    }
    
    SECTION("Click near event in trial_0 returns correct trial index") {
        // First event in trial_0 is at approximately (-500, -0.9)
        float click_x = -495.0f;  // Near the first event
        float click_y = -0.9f;    // Trial 0 Y position
        
        auto result = tester.hitTest(click_x, click_y, scene, layout);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.hit_type == HitType::DigitalEvent);
        REQUIRE(result.series_key.starts_with("trial_"));
        
        auto trial_index = extractTrialIndex(result.series_key);
        REQUIRE(trial_index.has_value());
        REQUIRE(trial_index.value() == 0);
    }
    
    SECTION("Click near event in trial_5 returns correct trial index") {
        // Trial 5 center Y is approximately 0.1
        float click_x = -495.0f;  // Near first event
        float click_y = 0.1f;     // Trial 5 Y position
        
        auto result = tester.hitTest(click_x, click_y, scene, layout);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.hit_type == HitType::DigitalEvent);
        
        auto trial_index = extractTrialIndex(result.series_key);
        REQUIRE(trial_index.has_value());
        REQUIRE(trial_index.value() == 5);
    }
    
    SECTION("Click far from any events returns no hit") {
        // Click in empty space between events (with small tolerance)
        HitTestConfig tight_config;
        tight_config.point_tolerance = 1.0f;  // Very tight tolerance
        tight_config.prioritize_discrete = true;
        SceneHitTester tight_tester(tight_config);
        
        // Click midway between events where nothing exists
        float click_x = 1000.0f;  // Way outside event range
        float click_y = 0.0f;
        
        auto result = tight_tester.hitTest(click_x, click_y, scene, layout);
        
        // Should either not hit or hit a series region (not an event)
        if (result.hasHit()) {
            REQUIRE(result.hit_type != HitType::DigitalEvent);
        }
    }
    
    SECTION("Click returns world coordinates of the actual event") {
        // Click near first event in trial_0 (within tolerance)
        float click_x = -495.0f;  // Within 10 units of event at -500
        float click_y = -0.9f;
        
        auto result = tester.hitTest(click_x, click_y, scene, layout);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.hit_type == HitType::DigitalEvent);
        
        // The world_x should be the actual event time, not the click position
        // First event is at time_range_min (-500)
        REQUIRE_THAT(result.world_x, WithinAbs(-500.0f, 1.0f));
    }
    
    SECTION("Click on different events in same trial returns correct times") {
        // Trial 0, event 0 is at -500
        // Trial 0, event 2 is at -500 + 1000 * 2/5 = -100
        
        float click_y = -0.9f;  // Trial 0 Y position
        
        // Click near first event
        auto result1 = tester.hitTest(-495.0f, click_y, scene, layout);
        REQUIRE(result1.hasHit());
        REQUIRE_THAT(result1.world_x, WithinAbs(-500.0f, 10.0f));
        
        // Click near third event (index 2)
        auto result2 = tester.hitTest(-105.0f, click_y, scene, layout);
        REQUIRE(result2.hasHit());
        REQUIRE_THAT(result2.world_x, WithinAbs(-100.0f, 10.0f));
    }
}

TEST_CASE("EventPlotWidget click selection with overlapping trials", "[CorePlotting][SceneHitTester][EventPlotWidget]") {
    // Test scenario where events from different trials are close together
    // (like when zoomed out and many trials overlap)
    
    RenderableScene scene;
    BoundingBox bounds(-100.0f, -1.0f, 200.0f, 2.0f);
    auto tree = std::make_unique<QuadTree<EntityId>>(bounds);
    
    // Two events at same X but different Y (different trials)
    tree->insert(50.0f, 0.8f, EntityId{1});   // trial_0 event at Y=0.8
    tree->insert(50.0f, -0.8f, EntityId{2});  // trial_1 event at Y=-0.8
    
    scene.spatial_index = std::move(tree);
    scene.entity_to_series_key[EntityId{1}] = "trial_0";
    scene.entity_to_series_key[EntityId{2}] = "trial_1";
    
    HitTestConfig config;
    config.point_tolerance = 20.0f;
    config.prioritize_discrete = true;
    SceneHitTester tester(config);
    
    LayoutResponse layout;
    layout.layouts.push_back(SeriesLayout("trial_0", LayoutTransform(0.8f, 0.2f), 0));
    layout.layouts.push_back(SeriesLayout("trial_1", LayoutTransform(-0.8f, 0.2f), 1));
    
    SECTION("Click near trial_0 event selects trial_0") {
        auto result = tester.hitTest(50.0f, 0.75f, scene, layout);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.hit_type == HitType::DigitalEvent);
        REQUIRE(result.series_key == "trial_0");
        
        auto trial_index = extractTrialIndex(result.series_key);
        REQUIRE(trial_index.value() == 0);
    }
    
    SECTION("Click near trial_1 event selects trial_1") {
        auto result = tester.hitTest(50.0f, -0.75f, scene, layout);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.hit_type == HitType::DigitalEvent);
        REQUIRE(result.series_key == "trial_1");
        
        auto trial_index = extractTrialIndex(result.series_key);
        REQUIRE(trial_index.value() == 1);
    }
    
    SECTION("Click between trials selects nearest") {
        // Click at Y=0.0 which is equidistant from both events
        auto result = tester.hitTest(50.0f, 0.0f, scene, layout);
        
        REQUIRE(result.hasHit());
        REQUIRE(result.hit_type == HitType::DigitalEvent);
        // The nearest neighbor algorithm should pick one
        REQUIRE_FALSE(result.series_key.empty());
    }
}

TEST_CASE("EventPlotWidget extractTrialIndex helper", "[CorePlotting][EventPlotWidget]") {
    SECTION("Valid trial_N format") {
        REQUIRE(extractTrialIndex("trial_0").value() == 0);
        REQUIRE(extractTrialIndex("trial_5").value() == 5);
        REQUIRE(extractTrialIndex("trial_42").value() == 42);
        REQUIRE(extractTrialIndex("trial_999").value() == 999);
    }
    
    SECTION("Invalid formats return nullopt") {
        REQUIRE_FALSE(extractTrialIndex("").has_value());
        REQUIRE_FALSE(extractTrialIndex("trial").has_value());
        REQUIRE_FALSE(extractTrialIndex("trial_").has_value());
        REQUIRE_FALSE(extractTrialIndex("TRIAL_5").has_value());
        REQUIRE_FALSE(extractTrialIndex("series_0").has_value());
        REQUIRE_FALSE(extractTrialIndex("trial_abc").has_value());
    }
}
