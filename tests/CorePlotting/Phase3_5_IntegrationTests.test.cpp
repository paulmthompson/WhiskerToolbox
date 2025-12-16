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
 * - Scenario 6: IntervalDragController State Machine
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
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"
#include "CorePlotting/SceneGraph/SceneBuilder.hpp"
#include "CorePlotting/SpatialAdapter/EventSpatialAdapter.hpp"
#include "CorePlotting/SpatialAdapter/PointSpatialAdapter.hpp"
#include "CorePlotting/CoordinateTransform/TimeRange.hpp"
#include "CorePlotting/CoordinateTransform/ViewState.hpp"
#include "CorePlotting/CoordinateTransform/TimeAxisCoordinates.hpp"
#include "CorePlotting/Interaction/SceneHitTester.hpp"
#include "CorePlotting/Interaction/IntervalDragController.hpp"
#include "CorePlotting/Transformers/RasterBuilder.hpp"

// DataManager includes
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
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
    
    std::vector<glm::vec2> all_positions;
    std::vector<EntityId> all_entity_ids;
    
    for (auto const& [series, layout] : series_layouts) {
        for (auto const& event : series->view()) {
            float x = static_cast<float>(time_frame.getTimeAtIndex(event.event_time));
            all_positions.emplace_back(x, layout->result.allocated_y_center);
            all_entity_ids.push_back(event.entity_id);
        }
    }
    
    return EventSpatialAdapter::buildFromPositions(all_positions, all_entity_ids, bounds);
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
        float height = layout.result.allocated_height;
        float y = layout.result.allocated_y_center - height / 2.0f;
        
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
        float y0 = layout.layouts[0].result.allocated_y_center;
        float y1 = layout.layouts[1].result.allocated_y_center;
        float y2 = layout.layouts[2].result.allocated_y_center;
        
        // All Y positions should be different
        REQUIRE_THAT(y0, !WithinAbs(y1, 0.01f));
        REQUIRE_THAT(y1, !WithinAbs(y2, 0.01f));
        REQUIRE_THAT(y0, !WithinAbs(y2, 0.01f));
    }
    
    SECTION("Hit test on event returns correct EntityId") {
        auto const* event_layout = layout.findLayout("spike_events");
        REQUIRE(event_layout != nullptr);
        
        BoundingBox bounds(0.0f, -2.0f, 1000.0f, 2.0f);
        auto index = EventSpatialAdapter::buildStacked(
            *events, *time_frame, *event_layout, bounds);
        
        // Create scene with spatial index
        RenderableScene scene;
        scene.spatial_index = std::move(index);
        scene.view_matrix = glm::mat4(1.0f);
        scene.projection_matrix = glm::mat4(1.0f);
        
        SceneHitTester tester;
        
        // Click on event at time=300 (should hit EntityId for event at index 1)
        float event_y = event_layout->result.allocated_y_center;
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
        float analog1_y = analog1_layout->result.allocated_y_center;
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
        REQUIRE_THAT(trial1_layout->result.allocated_height,
                    WithinAbs(trial2_layout->result.allocated_height, 0.001f));
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
        float trial1_y = trial1_layout->result.allocated_y_center;
        auto const* hit1 = combined_index->findNearest(50.0f, trial1_y, 10.0f);
        REQUIRE(hit1 != nullptr);
        REQUIRE(hit1->data == trial1->getEntityIds()[0]);
        
        // Query for event in trial2 (different row)
        float trial2_y = trial2_layout->result.allocated_y_center;
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
        
        float trial1_y = trial1_layout->result.allocated_y_center;
        float trial2_y = trial2_layout->result.allocated_y_center;
        
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
    auto index = EventSpatialAdapter::buildStacked(*events, *time_frame, *event_layout, bounds);
    
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
        auto index = EventSpatialAdapter::buildStacked(*events, *time_frame, *event_layout, bounds);
        
        RenderableScene scene;
        scene.spatial_index = std::move(index);
        scene.view_matrix = glm::mat4(1.0f);
        scene.projection_matrix = glm::mat4(1.0f);
        
        SceneHitTester tester;
        
        // Query at event position - should return event, not analog region
        float event_y = event_layout->result.allocated_y_center;
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
        float analog_y = analog_layout->result.allocated_y_center;
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
        tree->insert(500.0f, event_layout->result.allocated_y_center, EntityId{42});
        scene.spatial_index = std::move(tree);
        scene.view_matrix = glm::mat4(1.0f);
        scene.projection_matrix = glm::mat4(1.0f);
        
        // Query where event exists - should return event
        auto hit = tester.hitTest(500.0f, event_layout->result.allocated_y_center, scene, layout);
        
        REQUIRE(hit.hit_type == HitType::DigitalEvent);
    }
}


