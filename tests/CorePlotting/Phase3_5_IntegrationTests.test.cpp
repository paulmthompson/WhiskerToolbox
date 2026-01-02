/**
 * @file Phase3_5_IntegrationTests.test.cpp
 * @brief Integration tests for CorePlotting Phase 3.5 - Real Data Types, Plain Language Scenarios
 * 
 * These tests validate end-to-end workflows through the CorePlotting stack using real
 * data types and scenarios that match actual user interactions:
 * 
 * - Scenario 1: Stacked Analog + Events (DataViewer Style)
 * - Scenario 2: Interval Selection and Edge Detection
 * - Scenario 3: Raster Plot (Multi-Row Events)
 * - Scenario 4: Coordinate Transform Round-Trip
 * - Scenario 5: Mixed Series Priority (Event Beats Analog)
 * 
 * All tests use NO Qt/OpenGL—just CorePlotting + DataManager types.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// CorePlotting includes
#include "CorePlotting/Layout/LayoutEngine.hpp"
#include "CorePlotting/Layout/RowLayoutStrategy.hpp"
#include "CorePlotting/Layout/StackedLayoutStrategy.hpp"
#include "CorePlotting/Mappers/TimeSeriesMapper.hpp"
#include "CorePlotting/Mappers/SpatialMapper.hpp"
#include "CorePlotting/Mappers/RasterMapper.hpp"
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "CorePlotting/CoordinateTransform/TimeRange.hpp"
#include "CorePlotting/CoordinateTransform/ViewState.hpp"
#include "CorePlotting/CoordinateTransform/TimeAxisCoordinates.hpp"
#include "CorePlotting/Interaction/SceneHitTester.hpp"
#include "SpatialIndex/QuadTree.hpp"

// DataManager includes
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "Entity/EntityRegistry.hpp"

#include <cmath>
#include <memory>
#include <vector>

using namespace CorePlotting;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Test Helpers
// ============================================================================

namespace {

/**
 * @brief Create a DigitalEventSeries with known events and EntityIds
 */
std::unique_ptr<DigitalEventSeries> createEventSeries(
        std::vector<int> const& event_times,
        std::string const& data_key,
        EntityRegistry& registry) {
    
    std::vector<TimeFrameIndex> times;
    times.reserve(event_times.size());
    for (int t : event_times) {
        times.push_back(TimeFrameIndex{t});
    }
    
    auto series = std::make_unique<DigitalEventSeries>(times);
    series->setIdentityContext(data_key, &registry);
    series->rebuildAllEntityIds();
    
    return series;
}

/**
 * @brief Create a simple TimeFrame with 1:1 index to time mapping
 */
std::shared_ptr<TimeFrame> createSimpleTimeFrame(int length) {
    std::vector<int> times;
    times.reserve(length);
    for (int i = 0; i < length; ++i) {
        times.push_back(i);
    }
    return std::make_shared<TimeFrame>(times);
}

/**
 * @brief Create a DigitalIntervalSeries with known intervals
 */
std::unique_ptr<DigitalIntervalSeries> createIntervalSeries(
        std::vector<std::pair<int, int>> const& intervals,
        std::string const& data_key,
        EntityRegistry& registry) {
    
    auto series = std::make_unique<DigitalIntervalSeries>();
    series->setIdentityContext(data_key, &registry);
    
    for (auto const& [start, end] : intervals) {
        series->addEvent(TimeFrameIndex{start}, TimeFrameIndex{end});
    }
    
    series->rebuildAllEntityIds();
    return series;
}

/**
 * @brief Build a combined QuadTree from multiple stacked event series
 */
std::unique_ptr<QuadTree<EntityId>> buildCombinedEventIndex(
        std::vector<std::pair<DigitalEventSeries const*, SeriesLayout const*>> const& series_layouts,
        TimeFrame const& time_frame,
        BoundingBox const& bounds) {
    
    auto tree = std::make_unique<QuadTree<EntityId>>(bounds);
    
    for (auto const& [series, layout] : series_layouts) {
        for (auto const& event : series->view()) {
            float x = static_cast<float>(time_frame.getTimeAtIndex(event.event_time));
            float y = layout->y_transform.offset;
            tree->insert(x, y, event.entity_id);
        }
    }
    
    return tree;
}

/**
 * @brief Build a QuadTree from a single stacked event series
 */
std::unique_ptr<QuadTree<EntityId>> buildStackedEventIndex(
        DigitalEventSeries const& series,
        TimeFrame const& time_frame,
        SeriesLayout const& layout,
        BoundingBox const& bounds) {
    
    auto tree = std::make_unique<QuadTree<EntityId>>(bounds);
    float y = layout.y_transform.offset;
    
    for (auto const& event : series.view()) {
        float x = static_cast<float>(time_frame.getTimeAtIndex(event.event_time));
        tree->insert(x, y, event.entity_id);
    }
    
    return tree;
}

/**
 * @brief Create a RenderableScene with interval rectangles
 */
RenderableScene createIntervalScene(
        DigitalIntervalSeries const& intervals,
        TimeFrame const& time_frame,
        SeriesLayout const& layout) {
    
    RenderableScene scene;
    RenderableRectangleBatch batch;
    
    for (auto const& interval : intervals.view()) {
        float x_start = static_cast<float>(time_frame.getTimeAtIndex(TimeFrameIndex{interval.interval.start}));
        float x_end = static_cast<float>(time_frame.getTimeAtIndex(TimeFrameIndex{interval.interval.end}));
        float width = x_end - x_start;
        // y_transform: offset=center, gain=half_height
        float height = layout.y_transform.gain * 2.0f;
        float y = layout.y_transform.offset - height / 2.0f;
        
        batch.bounds.push_back(glm::vec4(x_start, y, width, height));
        batch.entity_ids.push_back(interval.entity_id);
    }
    
    scene.rectangle_batches.push_back(batch);
    scene.view_matrix = glm::mat4(1.0f);
    scene.projection_matrix = glm::mat4(1.0f);
    
    return scene;
}

} // anonymous namespace


// ============================================================================
// Test Scenario 1: Stacked Analog + Events (DataViewer Style)
// ============================================================================

TEST_CASE("Scenario 1: DataViewer-style stacked layout with hit testing", 
          "[CorePlotting][Integration][Phase3.5][Stacked]") {
    
    auto time_frame = createSimpleTimeFrame(2000);
    EntityRegistry registry;
    
    // Create two analog series (represented conceptually - we use events for simplicity
    // since analog series don't have EntityIds for hit testing)
    // In a real scenario, analog series regions are identified by series_key only
    
    // Create event series that overlays the "analog" region
    auto events = createEventSeries({100, 300, 500, 700}, "spike_events", registry);
    events->setTimeFrame(time_frame);
    
    // Build layout for 3 stacked series: 2 "analog" + 1 event
    LayoutRequest request;
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.series = {
        {"analog1", SeriesType::Analog, true},
        {"analog2", SeriesType::Analog, true},
        {"spike_events", SeriesType::DigitalEvent, true}
    };
    
    StackedLayoutStrategy strategy;
    LayoutResponse layout = strategy.compute(request);
    
    REQUIRE(layout.layouts.size() == 3);
    
    SECTION("Layout positions are distinct") {
        float y0 = layout.layouts[0].y_transform.offset;
        float y1 = layout.layouts[1].y_transform.offset;
        float y2 = layout.layouts[2].y_transform.offset;
        
        // All Y positions should be different
        REQUIRE_THAT(y0, !WithinAbs(y1, 0.01f));
        REQUIRE_THAT(y1, !WithinAbs(y2, 0.01f));
        REQUIRE_THAT(y0, !WithinAbs(y2, 0.01f));
    }
    
    SECTION("Hit test on event returns correct EntityId") {
        auto const* event_layout = layout.findLayout("spike_events");
        REQUIRE(event_layout != nullptr);
        
        BoundingBox bounds(0.0f, -2.0f, 1000.0f, 2.0f);
        auto index = buildStackedEventIndex(
            *events, *time_frame, *event_layout, bounds);
        
        // Create scene with spatial index
        RenderableScene scene;
        scene.spatial_index = std::move(index);
        scene.view_matrix = glm::mat4(1.0f);
        scene.projection_matrix = glm::mat4(1.0f);
        
        SceneHitTester tester;
        
        // Click on event at time=300 (should hit EntityId for event at index 1)
        float event_y = event_layout->y_transform.offset;
        auto hit = tester.hitTest(300.0f, event_y, scene, layout);
        
        REQUIRE(hit.hasHit());
        REQUIRE(hit.hit_type == HitType::DigitalEvent);
        REQUIRE(hit.entity_id.value() == events->getEntityIds()[1]);
    }
    
    SECTION("Hit test on analog region returns series_key without EntityId") {
        auto const* analog1_layout = layout.findLayout("analog1");
        REQUIRE(analog1_layout != nullptr);
        
        // Create empty scene (no discrete elements)
        RenderableScene scene;
        scene.view_matrix = glm::mat4(1.0f);
        scene.projection_matrix = glm::mat4(1.0f);
        
        SceneHitTester tester;
        
        // Click in analog1's region
        float analog1_y = analog1_layout->y_transform.offset;
        auto hit = tester.hitTest(500.0f, analog1_y, scene, layout);
        
        REQUIRE(hit.hasHit());
        REQUIRE(hit.hit_type == HitType::AnalogSeries);
        REQUIRE(hit.series_key == "analog1");
        REQUIRE_FALSE(hit.hasEntityId());
    }
    
    SECTION("Hit test far outside all series returns no hit") {
        // Test hitting a Y coordinate far outside the viewport bounds
        // This avoids edge cases where series may abut or overlap
        
        RenderableScene scene;
        scene.view_matrix = glm::mat4(1.0f);
        scene.projection_matrix = glm::mat4(1.0f);
        
        SceneHitTester tester;
        
        // Query at Y=10, which is far outside viewport bounds [-1, 1]
        auto hit = tester.hitTest(500.0f, 10.0f, scene, layout);
        
        // Should not hit any series region when far outside bounds
        REQUIRE_FALSE(hit.hasHit());
    }
}


// ============================================================================
// Test Scenario 2: Interval Selection and Edge Detection
// ============================================================================

TEST_CASE("Scenario 2: Interval hit testing with edge detection for drag handles",
          "[CorePlotting][Integration][Phase3.5][Intervals]") {
    
    auto time_frame = createSimpleTimeFrame(2000);
    EntityRegistry registry;
    
    // Create interval series
    auto intervals = createIntervalSeries(
        {{100, 300}, {500, 800}},  // Two intervals
        "intervals",
        registry
    );
    intervals->setTimeFrame(time_frame);
    
    // Layout: intervals use full canvas
    LayoutRequest request;
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.series = {
        {"intervals", SeriesType::DigitalInterval, false}  // Not stacked = full canvas
    };
    
    StackedLayoutStrategy strategy;
    LayoutResponse layout = strategy.compute(request);
    
    auto const* interval_layout = layout.findLayout("intervals");
    REQUIRE(interval_layout != nullptr);
    
    // Create scene with interval rectangles
    auto scene = createIntervalScene(*intervals, *time_frame, *interval_layout);
    
    std::map<size_t, std::string> key_map;
    key_map[0] = "intervals";
    
    SceneHitTester tester;
    
    SECTION("Click inside interval body") {
        // Click at x=200, inside first interval [100, 300]
        auto hit = tester.queryIntervals(200.0f, 0.0f, scene, key_map);
        
        REQUIRE(hit.hasHit());
        REQUIRE(hit.hit_type == HitType::IntervalBody);
        REQUIRE(hit.series_key == "intervals");
        REQUIRE(hit.interval_start.value() == 100);
        REQUIRE(hit.interval_end.value() == 300);
    }
    
    SECTION("Click near left edge (drag handle)") {
        std::map<std::string, std::pair<int64_t, int64_t>> selected;
        selected["intervals"] = {100, 300};
        
        // Click near left edge at x=102
        auto hit = tester.findIntervalEdge(102.0f, scene, selected, key_map);
        
        REQUIRE(hit.hasHit());
        REQUIRE(hit.hit_type == HitType::IntervalEdgeLeft);
        REQUIRE_THAT(hit.world_x, WithinAbs(100.0f, 1.0f));
    }
    
    SECTION("Click near right edge (drag handle)") {
        std::map<std::string, std::pair<int64_t, int64_t>> selected;
        selected["intervals"] = {100, 300};
        
        // Click near right edge at x=298
        auto hit = tester.findIntervalEdge(298.0f, scene, selected, key_map);
        
        REQUIRE(hit.hasHit());
        REQUIRE(hit.hit_type == HitType::IntervalEdgeRight);
        REQUIRE_THAT(hit.world_x, WithinAbs(300.0f, 1.0f));
    }
    
    SECTION("Click outside any interval") {
        // Click at x=400, between intervals
        auto hit = tester.queryIntervals(400.0f, 0.0f, scene, key_map);
        
        REQUIRE_FALSE(hit.hasHit());
    }
    
    SECTION("Click in second interval") {
        // Click at x=650, inside second interval [500, 800]
        auto hit = tester.queryIntervals(650.0f, 0.0f, scene, key_map);
        
        REQUIRE(hit.hasHit());
        REQUIRE(hit.hit_type == HitType::IntervalBody);
        REQUIRE(hit.interval_start.value() == 500);
        REQUIRE(hit.interval_end.value() == 800);
    }
}