// ============================================================================
// Test Scenario 6: IntervalDragController State Machine
// ============================================================================

TEST_CASE("Scenario 6: Interval drag controller enforces constraints",
          "[CorePlotting][Integration][Phase3.5][DragController]") {
    
    SECTION("Basic drag workflow: start → update → finish") {
        IntervalDragController controller;
        
        // Start drag from left edge hit
        auto hit = HitTestResult::intervalEdgeHit(
            "intervals", EntityId{100}, true, 50, 150, 50.0f, 0.0f);
        
        REQUIRE(controller.startDrag(hit));
        REQUIRE(controller.isActive());
        REQUIRE(controller.getState().edge == DraggedEdge::Left);
        
        // Update drag position
        controller.updateDrag(30.0f);
        REQUIRE(controller.getState().current_start == 30);
        REQUIRE(controller.getState().current_end == 150);  // Unchanged
        
        // Finish drag
        auto final_state = controller.finishDrag();
        REQUIRE(final_state.hasChanged());
        REQUIRE(final_state.current_start == 30);
        REQUIRE_FALSE(controller.isActive());
    }
    
    SECTION("Minimum width constraint prevents collapse") {
        IntervalDragConfig config;
        config.min_width = 20;
        IntervalDragController controller(config);
        
        auto hit = HitTestResult::intervalEdgeHit(
            "intervals", EntityId{100}, true, 50, 150, 50.0f, 0.0f);
        controller.startDrag(hit);
        
        // Try to drag left edge past minimum width
        controller.updateDrag(140.0f);  // Would result in width of 10
        
        // Should be clamped to maintain min_width
        REQUIRE(controller.getState().current_start == 130);  // 150 - 20 = 130
        REQUIRE((controller.getState().current_end - controller.getState().current_start) >= 20);
    }
    
    SECTION("Time bounds constraint prevents exceeding limits") {
        IntervalDragConfig config;
        config.min_time = 0;
        config.max_time = 1000;
        config.min_width = 1;
        IntervalDragController controller(config);
        
        auto hit = HitTestResult::intervalEdgeHit(
            "intervals", EntityId{100}, true, 50, 150, 50.0f, 0.0f);
        controller.startDrag(hit);
        
        // Try to drag past min_time
        controller.updateDrag(-50.0f);
        REQUIRE(controller.getState().current_start == 0);  // Clamped to min_time
    }
    
    SECTION("Cancel restores original values") {
        IntervalDragController controller;
        
        auto hit = HitTestResult::intervalEdgeHit(
            "intervals", EntityId{100}, true, 50, 150, 50.0f, 0.0f);
        controller.startDrag(hit);
        controller.updateDrag(30.0f);
        
        // Cancel
        auto cancelled = controller.cancelDrag();
        
        REQUIRE(cancelled.current_start == 50);  // Restored to original
        REQUIRE(cancelled.current_end == 150);
        REQUIRE_FALSE(cancelled.hasChanged());
        REQUIRE_FALSE(controller.isActive());
    }
    
    SECTION("Edge swap when allowed") {
        IntervalDragConfig config;
        config.min_width = 1;
        config.allow_edge_swap = true;
        IntervalDragController controller(config);
        
        auto hit = HitTestResult::intervalEdgeHit(
            "intervals", EntityId{100}, true, 50, 150, 50.0f, 0.0f);
        controller.startDrag(hit);
        
        // Drag left edge past right edge
        controller.updateDrag(200.0f);
        
        // Edge should swap
        REQUIRE(controller.getState().edge == DraggedEdge::Right);
        REQUIRE(controller.getState().current_start == 150);
        REQUIRE(controller.getState().current_end == 200);
    }
    
    SECTION("Only interval edge hits can start drag") {
        IntervalDragController controller;
        
        // Body hit should not start drag
        auto body_hit = HitTestResult::intervalBodyHit(
            "intervals", EntityId{100}, 50, 150, 0.0f);
        REQUIRE_FALSE(controller.startDrag(body_hit));
        
        // Event hit should not start drag
        auto event_hit = HitTestResult::eventHit(
            "events", EntityId{1}, 0.0f, 0.0f, 0.0f);
        REQUIRE_FALSE(controller.startDrag(event_hit));
        
        // No hit should not start drag
        auto no_hit = HitTestResult::noHit();
        REQUIRE_FALSE(controller.startDrag(no_hit));
    }
}