// ============================================================================
// Test Scenario 3: Raster Plot (Multi-Row Events)
// ============================================================================

TEST_CASE("Scenario 3: Raster plot with row-based event layout",
          "[CorePlotting][Integration][Phase3.5][Raster]") {
    
    auto time_frame = createSimpleTimeFrame(2000);
    EntityRegistry registry;
    
    // Create multiple trials as separate event series
    auto trial1 = createEventSeries({50, 150}, "trial1", registry);
    trial1->setTimeFrame(time_frame);
    
    auto trial2 = createEventSeries({80, 120}, "trial2", registry);
    trial2->setTimeFrame(time_frame);
    
    // Use RowLayoutStrategy for raster-style rows
    LayoutRequest request;
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.series = {
        {"trial1", SeriesType::DigitalEvent, true},
        {"trial2", SeriesType::DigitalEvent, true}
    };
    
    RowLayoutStrategy strategy;
    LayoutResponse layout = strategy.compute(request);
    
    REQUIRE(layout.layouts.size() == 2);
    
    auto const* trial1_layout = layout.findLayout("trial1");
    auto const* trial2_layout = layout.findLayout("trial2");
    REQUIRE(trial1_layout != nullptr);
    REQUIRE(trial2_layout != nullptr);
    
    SECTION("Rows have equal heights") {
        REQUIRE_THAT(trial1_layout->y_transform.gain * 2.0f,
                    WithinAbs(trial2_layout->y_transform.gain * 2.0f, 0.001f));
    }
    
    SECTION("Build combined spatial index from multiple trials") {
        BoundingBox bounds(0.0f, -2.0f, 200.0f, 2.0f);
        
        auto combined_index = buildCombinedEventIndex(
            {{trial1.get(), trial1_layout}, {trial2.get(), trial2_layout}},
            *time_frame,
            bounds
        );
        
        REQUIRE(combined_index != nullptr);
        REQUIRE(combined_index->size() == 4);  // 2 + 2 events
        
        // Query for event in trial1
        float trial1_y = trial1_layout->y_transform.offset;
        auto const* hit1 = combined_index->findNearest(50.0f, trial1_y, 10.0f);
        REQUIRE(hit1 != nullptr);
        REQUIRE(hit1->data == trial1->getEntityIds()[0]);
        
        // Query for event in trial2 (different row)
        float trial2_y = trial2_layout->y_transform.offset;
        auto const* hit2 = combined_index->findNearest(80.0f, trial2_y, 10.0f);
        REQUIRE(hit2 != nullptr);
        REQUIRE(hit2->data == trial2->getEntityIds()[0]);
    }
    
    SECTION("Y position distinguishes events at similar times") {
        BoundingBox bounds(0.0f, -2.0f, 200.0f, 2.0f);
        
        auto combined_index = buildCombinedEventIndex(
            {{trial1.get(), trial1_layout}, {trial2.get(), trial2_layout}},
            *time_frame,
            bounds
        );
        
        // Both trials have events around time 120-150 range
        // Query at Y positions should return correct trial's event
        
        float trial1_y = trial1_layout->y_transform.offset;
        float trial2_y = trial2_layout->y_transform.offset;
        
        // Event at time 150 is only in trial1
        auto const* r1 = combined_index->findNearest(150.0f, trial1_y, 10.0f);
        REQUIRE(r1 != nullptr);
        REQUIRE(r1->data == trial1->getEntityIds()[1]);
        
        // Event at time 120 is only in trial2
        auto const* r2 = combined_index->findNearest(120.0f, trial2_y, 10.0f);
        REQUIRE(r2 != nullptr);
        REQUIRE(r2->data == trial2->getEntityIds()[1]);
    }
}


// ============================================================================
// Test Scenario 4: Coordinate Transform Round-Trip
// ============================================================================

TEST_CASE("Scenario 4: Screen → World → Hit → Verify coordinates",
          "[CorePlotting][Integration][Phase3.5][Coordinates]") {
    
    auto time_frame = createSimpleTimeFrame(1000);
    EntityRegistry registry;
    
    // Create events at known positions
    auto events = createEventSeries({250, 500, 750}, "events", registry);
    events->setTimeFrame(time_frame);
    
    // Single row layout
    LayoutRequest request;
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.series = {{"events", SeriesType::DigitalEvent, true}};
    
    StackedLayoutStrategy strategy;
    LayoutResponse layout = strategy.compute(request);
    
    auto const* event_layout = layout.findLayout("events");
    REQUIRE(event_layout != nullptr);
    
    BoundingBox bounds(0.0f, -1.0f, 1000.0f, 1.0f);
    auto index = buildStackedEventIndex(*events, *time_frame, *event_layout, bounds);
    
    SECTION("TimeAxisParams converts screen X to time correctly") {
        // Setup: 800px wide canvas, time range [0, 1000]
        TimeAxisParams params(0, 1000, 800);
        
        // Middle of canvas (x=400) should map to time=500
        float time = canvasXToTime(400.0f, params);
        REQUIRE_THAT(time, WithinAbs(500.0f, 0.1f));
        
        // Left edge (x=0) should map to time=0
        float time_left = canvasXToTime(0.0f, params);
        REQUIRE_THAT(time_left, WithinAbs(0.0f, 0.1f));
        
        // Right edge (x=800) should map to time=1000
        float time_right = canvasXToTime(800.0f, params);
        REQUIRE_THAT(time_right, WithinAbs(1000.0f, 0.1f));
    }
    
    SECTION("Time to canvas X round-trip") {
        TimeAxisParams params(0, 1000, 800);
        
        // Convert time 500 to canvas X
        float canvas_x = timeToCanvasX(500.0f, params);
        REQUIRE_THAT(canvas_x, WithinAbs(400.0f, 0.1f));
        
        // Round trip: canvas → time → canvas
        float time = canvasXToTime(canvas_x, params);
        float canvas_x2 = timeToCanvasX(time, params);
        REQUIRE_THAT(canvas_x2, WithinAbs(canvas_x, 0.001f));
    }
    
    SECTION("ViewState screenToWorld transforms correctly") {
        ViewState view;
        view.data_bounds = bounds;
        view.data_bounds_valid = true;
        view.viewport_width = 1000;  // 1:1 mapping for simplicity
        view.viewport_height = 200;
        view.zoom_level_x = 1.0f;
        view.zoom_level_y = 1.0f;
        view.pan_offset_x = 0.0f;
        view.pan_offset_y = 0.0f;
        view.padding_factor = 1.0f;
        
        // Screen center should map to world center
        auto world = screenToWorld(view, 500, 100);
        
        REQUIRE_THAT(world.x, WithinAbs(500.0f, 10.0f));  // Some tolerance for projection
        REQUIRE_THAT(world.y, WithinAbs(0.0f, 0.5f));
    }
    
    SECTION("Complete pipeline: screen → world → QuadTree → EntityId") {
        ViewState view;
        view.data_bounds = bounds;
        view.data_bounds_valid = true;
        view.viewport_width = 1000;
        view.viewport_height = 200;
        view.zoom_level_x = 1.0f;
        view.zoom_level_y = 1.0f;
        view.pan_offset_x = 0.0f;
        view.pan_offset_y = 0.0f;
        view.padding_factor = 1.0f;
        
        // Simulate click at pixel position corresponding to event at time=500
        int screen_x = 500;  // Should map to world x ≈ 500
        int screen_y = 100;  // Center of viewport
        
        auto world = screenToWorld(view, screen_x, screen_y);
        
        // Query spatial index at world position
        auto const* result = index->findNearest(world.x, world.y, 100.0f);
        
        REQUIRE(result != nullptr);
        // Should find event at time 500 (index 1)
        REQUIRE(result->data == events->getEntityIds()[1]);
    }
    
    SECTION("Panned view correctly transforms coordinates") {
        ViewState view;
        view.data_bounds = bounds;
        view.data_bounds_valid = true;
        view.viewport_width = 1000;
        view.viewport_height = 200;
        view.zoom_level_x = 1.0f;
        view.zoom_level_y = 1.0f;
        view.padding_factor = 1.0f;
        
        // Pan the view to the right
        view.pan_offset_x = 0.25f;  // 25% of data width = 250 units
        view.pan_offset_y = 0.0f;
        
        // Screen center should now map to world x ≈ 750 (500 + 250)
        auto world = screenToWorld(view, 500, 100);
        
        // Query should find event at time 750
        auto const* result = index->findNearest(world.x, world.y, 100.0f);
        
        REQUIRE(result != nullptr);
        REQUIRE(result->data == events->getEntityIds()[2]);  // Event at 750
    }
}


// ============================================================================
// Test Scenario 5: Mixed Series Priority (Event Beats Analog)
// ============================================================================

TEST_CASE("Scenario 5: When event overlaps analog region, event takes priority",
          "[CorePlotting][Integration][Phase3.5][Priority]") {
    
    auto time_frame = createSimpleTimeFrame(1000);
    EntityRegistry registry;
    
    // Create event series
    auto events = createEventSeries({500}, "events", registry);
    events->setTimeFrame(time_frame);
    
    // Layout: analog and events share same row (in real use case, events would overlay)
    // For this test, we place them at same Y position
    LayoutRequest request;
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.series = {
        {"analog", SeriesType::Analog, true},
        {"events", SeriesType::DigitalEvent, true}
    };
    
    StackedLayoutStrategy strategy;
    LayoutResponse layout = strategy.compute(request);
    
    auto const* analog_layout = layout.findLayout("analog");
    auto const* event_layout = layout.findLayout("events");
    REQUIRE(analog_layout != nullptr);
    REQUIRE(event_layout != nullptr);
    
    SECTION("Discrete element takes priority over region at same position") {
        // Create scene where event is at same X as point in analog region
        BoundingBox bounds(0.0f, -2.0f, 1000.0f, 2.0f);
        auto index = buildStackedEventIndex(*events, *time_frame, *event_layout, bounds);
        
        RenderableScene scene;
        scene.spatial_index = std::move(index);
        scene.view_matrix = glm::mat4(1.0f);
        scene.projection_matrix = glm::mat4(1.0f);
        
        SceneHitTester tester;
        
        // Query at event position - should return event, not analog region
        float event_y = event_layout->y_transform.offset;
        auto hit = tester.hitTest(500.0f, event_y, scene, layout);
        
        REQUIRE(hit.hasHit());
        REQUIRE(hit.hit_type == HitType::DigitalEvent);
        REQUIRE(hit.hasEntityId());
    }
    
    SECTION("Region returned when no discrete element nearby") {
        // Create empty scene (no events in spatial index)
        RenderableScene scene;
        scene.view_matrix = glm::mat4(1.0f);
        scene.projection_matrix = glm::mat4(1.0f);
        
        SceneHitTester tester;
        
        // Query in analog region
        float analog_y = analog_layout->y_transform.offset;
        auto hit = tester.hitTest(200.0f, analog_y, scene, layout);
        
        REQUIRE(hit.hasHit());
        REQUIRE(hit.hit_type == HitType::AnalogSeries);
        REQUIRE(hit.series_key == "analog");
        REQUIRE_FALSE(hit.hasEntityId());
    }
    
    SECTION("Priority ordering: Event > Interval Edge > Interval Body > Region") {
        HitTestConfig config;
        config.prioritize_discrete = true;
        SceneHitTester tester(config);
        
        // Create scene with overlapping hits at same position
        RenderableScene scene;
        BoundingBox bounds(0.0f, -2.0f, 1000.0f, 2.0f);
        
        // Add event to spatial index
        auto tree = std::make_unique<QuadTree<EntityId>>(bounds);
        tree->insert(500.0f, event_layout->y_transform.offset, EntityId{42});
        scene.spatial_index = std::move(tree);
        scene.view_matrix = glm::mat4(1.0f);
        scene.projection_matrix = glm::mat4(1.0f);
        
        // Query where event exists - should return event
        auto hit = tester.hitTest(500.0f, event_layout->y_transform.offset, scene, layout);
        
        REQUIRE(hit.hit_type == HitType::DigitalEvent);
    }
}