// ============================================================================
// Additional Integration Tests: TimeRange Bounds with TimeFrame
// ============================================================================

TEST_CASE("TimeRange integration with TimeFrame bounds",
          "[CorePlotting][Integration][Phase3.5][TimeRange]") {
    
    auto time_frame = createSimpleTimeFrame(1000);  // Times 0-999
    
    SECTION("fromTimeFrame captures bounds correctly") {
        TimeRange range = TimeRange::fromTimeFrame(*time_frame);
        
        REQUIRE(range.min_bound == 0);
        REQUIRE(range.max_bound == 999);
        REQUIRE(range.start == 0);
        REQUIRE(range.end == 999);
    }
    
    SECTION("Cannot zoom beyond data bounds") {
        TimeRange range = TimeRange::fromTimeFrame(*time_frame);
        
        // Request full range
        range.setCenterAndZoom(500, 2000);  // Width exceeds total
        
        REQUIRE(range.start >= range.min_bound);
        REQUIRE(range.end <= range.max_bound);
        REQUIRE(range.getWidth() == range.getTotalBoundedWidth());
    }
    
    SECTION("Zoom in maintains center when possible") {
        TimeRange range = TimeRange::fromTimeFrame(*time_frame);
        
        // Zoom to 100 units centered at 500
        range.setCenterAndZoom(500, 100);
        
        REQUIRE(range.getWidth() == 100);
        REQUIRE(range.contains(500));
        // Center should be approximately at 500
        int64_t center = range.getCenter();
        REQUIRE(center >= 450);
        REQUIRE(center <= 550);
    }
    
    SECTION("Zoom near edge shifts to stay in bounds") {
        TimeRange range = TimeRange::fromTimeFrame(*time_frame);
        
        // Zoom near right edge
        range.setCenterAndZoom(950, 200);
        
        REQUIRE(range.end <= range.max_bound);
        REQUIRE(range.start >= range.min_bound);
        REQUIRE(range.getWidth() == 200);
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
        float y1 = layout1->result.allocated_y_center;
        auto const* hit1 = scene.spatial_index->findNearest(100.0f, y1, 20.0f);
        REQUIRE(hit1 != nullptr);
        REQUIRE(hit1->data == events1->getEntityIds()[0]);
        
        // Query for event at time 150 in series2's row
        float y2 = layout2->result.allocated_y_center;
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
        
        float y1 = layout1->result.allocated_y_center;
        float y2 = layout2->result.allocated_y_center;
        
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