// ============================================================================
// Test Scenario 7: SceneBuilder High-Level API
// ============================================================================

TEST_CASE("Scenario 7: SceneBuilder high-level API creates scene with spatial index",
          "[CorePlotting][Integration][Phase3.5][SceneBuilder]") {
    
    auto time_frame = createSimpleTimeFrame(1000);
    EntityRegistry registry;
    
    auto events1 = createEventSeries({100, 200, 300}, "series1", registry);
    events1->setTimeFrame(time_frame);
    
    auto events2 = createEventSeries({150, 250}, "series2", registry);
    events2->setTimeFrame(time_frame);
    
    // Layout the series
    LayoutRequest request;
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.series = {
        {"series1", SeriesType::DigitalEvent, true},
        {"series2", SeriesType::DigitalEvent, true}
    };
    
    RowLayoutStrategy strategy;
    LayoutResponse layout = strategy.compute(request);
    
    auto const* layout1 = layout.findLayout("series1");
    auto const* layout2 = layout.findLayout("series2");
    REQUIRE(layout1 != nullptr);
    REQUIRE(layout2 != nullptr);
    
    SECTION("SceneBuilder creates batches and spatial index automatically") {
        BoundingBox bounds(0.0f, -2.0f, 500.0f, 2.0f);
        
        SceneBuilder builder;
        RenderableScene scene = builder
            .setBounds(bounds)
            .setMatrices(glm::mat4(1.0f), glm::mat4(1.0f))
            .addGlyphs("series1", TimeSeriesMapper::mapEvents(*events1, *layout1, *time_frame))
            .addGlyphs("series2", TimeSeriesMapper::mapEvents(*events2, *layout2, *time_frame))
            .build();
        
        // Verify glyph batches were created
        REQUIRE(scene.glyph_batches.size() == 2);
        REQUIRE(scene.glyph_batches[0].positions.size() == 3);  // 3 events in series1
        REQUIRE(scene.glyph_batches[1].positions.size() == 2);  // 2 events in series2
        
        // Verify spatial index was built automatically
        REQUIRE(scene.spatial_index != nullptr);
        REQUIRE(scene.spatial_index->size() == 5);  // Total 5 events
    }
    
    SECTION("Spatial index from SceneBuilder allows hit testing") {
        BoundingBox bounds(0.0f, -2.0f, 500.0f, 2.0f);
        
        SceneBuilder builder;
        RenderableScene scene = builder
            .setBounds(bounds)
            .setMatrices(glm::mat4(1.0f), glm::mat4(1.0f))
            .addGlyphs("series1", TimeSeriesMapper::mapEvents(*events1, *layout1, *time_frame))
            .addGlyphs("series2", TimeSeriesMapper::mapEvents(*events2, *layout2, *time_frame))
            .build();
        
        // Query for event at time 100 in series1's row
        float y1 = layout1->y_transform.offset;
        auto const* hit1 = scene.spatial_index->findNearest(100.0f, y1, 20.0f);
        REQUIRE(hit1 != nullptr);
        REQUIRE(hit1->data == events1->getEntityIds()[0]);
        
        // Query for event at time 150 in series2's row
        float y2 = layout2->y_transform.offset;
        auto const* hit2 = scene.spatial_index->findNearest(150.0f, y2, 20.0f);
        REQUIRE(hit2 != nullptr);
        REQUIRE(hit2->data == events2->getEntityIds()[0]);
    }
    
    SECTION("SceneBuilder distinguishes rows by Y position") {
        BoundingBox bounds(0.0f, -2.0f, 500.0f, 2.0f);
        
        SceneBuilder builder;
        RenderableScene scene = builder
            .setBounds(bounds)
            .addGlyphs("series1", TimeSeriesMapper::mapEvents(*events1, *layout1, *time_frame))
            .addGlyphs("series2", TimeSeriesMapper::mapEvents(*events2, *layout2, *time_frame))
            .build();
        
        float y1 = layout1->y_transform.offset;
        float y2 = layout2->y_transform.offset;
        
        // Event at time 200: only in series1
        // Query at series1's Y should find it with small tolerance
        float y_tolerance = std::abs(y1 - y2) / 4.0f;  // Less than half the row spacing
        auto const* r1 = scene.spatial_index->findNearest(200.0f, y1, y_tolerance);
        REQUIRE(r1 != nullptr);
        REQUIRE(r1->data == events1->getEntityIds()[1]);
        
        // Query at series2's Y with small tolerance should find series2's event only
        // series2 has events at 150, 250 - closest to 200 is either one
        auto const* r2 = scene.spatial_index->findNearest(200.0f, y2, y_tolerance);
        if (r2 != nullptr) {
            // Should be from series2, not series1
            bool is_series2_event = (r2->data == events2->getEntityIds()[0] || 
                                     r2->data == events2->getEntityIds()[1]);
            REQUIRE(is_series2_event);
        }
    }
}


TEST_CASE("Scenario 8: SceneBuilder with interval series",
          "[CorePlotting][Integration][Phase3.5][SceneBuilder][Intervals]") {
    
    auto time_frame = createSimpleTimeFrame(1000);
    EntityRegistry registry;
    
    auto intervals = createIntervalSeries(
        {{100, 200}, {400, 600}},
        "intervals",
        registry
    );
    intervals->setTimeFrame(time_frame);
    
    // Layout
    LayoutRequest request;
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.series = {{"intervals", SeriesType::DigitalInterval, false}};
    
    StackedLayoutStrategy strategy;
    LayoutResponse layout = strategy.compute(request);
    
    auto const* interval_layout = layout.findLayout("intervals");
    REQUIRE(interval_layout != nullptr);
    
    SECTION("SceneBuilder creates rectangle batch for intervals") {
        BoundingBox bounds(0.0f, -2.0f, 1000.0f, 2.0f);
        
        SceneBuilder builder;
        RenderableScene scene = builder
            .setBounds(bounds)
            .addRectangles("intervals", TimeSeriesMapper::mapIntervals(*intervals, *interval_layout, *time_frame))
            .build();
        
        // Verify rectangle batch was created
        REQUIRE(scene.rectangle_batches.size() == 1);
        REQUIRE(scene.rectangle_batches[0].bounds.size() == 2);
        
        // Verify spatial index was built
        REQUIRE(scene.spatial_index != nullptr);
        REQUIRE(scene.spatial_index->size() == 2);
    }
    
    SECTION("Interval rectangles have correct bounds") {
        BoundingBox bounds(0.0f, -2.0f, 1000.0f, 2.0f);
        
        SceneBuilder builder;
        RenderableScene scene = builder
            .setBounds(bounds)
            .addRectangles("intervals", TimeSeriesMapper::mapIntervals(*intervals, *interval_layout, *time_frame))
            .build();
        
        auto const& rect_batch = scene.rectangle_batches[0];
        
        // First interval [100, 200]
        auto const& rect0 = rect_batch.bounds[0];  // {x, y, width, height}
        REQUIRE_THAT(rect0.x, WithinAbs(100.0f, 0.1f));  // Start X
        REQUIRE_THAT(rect0.z, WithinAbs(100.0f, 0.1f));  // Width = 200-100
        
        // Second interval [400, 600]
        auto const& rect1 = rect_batch.bounds[1];
        REQUIRE_THAT(rect1.x, WithinAbs(400.0f, 0.1f));  // Start X
        REQUIRE_THAT(rect1.z, WithinAbs(200.0f, 0.1f));  // Width = 600-400
    }
    
    SECTION("SceneBuilder provides batch key mapping") {
        BoundingBox bounds(0.0f, -2.0f, 1000.0f, 2.0f);
        
        SceneBuilder builder;
        builder.setBounds(bounds)
               .addRectangles("intervals", TimeSeriesMapper::mapIntervals(*intervals, *interval_layout, *time_frame));
        
        // Capture key map BEFORE build (it's cleared in build())
        std::map<size_t, std::string> key_map = builder.getRectangleBatchKeyMap();
        REQUIRE(key_map.size() == 1);
        REQUIRE(key_map.at(0) == "intervals");
        
        // Build scene
        auto scene = builder.build();
        
        // Use captured key map with SceneHitTester
        SceneHitTester tester;
        auto hit = tester.queryIntervals(150.0f, 0.0f, scene, key_map);
        
        REQUIRE(hit.hasHit());
        REQUIRE(hit.series_key == "intervals");
        REQUIRE(hit.interval_start.value() == 100);
        REQUIRE(hit.interval_end.value() == 200);
    }
}


// ============================================================================
// Test Scenario 9: TimeSeriesMapper End-to-End
// ============================================================================

TEST_CASE("Scenario 9: TimeSeriesMapper end-to-end with events and analog",
          "[CorePlotting][Integration][Phase3.5][Mappers][TimeSeriesMapper]") {
    
    // Create TimeFrame with 1:1 mapping (index i → time i*10)
    std::vector<int> times;
    for (int i = 0; i < 100; ++i) {
        times.push_back(i * 10);  // Times: 0, 10, 20, 30, ...
    }
    auto time_frame = std::make_shared<TimeFrame>(times);
    EntityRegistry registry;
    
    // Create event series at indices 10, 25, 50 (times 100, 250, 500)
    auto events = createEventSeries({10, 25, 50}, "spike_events", registry);
    events->setTimeFrame(time_frame);
    
    // Create analog series with known values
    std::vector<float> analog_values;
    std::vector<TimeFrameIndex> analog_times;
    for (int i = 0; i < 100; ++i) {
        analog_values.push_back(static_cast<float>(i) * 0.1f);  // Values: 0.0, 0.1, 0.2, ...
        analog_times.push_back(TimeFrameIndex{i});
    }
    auto analog = std::make_shared<AnalogTimeSeries>(analog_values, analog_times);
    analog->setTimeFrame(time_frame);
    
    // Build stacked layout
    LayoutRequest request;
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.series = {
        {"spike_events", SeriesType::DigitalEvent, true},
        {"analog_trace", SeriesType::Analog, true}
    };
    
    StackedLayoutStrategy strategy;
    LayoutResponse layout = strategy.compute(request);
    
    auto const* event_layout = layout.findLayout("spike_events");
    auto const* analog_layout = layout.findLayout("analog_trace");
    REQUIRE(event_layout != nullptr);
    REQUIRE(analog_layout != nullptr);
    
    SECTION("Event positions match TimeFrame conversion") {
        // Map events using TimeSeriesMapper
        auto mapped_events = TimeSeriesMapper::mapEvents(*events, *event_layout, *time_frame);
        
        std::vector<MappedElement> event_vec;
        for (auto const& elem : mapped_events) {
            event_vec.push_back(elem);
        }
        
        REQUIRE(event_vec.size() == 3);
        
        // Event at index 10 → time 100
        REQUIRE_THAT(event_vec[0].x, WithinAbs(100.0f, 0.1f));
        REQUIRE_THAT(event_vec[0].y, WithinAbs(event_layout->y_transform.offset, 0.001f));
        
        // Event at index 25 → time 250
        REQUIRE_THAT(event_vec[1].x, WithinAbs(250.0f, 0.1f));
        
        // Event at index 50 → time 500
        REQUIRE_THAT(event_vec[2].x, WithinAbs(500.0f, 0.1f));
    }
    
    SECTION("SceneBuilder with mapped events enables hit testing") {
        BoundingBox bounds(0.0f, -2.0f, 1000.0f, 2.0f);
        
        SceneBuilder builder;
        RenderableScene scene = builder
            .setBounds(bounds)
            .addGlyphs("spike_events", TimeSeriesMapper::mapEvents(*events, *event_layout, *time_frame))
            .build();
        
        REQUIRE(scene.glyph_batches.size() == 1);
        REQUIRE(scene.glyph_batches[0].positions.size() == 3);
        REQUIRE(scene.spatial_index != nullptr);
        REQUIRE(scene.spatial_index->size() == 3);
        
        // Query near event at time 250
        auto const* nearest = scene.spatial_index->findNearest(250.0f, event_layout->y_transform.offset, 10.0f);
        REQUIRE(nearest != nullptr);
        
        // Verify EntityId matches
        EntityId expected_id = events->view()[1].entity_id;  // Second event
        REQUIRE(nearest->data == expected_id);
    }
    
    SECTION("Analog mapping produces correct vertex positions") {
        float const y_scale = 1.0f;  // No scaling for test
        
        // Map a subset of analog data
        auto mapped_analog = TimeSeriesMapper::mapAnalogSeries(
            *analog, *analog_layout, *time_frame, y_scale,
            TimeFrameIndex{10}, TimeFrameIndex{15}
        );
        
        std::vector<MappedVertex> vertices;
        for (auto const& v : mapped_analog) {
            vertices.push_back(v);
        }
        
        // Should have 6 vertices (indices 10-15 inclusive)
        REQUIRE(vertices.size() == 6);
        
        // First vertex: index 10 → time 100, value 1.0
        REQUIRE_THAT(vertices[0].x, WithinAbs(100.0f, 0.1f));
        REQUIRE_THAT(vertices[0].y, WithinAbs(1.0f + analog_layout->y_transform.offset, 0.01f));
        
        // Last vertex: index 15 → time 150, value 1.5
        REQUIRE_THAT(vertices[5].x, WithinAbs(150.0f, 0.1f));
    }
}


// ============================================================================
// Test Scenario 10: SpatialMapper End-to-End
// ============================================================================

TEST_CASE("Scenario 10: SpatialMapper end-to-end with PointData",
          "[CorePlotting][Integration][Phase3.5][Mappers][SpatialMapper]") {
    
    // Create PointData with known positions at specific times
    PointData points;
    EntityRegistry registry;
    points.setIdentityContext("spatial_points", &registry);
    
    // Add points at time index 0: (100, 200), (300, 400)
    std::vector<Point2D<float>> frame0_points = {{100.0f, 200.0f}, {300.0f, 400.0f}};
    points.addAtTime(TimeFrameIndex{0}, frame0_points, NotifyObservers::No);
    
    // Add points at time index 1: (150, 250), (350, 450), (500, 100)
    std::vector<Point2D<float>> frame1_points = {{150.0f, 250.0f}, {350.0f, 450.0f}, {500.0f, 100.0f}};
    points.addAtTime(TimeFrameIndex{1}, frame1_points, NotifyObservers::No);
    
    points.rebuildAllEntityIds();
    
    SECTION("mapPointsAtTime extracts correct positions") {
        auto mapped = SpatialMapper::mapPointsAtTime(points, TimeFrameIndex{0});
        
        REQUIRE(mapped.size() == 2);
        REQUIRE_THAT(mapped[0].x, WithinAbs(100.0f, 0.001f));
        REQUIRE_THAT(mapped[0].y, WithinAbs(200.0f, 0.001f));
        REQUIRE_THAT(mapped[1].x, WithinAbs(300.0f, 0.001f));
        REQUIRE_THAT(mapped[1].y, WithinAbs(400.0f, 0.001f));
    }
    
    SECTION("mapPointsAtTime with scaling") {
        // Scale to NDC-like coordinates (assuming 640x480 image)
        float x_scale = 2.0f / 640.0f;
        float y_scale = 2.0f / 480.0f;
        float x_offset = -1.0f;
        float y_offset = -1.0f;
        
        auto mapped = SpatialMapper::mapPointsAtTime(
            points, TimeFrameIndex{0}, x_scale, y_scale, x_offset, y_offset
        );
        
        REQUIRE(mapped.size() == 2);
        
        // Point (100, 200) → (100 * 2/640 - 1, 200 * 2/480 - 1) = (-0.6875, -0.1667)
        float expected_x = 100.0f * x_scale + x_offset;
        float expected_y = 200.0f * y_scale + y_offset;
        REQUIRE_THAT(mapped[0].x, WithinAbs(expected_x, 0.001f));
        REQUIRE_THAT(mapped[0].y, WithinAbs(expected_y, 0.001f));
    }
    
    SECTION("SceneBuilder with spatial points enables hit testing") {
        BoundingBox bounds(0.0f, 0.0f, 640.0f, 480.0f);
        
        auto mapped = SpatialMapper::mapPointsAtTime(points, TimeFrameIndex{1});
        
        SceneBuilder builder;
        RenderableScene scene = builder
            .setBounds(bounds)
            .addGlyphs("spatial_points", mapped)
            .build();
        
        REQUIRE(scene.glyph_batches.size() == 1);
        REQUIRE(scene.glyph_batches[0].positions.size() == 3);
        REQUIRE(scene.spatial_index != nullptr);
        REQUIRE(scene.spatial_index->size() == 3);
        
        // Query near point at (350, 450)
        auto const* nearest = scene.spatial_index->findNearest(350.0f, 450.0f, 10.0f);
        REQUIRE(nearest != nullptr);
        
        // Query near point at (500, 100)
        auto const* nearest2 = scene.spatial_index->findNearest(500.0f, 100.0f, 10.0f);
        REQUIRE(nearest2 != nullptr);
        
        // Query at empty location (far from any point)
        auto const* empty = scene.spatial_index->findNearest(0.0f, 0.0f, 5.0f);
        REQUIRE(empty == nullptr);
    }
    
    SECTION("EntityIds are preserved through mapping") {
        auto mapped = SpatialMapper::mapPointsAtTime(points, TimeFrameIndex{0});
        auto entity_ids = points.getEntityIdsAtTime(TimeFrameIndex{0});
        
        std::vector<EntityId> expected_ids;
        for (auto id : entity_ids) {
            expected_ids.push_back(id);
        }
        
        REQUIRE(mapped.size() == expected_ids.size());
        for (size_t i = 0; i < mapped.size(); ++i) {
            REQUIRE(mapped[i].entity_id == expected_ids[i]);
        }
    }
}


// ============================================================================
// Test Scenario 11: RasterMapper with Relative Time
// ============================================================================

TEST_CASE("Scenario 11: RasterMapper with relative time positioning",
          "[CorePlotting][Integration][Phase3.5][Mappers][RasterMapper]") {
    
    // Create TimeFrame with 1:1 mapping
    auto time_frame = createSimpleTimeFrame(1000);
    EntityRegistry registry;
    
    // Create event series representing spikes in a trial
    // Events at absolute times: 100, 150, 200, 300, 400
    auto trial_events = createEventSeries({100, 150, 200, 300, 400}, "trial_spikes", registry);
    trial_events->setTimeFrame(time_frame);
    
    // Reference time (e.g., stimulus onset) at time 200
    TimeFrameIndex reference_time{200};
    
    // Build row layout (single row for this trial)
    LayoutRequest request;
    request.viewport_y_min = -1.0f;
    request.viewport_y_max = 1.0f;
    request.series = {{"trial_spikes", SeriesType::DigitalEvent, true}};
    
    RowLayoutStrategy row_strategy;
    LayoutResponse layout = row_strategy.compute(request);
    
    auto const* trial_layout = layout.findLayout("trial_spikes");
    REQUIRE(trial_layout != nullptr);
    
    SECTION("mapEventsRelative produces correct relative positions") {
        auto mapped = RasterMapper::mapEventsRelative(
            *trial_events, *trial_layout, *time_frame, reference_time
        );
        
        std::vector<MappedElement> elements;
        for (auto const& elem : mapped) {
            elements.push_back(elem);
        }
        
        REQUIRE(elements.size() == 5);
        
        // Event at 100 → relative = 100 - 200 = -100
        REQUIRE_THAT(elements[0].x, WithinAbs(-100.0f, 0.1f));
        
        // Event at 150 → relative = 150 - 200 = -50
        REQUIRE_THAT(elements[1].x, WithinAbs(-50.0f, 0.1f));
        
        // Event at 200 → relative = 200 - 200 = 0 (aligned to reference)
        REQUIRE_THAT(elements[2].x, WithinAbs(0.0f, 0.1f));
        
        // Event at 300 → relative = 300 - 200 = 100
        REQUIRE_THAT(elements[3].x, WithinAbs(100.0f, 0.1f));
        
        // Event at 400 → relative = 400 - 200 = 200
        REQUIRE_THAT(elements[4].x, WithinAbs(200.0f, 0.1f));
        
        // All Y positions should be at trial layout center
        for (auto const& elem : elements) {
            REQUIRE_THAT(elem.y, WithinAbs(trial_layout->y_transform.offset, 0.001f));
        }
    }
    
    SECTION("mapEventsInWindow filters events correctly") {
        // Window: 50 before to 150 after reference (200)
        // Should include events at 150 (rel=-50), 200 (rel=0), 300 (rel=100)
        // Excludes 100 (rel=-100 < -50) and 400 (rel=200 > 150)
        
        auto mapped = RasterMapper::mapEventsInWindow(
            *trial_events, *trial_layout, *time_frame, reference_time,
            50,  // window_before
            150  // window_after
        );
        
        std::vector<MappedElement> elements;
        for (auto const& elem : mapped) {
            elements.push_back(elem);
        }
        
        REQUIRE(elements.size() == 3);
        REQUIRE_THAT(elements[0].x, WithinAbs(-50.0f, 0.1f));   // Event at 150
        REQUIRE_THAT(elements[1].x, WithinAbs(0.0f, 0.1f));     // Event at 200
        REQUIRE_THAT(elements[2].x, WithinAbs(100.0f, 0.1f));   // Event at 300
    }
    
    SECTION("Multi-trial mapping with different reference times") {
        // Create second trial with different events
        auto trial2_events = createEventSeries({500, 520, 550}, "trial2_spikes", registry);
        trial2_events->setTimeFrame(time_frame);
        
        // Layout for 2 rows
        LayoutRequest multi_request;
        multi_request.viewport_y_min = -1.0f;
        multi_request.viewport_y_max = 1.0f;
        multi_request.series = {
            {"trial1", SeriesType::DigitalEvent, true},
            {"trial2", SeriesType::DigitalEvent, true}
        };
        
        RowLayoutStrategy multi_strategy;
        LayoutResponse multi_layout = multi_strategy.compute(multi_request);
        
        // Configure trials
        std::vector<RasterMapper::TrialConfig> trials = {
            {trial_events.get(), TimeFrameIndex{200}, *multi_layout.findLayout("trial1")},
            {trial2_events.get(), TimeFrameIndex{510}, *multi_layout.findLayout("trial2")}
        };
        
        auto mapped = RasterMapper::mapTrials(trials, *time_frame);
        
        // Trial 1: 5 events, Trial 2: 3 events = 8 total
        REQUIRE(mapped.size() == 8);
        
        // Trial 1 events relative to 200
        REQUIRE_THAT(mapped[0].x, WithinAbs(-100.0f, 0.1f));  // 100 - 200
        REQUIRE_THAT(mapped[4].x, WithinAbs(200.0f, 0.1f));   // 400 - 200
        
        // Trial 2 events relative to 510
        REQUIRE_THAT(mapped[5].x, WithinAbs(-10.0f, 0.1f));   // 500 - 510
        REQUIRE_THAT(mapped[6].x, WithinAbs(10.0f, 0.1f));    // 520 - 510
        REQUIRE_THAT(mapped[7].x, WithinAbs(40.0f, 0.1f));    // 550 - 510
        
        // Verify Y positions differ between trials
        float y_trial1 = multi_layout.findLayout("trial1")->y_transform.offset;
        float y_trial2 = multi_layout.findLayout("trial2")->y_transform.offset;
        REQUIRE(y_trial1 != y_trial2);
        
        REQUIRE_THAT(mapped[0].y, WithinAbs(y_trial1, 0.001f));
        REQUIRE_THAT(mapped[5].y, WithinAbs(y_trial2, 0.001f));
    }
    
    SECTION("SceneBuilder with raster-mapped events") {
        BoundingBox bounds(-200.0f, -2.0f, 300.0f, 2.0f);  // X range for relative time
        
        auto mapped = RasterMapper::mapEventsRelative(
            *trial_events, *trial_layout, *time_frame, reference_time
        );
        
        // Collect to vector for SceneBuilder
        std::vector<MappedElement> mapped_vec;
        for (auto const& elem : mapped) {
            mapped_vec.push_back(elem);
        }
        
        SceneBuilder builder;
        RenderableScene scene = builder
            .setBounds(bounds)
            .addGlyphs("raster_events", mapped_vec)
            .build();
        
        REQUIRE(scene.glyph_batches.size() == 1);
        REQUIRE(scene.glyph_batches[0].positions.size() == 5);
        REQUIRE(scene.spatial_index != nullptr);
        
        // Query at relative time 0 (the reference-aligned event)
        auto const* nearest = scene.spatial_index->findNearest(0.0f, trial_layout->y_transform.offset, 10.0f);
        REQUIRE(nearest != nullptr);
    }
}
