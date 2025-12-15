/**
 * @file IntegrationTests.test.cpp
 * @brief Integration tests for CorePlotting Phase 1.6
 * 
 * These tests validate end-to-end workflows through the CorePlotting stack:
 * - Raster plot: multiple series, centered events, spatial queries
 * - Stacked events: DataViewer-style absolute time positioning
 * - Scene building: LayoutEngine → Transformer → SceneBuilder → RenderableScene
 * - Coordinate transforms: screen → world → QuadTree query
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/Layout/LayoutEngine.hpp"
#include "CorePlotting/Layout/RowLayoutStrategy.hpp"
#include "CorePlotting/Layout/StackedLayoutStrategy.hpp"
#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"
#include "CorePlotting/SpatialAdapter/EventSpatialAdapter.hpp"
#include "CorePlotting/SpatialAdapter/PointSpatialAdapter.hpp"
#include "CorePlotting/SpatialAdapter/PolyLineSpatialAdapter.hpp"
#include "CorePlotting/Transformers/GapDetector.hpp"
#include "CorePlotting/Transformers/RasterBuilder.hpp"
#include "CorePlotting/CoordinateTransform/TimeRange.hpp"
#include "CorePlotting/CoordinateTransform/ViewState.hpp"

#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "Entity/EntityRegistry.hpp"

#include <cmath>
#include <limits>
#include <memory>
#include <vector>

using namespace CorePlotting;

// ============================================================================
// Test Fixtures and Helpers
// ============================================================================

namespace {

/**
 * @brief Create a DigitalEventSeries with known events and EntityIds
 * 
 * @param event_times Vector of event times (as integers for TimeFrameIndex)
 * @param data_key Key for entity registration
 * @param registry EntityRegistry for assigning IDs
 * @return Configured DigitalEventSeries
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
 * 
 * @param length Number of time points
 * @return TimeFrame where index i maps to time i
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
 * @brief Helper to check if a QuadTree query returns the expected EntityId
 */
bool queryReturnsEntity(QuadTree<EntityId> const& tree, 
                       float x, float y, 
                       float tolerance,
                       EntityId expected_id) {
    auto const* result = tree.findNearest(x, y, tolerance);
    return result != nullptr && result->data == expected_id;
}

} // anonymous namespace


// ============================================================================
// Raster Plot Integration Tests
// ============================================================================

TEST_CASE("Raster Plot - Multiple trials with centered events", "[CorePlotting][Integration][Raster]") {
    // Setup: Create a TimeFrame and EntityRegistry
    auto time_frame = createSimpleTimeFrame(2000);
    EntityRegistry registry;
    
    // Create a single event series representing spike times
    // Events at times: 100, 200, 500, 800, 1000, 1100, 1500
    auto event_series = createEventSeries(
        {100, 200, 500, 800, 1000, 1100, 1500},
        "spikes",
        registry
    );
    event_series->setTimeFrame(time_frame);
    
    // Get the EntityIds for verification
    auto const& entity_ids = event_series->getEntityIds();
    REQUIRE(entity_ids.size() == 7);
    
    // Define trial centers (reference events for each row)
    // Trial 0: centered at time 500
    // Trial 1: centered at time 1000
    // Trial 2: centered at time 1500
    std::vector<int64_t> trial_centers = {500, 1000, 1500};
    
    // Use RowLayoutStrategy to compute row positions
    LayoutRequest layout_request;
    layout_request.viewport_y_min = -1.0f;
    layout_request.viewport_y_max = 1.0f;
    for (size_t i = 0; i < trial_centers.size(); ++i) {
        layout_request.series.push_back({
            "trial_" + std::to_string(i),
            SeriesType::DigitalEvent,
            true
        });
    }
    
    RowLayoutStrategy strategy;
    LayoutResponse layout_response = strategy.compute(layout_request);
    
    REQUIRE(layout_response.layouts.size() == 3);
    
    // Extract just the SeriesLayout vector for adapters
    std::vector<SeriesLayout> row_layouts = layout_response.layouts;
    
    SECTION("RasterBuilder produces correct glyph positions") {
        RasterBuilder builder;
        RasterBuilder::Config config;
        config.window_start = -600;  // Look 600 units before center
        config.window_end = 600;     // Look 600 units after center
        builder.setTimeWindow(config.window_start, config.window_end);
        
        auto batch = builder.transform(*event_series, *time_frame, row_layouts, trial_centers);
        
        // Verify we got glyphs
        REQUIRE(!batch.positions.empty());
        REQUIRE(batch.positions.size() == batch.entity_ids.size());
        
        // Check some specific positions:
        // Trial 0 (center=500): events at 100(-400), 200(-300), 500(0), 800(+300), 1000(+500), 1100(+600) are in window = 6
        // Trial 1 (center=1000): events at 500(-500), 800(-200), 1000(0), 1100(+100), 1500(+500) are in window = 5
        // Trial 2 (center=1500): events at 1000(-500), 1100(-400), 1500(0) are in window = 3
        
        // Total expected: 6 + 5 + 3 = 14 glyphs
        REQUIRE(batch.positions.size() == 14);
    }
    
    SECTION("EventSpatialAdapter builds raster QuadTree with correct positions") {
        BoundingBox bounds(-1000.0f, -2.0f, 1000.0f, 2.0f);
        
        auto index = EventSpatialAdapter::buildRaster(
            *event_series, *time_frame, row_layouts, trial_centers, bounds);
        
        REQUIRE(index != nullptr);
        REQUIRE(index->size() > 0);
        
        // Query for event at relative time 0 in Trial 0 (event at absolute time 500)
        // Y position should be row_layouts[0].result.allocated_y_center
        float trial0_y = row_layouts[0].result.allocated_y_center;
        
        // The event at time 500 has entity_ids[2] (index 2 in the series)
        EntityId event_500_id = entity_ids[2];
        
        // Query at (0, trial0_y) should find event at time 500
        auto const* result = index->findNearest(0.0f, trial0_y, 10.0f);
        REQUIRE(result != nullptr);
        REQUIRE(result->data == event_500_id);
        
        // Query for event at relative time -300 in Trial 0 (event at absolute time 200)
        // event at time 200 has entity_ids[1]
        EntityId event_200_id = entity_ids[1];
        result = index->findNearest(-300.0f, trial0_y, 10.0f);
        REQUIRE(result != nullptr);
        REQUIRE(result->data == event_200_id);
    }
    
    SECTION("Same event appears in multiple trials at different relative positions") {
        BoundingBox bounds(-1000.0f, -2.0f, 1000.0f, 2.0f);
        
        auto index = EventSpatialAdapter::buildRaster(
            *event_series, *time_frame, row_layouts, trial_centers, bounds);
        
        // Event at absolute time 1000 appears in:
        // - Trial 1 (center=1000) at relative time 0
        // - Trial 2 (center=1500) at relative time -500
        EntityId event_1000_id = entity_ids[4]; // index 4 in series
        
        float trial1_y = row_layouts[1].result.allocated_y_center;
        float trial2_y = row_layouts[2].result.allocated_y_center;
        
        // Find in Trial 1 at relative time 0
        auto const* result1 = index->findNearest(0.0f, trial1_y, 10.0f);
        REQUIRE(result1 != nullptr);
        REQUIRE(result1->data == event_1000_id);
        
        // Find in Trial 2 at relative time -500
        auto const* result2 = index->findNearest(-500.0f, trial2_y, 10.0f);
        REQUIRE(result2 != nullptr);
        REQUIRE(result2->data == event_1000_id);
        
        // Different Y positions but same EntityId
        REQUIRE_THAT(result1->y, !Catch::Matchers::WithinAbs(result2->y, 0.01f));
    }
    
    SECTION("Query at position with no event returns nullptr") {
        BoundingBox bounds(-1000.0f, -2.0f, 1000.0f, 2.0f);
        
        auto index = EventSpatialAdapter::buildRaster(
            *event_series, *time_frame, row_layouts, trial_centers, bounds);
        
        // Query far from any event
        auto const* result = index->findNearest(999.0f, 999.0f, 1.0f);
        REQUIRE(result == nullptr);
    }
}


// ============================================================================
// Stacked Events Integration Tests (DataViewer Style)
// ============================================================================

TEST_CASE("Stacked Events - Multiple series in absolute time", "[CorePlotting][Integration][Stacked]") {
    // Setup: Create a TimeFrame and EntityRegistry
    auto time_frame = createSimpleTimeFrame(2000);
    EntityRegistry registry;
    
    // Create multiple event series (different data sources)
    // Series A: lick events at times 100, 300, 500, 700
    auto series_a = createEventSeries({100, 300, 500, 700}, "licks", registry);
    series_a->setTimeFrame(time_frame);
    
    // Series B: reward events at times 200, 600
    auto series_b = createEventSeries({200, 600}, "rewards", registry);
    series_b->setTimeFrame(time_frame);
    
    // Series C: tone events at times 50, 400, 800
    auto series_c = createEventSeries({50, 400, 800}, "tones", registry);
    series_c->setTimeFrame(time_frame);
    
    // Use StackedLayoutStrategy to compute positions
    LayoutRequest layout_request;
    layout_request.viewport_y_min = -1.0f;
    layout_request.viewport_y_max = 1.0f;
    layout_request.series = {
        {"licks", SeriesType::DigitalEvent, true},
        {"rewards", SeriesType::DigitalEvent, true},
        {"tones", SeriesType::DigitalEvent, true}
    };
    
    StackedLayoutStrategy strategy;
    LayoutResponse layout_response = strategy.compute(layout_request);
    
    REQUIRE(layout_response.layouts.size() == 3);
    
    SECTION("Each series has distinct Y position") {
        float y0 = layout_response.layouts[0].result.allocated_y_center;
        float y1 = layout_response.layouts[1].result.allocated_y_center;
        float y2 = layout_response.layouts[2].result.allocated_y_center;
        
        // All Y positions should be different
        REQUIRE_THAT(y0, !Catch::Matchers::WithinAbs(y1, 0.01f));
        REQUIRE_THAT(y1, !Catch::Matchers::WithinAbs(y2, 0.01f));
        REQUIRE_THAT(y0, !Catch::Matchers::WithinAbs(y2, 0.01f));
    }
    
    SECTION("Build combined QuadTree from multiple stacked series") {
        BoundingBox bounds(0.0f, -2.0f, 1000.0f, 2.0f);
        
        // Build index for each series, then combine
        auto index_a = EventSpatialAdapter::buildStacked(
            *series_a, *time_frame, layout_response.layouts[0], bounds);
        auto index_b = EventSpatialAdapter::buildStacked(
            *series_b, *time_frame, layout_response.layouts[1], bounds);
        auto index_c = EventSpatialAdapter::buildStacked(
            *series_c, *time_frame, layout_response.layouts[2], bounds);
        
        // Verify individual indexes work
        float y_licks = layout_response.layouts[0].result.allocated_y_center;
        float y_rewards = layout_response.layouts[1].result.allocated_y_center;
        float y_tones = layout_response.layouts[2].result.allocated_y_center;
        
        // Query licks series for event at time 300
        auto const* lick_result = index_a->findNearest(300.0f, y_licks, 10.0f);
        REQUIRE(lick_result != nullptr);
        REQUIRE(lick_result->data == series_a->getEntityIds()[1]); // index 1 = time 300
        
        // Query rewards series for event at time 200
        auto const* reward_result = index_b->findNearest(200.0f, y_rewards, 10.0f);
        REQUIRE(reward_result != nullptr);
        REQUIRE(reward_result->data == series_b->getEntityIds()[0]); // index 0 = time 200
        
        // Query tones series for event at time 400
        auto const* tone_result = index_c->findNearest(400.0f, y_tones, 10.0f);
        REQUIRE(tone_result != nullptr);
        REQUIRE(tone_result->data == series_c->getEntityIds()[1]); // index 1 = time 400
    }
    
    SECTION("Combined spatial index from all series using buildFromPositions") {
        BoundingBox bounds(0.0f, -2.0f, 1000.0f, 2.0f);
        
        // Manually build combined positions and entity_ids
        std::vector<glm::vec2> all_positions;
        std::vector<EntityId> all_entity_ids;
        
        // Helper to add series events to combined vectors
        auto add_series = [&](DigitalEventSeries const& series, float y_pos) {
            for (auto const& event : series.view()) {
                float x = static_cast<float>(time_frame->getTimeAtIndex(event.event_time));
                all_positions.emplace_back(x, y_pos);
                all_entity_ids.push_back(event.entity_id);
            }
        };
        
        add_series(*series_a, layout_response.layouts[0].result.allocated_y_center);
        add_series(*series_b, layout_response.layouts[1].result.allocated_y_center);
        add_series(*series_c, layout_response.layouts[2].result.allocated_y_center);
        
        // Build combined index
        auto combined_index = EventSpatialAdapter::buildFromPositions(
            all_positions, all_entity_ids, bounds);
        
        REQUIRE(combined_index != nullptr);
        REQUIRE(combined_index->size() == 9); // 4 + 2 + 3 events
        
        // Query for specific events from each series
        float y_licks = layout_response.layouts[0].result.allocated_y_center;
        float y_rewards = layout_response.layouts[1].result.allocated_y_center;
        float y_tones = layout_response.layouts[2].result.allocated_y_center;
        
        // Find lick at time 500
        auto const* r1 = combined_index->findNearest(500.0f, y_licks, 10.0f);
        REQUIRE(r1 != nullptr);
        REQUIRE(r1->data == series_a->getEntityIds()[2]);
        
        // Find reward at time 600
        auto const* r2 = combined_index->findNearest(600.0f, y_rewards, 10.0f);
        REQUIRE(r2 != nullptr);
        REQUIRE(r2->data == series_b->getEntityIds()[1]);
        
        // Find tone at time 50
        auto const* r3 = combined_index->findNearest(50.0f, y_tones, 10.0f);
        REQUIRE(r3 != nullptr);
        REQUIRE(r3->data == series_c->getEntityIds()[0]);
    }
    
    SECTION("Y-position distinguishes events at same time from different series") {
        BoundingBox bounds(0.0f, -2.0f, 1000.0f, 2.0f);
        
        // Create two series with events at same absolute time
        auto series_x = createEventSeries({500}, "series_x", registry);
        series_x->setTimeFrame(time_frame);
        
        auto series_y = createEventSeries({500}, "series_y", registry);
        series_y->setTimeFrame(time_frame);
        
        // Layout them in different rows
        LayoutRequest req;
        req.viewport_y_min = -1.0f;
        req.viewport_y_max = 1.0f;
        req.series = {
            {"series_x", SeriesType::DigitalEvent, true},
            {"series_y", SeriesType::DigitalEvent, true}
        };
        
        StackedLayoutStrategy strat;
        LayoutResponse resp = strat.compute(req);
        
        float y_x = resp.layouts[0].result.allocated_y_center;
        float y_y = resp.layouts[1].result.allocated_y_center;
        
        // Build separate indexes
        auto idx_x = EventSpatialAdapter::buildStacked(*series_x, *time_frame, resp.layouts[0], bounds);
        auto idx_y = EventSpatialAdapter::buildStacked(*series_y, *time_frame, resp.layouts[1], bounds);
        
        // Query at time 500 but different Y positions
        auto const* rx = idx_x->findNearest(500.0f, y_x, 10.0f);
        auto const* ry = idx_y->findNearest(500.0f, y_y, 10.0f);
        
        REQUIRE(rx != nullptr);
        REQUIRE(ry != nullptr);
        
        // Same X position but different EntityIds
        REQUIRE_THAT(rx->x, Catch::Matchers::WithinAbs(ry->x, 0.01f));
        REQUIRE(rx->data != ry->data);
    }
}


// ============================================================================
// End-to-End Scene Building Tests
// ============================================================================

TEST_CASE("Scene Building - Layout to Glyph Batch workflow", "[CorePlotting][Integration][Scene]") {
    auto time_frame = createSimpleTimeFrame(2000);
    EntityRegistry registry;
    
    auto event_series = createEventSeries(
        {100, 300, 600, 900, 1200},
        "events",
        registry
    );
    event_series->setTimeFrame(time_frame);
    
    // Define trial centers for 3-row raster
    std::vector<int64_t> trial_centers = {300, 600, 900};
    
    SECTION("Complete pipeline: Layout → RasterBuilder → QuadTree") {
        // Step 1: Compute layout
        LayoutRequest layout_request;
        layout_request.viewport_y_min = -1.0f;
        layout_request.viewport_y_max = 1.0f;
        for (size_t i = 0; i < trial_centers.size(); ++i) {
            layout_request.series.push_back({
                "trial_" + std::to_string(i),
                SeriesType::DigitalEvent,
                true
            });
        }
        
        RowLayoutStrategy strategy;
        LayoutResponse layout_response = strategy.compute(layout_request);
        
        // Step 2: Build glyph batch with RasterBuilder
        RasterBuilder builder;
        builder.setTimeWindow(-500, 500);
        
        auto batch = builder.transform(
            *event_series, *time_frame, 
            layout_response.layouts, trial_centers);
        
        REQUIRE(!batch.positions.empty());
        
        // Step 3: Build spatial index from the glyph batch
        BoundingBox bounds(-600.0f, -2.0f, 600.0f, 2.0f);
        
        auto spatial_index = PointSpatialAdapter::buildFromGlyphs(batch, bounds);
        
        REQUIRE(spatial_index != nullptr);
        
        // Verify spatial index matches glyph positions
        for (size_t i = 0; i < batch.positions.size(); ++i) {
            auto const& pos = batch.positions[i];
            EntityId expected_id = batch.entity_ids[i];
            
            // Query should find this exact glyph
            auto const* result = spatial_index->findNearest(pos.x, pos.y, 1.0f);
            REQUIRE(result != nullptr);
            REQUIRE(result->data == expected_id);
        }
    }
    
    SECTION("QuadTree contains all glyphs from batch") {
        LayoutRequest layout_request;
        layout_request.viewport_y_min = -1.0f;
        layout_request.viewport_y_max = 1.0f;
        for (size_t i = 0; i < trial_centers.size(); ++i) {
            layout_request.series.push_back({
                "trial_" + std::to_string(i),
                SeriesType::DigitalEvent,
                true
            });
        }
        
        RowLayoutStrategy strategy;
        LayoutResponse layout_response = strategy.compute(layout_request);
        
        RasterBuilder builder;
        builder.setTimeWindow(-500, 500);
        
        auto batch = builder.transform(
            *event_series, *time_frame,
            layout_response.layouts, trial_centers);
        
        size_t expected_glyph_count = batch.positions.size();
        
        BoundingBox bounds(-600.0f, -2.0f, 600.0f, 2.0f);
        
        auto spatial_index = PointSpatialAdapter::buildFromGlyphs(batch, bounds);
        
        // Query entire bounds to get all points
        std::vector<QuadTreePoint<EntityId>> all_points;
        spatial_index->query(bounds, all_points);
        
        REQUIRE(all_points.size() == expected_glyph_count);
    }
}


// ============================================================================
// TimeRange Bounds Enforcement Tests
// ============================================================================

TEST_CASE("TimeRange - Bounds enforcement", "[CorePlotting][Integration][TimeRange]") {
    auto time_frame = createSimpleTimeFrame(1000); // Times 0-999
    
    SECTION("TimeRange clamps to TimeFrame bounds on construction") {
        TimeRange range = TimeRange::fromTimeFrame(*time_frame);
        
        REQUIRE(range.start == 0);
        REQUIRE(range.end == 999);
        REQUIRE(range.min_bound == 0);
        REQUIRE(range.max_bound == 999);
    }
    
    SECTION("setVisibleRange clamps to bounds") {
        TimeRange range = TimeRange::fromTimeFrame(*time_frame);
        
        // Try to set beyond bounds
        range.setVisibleRange(-100, 2000);
        
        REQUIRE(range.start >= range.min_bound);
        REQUIRE(range.end <= range.max_bound);
    }
    
    SECTION("setCenterAndZoom respects bounds") {
        TimeRange range = TimeRange::fromTimeFrame(*time_frame);
        
        // Try to center at right edge with width that would exceed bounds
        range.setCenterAndZoom(950, 200); // Center at 950, width 200 would go to 1050
        
        // Should be clamped/shifted to stay within bounds
        REQUIRE(range.end <= range.max_bound);
        REQUIRE(range.start >= range.min_bound);
        
        // Width should be approximately what we asked for (or clamped)
        REQUIRE(range.getWidth() == 200);
    }
    
    SECTION("setCenterAndZoom at left edge shifts range") {
        TimeRange range = TimeRange::fromTimeFrame(*time_frame);
        
        // Center at left edge
        range.setCenterAndZoom(50, 200);
        
        REQUIRE(range.start >= range.min_bound);
        REQUIRE(range.getWidth() == 200);
    }
    
    SECTION("setCenterAndZoom with width exceeding total bounds") {
        TimeRange range = TimeRange::fromTimeFrame(*time_frame);
        
        // Request width larger than total available
        range.setCenterAndZoom(500, 2000);
        
        // Should show entire range
        REQUIRE(range.start == range.min_bound);
        REQUIRE(range.end == range.max_bound);
        REQUIRE(range.getWidth() == range.getTotalBoundedWidth());
    }
    
    SECTION("setCenterAndZoom with zoom in") {
        TimeRange range = TimeRange::fromTimeFrame(*time_frame);
        
        // Start with full range
        REQUIRE(range.getWidth() == 1000);
        
        // Zoom in to 100 units centered at 500
        range.setCenterAndZoom(500, 100);
        
        REQUIRE(range.getWidth() == 100);
        REQUIRE(range.contains(500));
        // Center should be approximately at 500
        REQUIRE(range.getCenter() >= 450);
        REQUIRE(range.getCenter() <= 550);
    }
}


// ============================================================================
// Coordinate Transform Round-Trip Tests
// ============================================================================

TEST_CASE("Coordinate Transform - Screen to World to Query", "[CorePlotting][Integration][Coordinates]") {
    auto time_frame = createSimpleTimeFrame(1000);
    EntityRegistry registry;
    
    auto event_series = createEventSeries(
        {250, 500, 750},
        "events",
        registry
    );
    event_series->setTimeFrame(time_frame);
    
    // Simple layout: single row
    SeriesLayout layout;
    layout.result = SeriesLayoutResult(0.0f, 2.0f); // Center at 0, height 2
    layout.series_id = "events";
    layout.series_index = 0;
    
    BoundingBox bounds(0.0f, -1.0f, 1000.0f, 1.0f);
    
    auto index = EventSpatialAdapter::buildStacked(
        *event_series, *time_frame, layout, bounds);
    
    SECTION("ViewState screenToWorld basic transform") {
        ViewState view;
        view.data_bounds = bounds;
        view.data_bounds_valid = true;
        view.viewport_width = 800;
        view.viewport_height = 600;
        view.zoom_level_x = 1.0f;
        view.zoom_level_y = 1.0f;
        view.pan_offset_x = 0.0f;
        view.pan_offset_y = 0.0f;
        view.padding_factor = 1.0f; // No padding for simpler math
        
        // Screen center should map to world center
        auto world_center = screenToWorld(view, 400, 300);
        
        // With default view (no pan/zoom), screen center = world center
        REQUIRE_THAT(world_center.x, Catch::Matchers::WithinAbs(500.0f, 1.0f));
        REQUIRE_THAT(world_center.y, Catch::Matchers::WithinAbs(0.0f, 0.1f));
    }
    
    SECTION("Query after screen-to-world transform finds correct event") {
        ViewState view;
        view.data_bounds = bounds;
        view.data_bounds_valid = true;
        view.viewport_width = 1000;  // 1:1 pixel to world unit for simplicity
        view.viewport_height = 200;  // Y range [-1, 1] = 2 units, so 100 pixels per unit
        view.zoom_level_x = 1.0f;
        view.zoom_level_y = 1.0f;
        view.pan_offset_x = 0.0f;
        view.pan_offset_y = 0.0f;
        view.padding_factor = 1.0f;
        
        // Screen position that should be near event at time 500
        // X: 500 pixels = 500 world units (assuming viewport maps to bounds)
        // Y: 100 pixels = center (y=0 in world)
        
        auto world_pos = screenToWorld(view, 500, 100);
        
        // Query the spatial index
        auto const* result = index->findNearest(world_pos.x, world_pos.y, 50.0f);
        
        REQUIRE(result != nullptr);
        // Should find event at time 500 (index 1)
        REQUIRE(result->data == event_series->getEntityIds()[1]);
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
        
        // Pan the view to the right (positive pan_offset_x means we see higher x values)
        // pan_offset is normalized, so 0.2 means 20% of data width
        view.pan_offset_x = 0.2f; // Pan right by 200 units (20% of 1000)
        view.pan_offset_y = 0.0f;
        
        // Now screen center (500, 100) should map to world position ~700
        auto world_pos = screenToWorld(view, 500, 100);
        
        // Query should find event at time 750 (closest to 700)
        auto const* result = index->findNearest(world_pos.x, world_pos.y, 100.0f);
        
        REQUIRE(result != nullptr);
        REQUIRE(result->data == event_series->getEntityIds()[2]); // Event at 750
    }
}


// ============================================================================
// GapDetector + PolyLineSpatialAdapter Integration Tests
// ============================================================================

TEST_CASE("GapDetector + PolyLineSpatialAdapter - Segmented analog series", "[CorePlotting][Integration][GapDetector]") {
    
    SECTION("GapDetector creates segments from time gaps") {
        // Create time series with a gap in the middle
        // Segment 1: times 0-100, Segment 2: times 200-300 (gap at 100-200)
        std::vector<float> time_values;
        std::vector<float> data_values;
        
        // First segment: 0-100
        for (int i = 0; i <= 100; ++i) {
            time_values.push_back(static_cast<float>(i));
            data_values.push_back(std::sin(static_cast<float>(i) * 0.1f));
        }
        
        // Second segment: 200-300 (gap of 100 time units)
        for (int i = 200; i <= 300; ++i) {
            time_values.push_back(static_cast<float>(i));
            data_values.push_back(std::sin(static_cast<float>(i) * 0.1f));
        }
        
        GapDetector detector;
        detector.setTimeThreshold(50); // Gap threshold of 50 time units
        
        EntityId series_id{42};
        auto batch = detector.transform(time_values, data_values, series_id);
        
        // Should produce 2 segments
        REQUIRE(batch.line_start_indices.size() == 2);
        REQUIRE(batch.line_vertex_counts.size() == 2);
        
        // First segment: 101 vertices (0-100 inclusive)
        REQUIRE(batch.line_vertex_counts[0] == 101);
        
        // Second segment: 101 vertices (200-300 inclusive)
        REQUIRE(batch.line_vertex_counts[1] == 101);
        
        // Global entity ID should be set
        REQUIRE(batch.global_entity_id == series_id);
    }
    
    SECTION("PolyLineSpatialAdapter builds index from segmented batch") {
        // Create a simple 2-segment polyline batch
        std::vector<float> time_values;
        std::vector<float> data_values;
        
        // Segment 1: horizontal line at y=1 from x=0 to x=100
        for (int i = 0; i <= 100; i += 10) {
            time_values.push_back(static_cast<float>(i));
            data_values.push_back(1.0f);
        }
        
        // Segment 2: horizontal line at y=-1 from x=200 to x=300
        for (int i = 200; i <= 300; i += 10) {
            time_values.push_back(static_cast<float>(i));
            data_values.push_back(-1.0f);
        }
        
        GapDetector detector;
        detector.setTimeThreshold(50);
        
        EntityId series_id{100};
        auto batch = detector.transform(time_values, data_values, series_id);
        
        BoundingBox bounds(-10.0f, -2.0f, 350.0f, 2.0f);
        
        // Build spatial index from vertices
        auto index = PolyLineSpatialAdapter::buildFromVertices(batch, bounds);
        
        REQUIRE(index != nullptr);
        REQUIRE(index->size() > 0);
        
        // Query near first segment (x=50, y=1) - should find series_id
        auto const* result1 = index->findNearest(50.0f, 1.0f, 20.0f);
        REQUIRE(result1 != nullptr);
        REQUIRE(result1->data == series_id);
        
        // Query near second segment (x=200, y=-1) - should find series_id
        // Use exact vertex position x=200 which is the first vertex of segment 2
        auto const* result2 = index->findNearest(200.0f, -1.0f, 20.0f);
        REQUIRE(result2 != nullptr);
        REQUIRE(result2->data == series_id);
        
        // Query in the gap (x=150, y=0) with small tolerance - should find nothing
        auto const* result_gap = index->findNearest(150.0f, 0.0f, 5.0f);
        REQUIRE(result_gap == nullptr);
    }
    
    SECTION("Value-based gap detection") {
        // Create time series with a value jump
        std::vector<float> time_values;
        std::vector<float> data_values;
        
        // Smooth segment 1
        for (int i = 0; i <= 50; ++i) {
            time_values.push_back(static_cast<float>(i));
            data_values.push_back(static_cast<float>(i) * 0.1f); // 0 to 5
        }
        
        // Jump to 100, then smooth segment 2
        for (int i = 51; i <= 100; ++i) {
            time_values.push_back(static_cast<float>(i));
            data_values.push_back(100.0f + static_cast<float>(i - 51) * 0.1f); // 100 to 105
        }
        
        GapDetector detector;
        detector.setValueThreshold(10.0f); // Gap if value changes by more than 10
        
        auto batch = detector.transform(time_values, data_values, EntityId{1});
        
        // Should produce 2 segments due to value jump
        REQUIRE(batch.line_start_indices.size() == 2);
    }
}


// ============================================================================
// Mixed Series Scene Tests
// ============================================================================

TEST_CASE("Mixed Series Scene - Layout with multiple series types", "[CorePlotting][Integration][Mixed]") {
    auto time_frame = createSimpleTimeFrame(2000);
    EntityRegistry registry;
    
    // Create different series types
    auto event_series = createEventSeries({100, 500, 1000, 1500}, "events", registry);
    event_series->setTimeFrame(time_frame);
    
    SECTION("StackedLayoutStrategy handles mixed series types") {
        LayoutRequest layout_request;
        layout_request.viewport_y_min = -1.0f;
        layout_request.viewport_y_max = 1.0f;
        layout_request.series = {
            {"analog1", SeriesType::Analog, true},
            {"events", SeriesType::DigitalEvent, true},
            {"analog2", SeriesType::Analog, true},
            {"intervals", SeriesType::DigitalInterval, false} // Full canvas, not stacked
        };
        
        StackedLayoutStrategy strategy;
        LayoutResponse response = strategy.compute(layout_request);
        
        // Should have 4 layouts
        REQUIRE(response.layouts.size() == 4);
        
        // Stackable series (analog1, events, analog2) should have distinct Y positions
        auto const* analog1 = response.findLayout("analog1");
        auto const* events = response.findLayout("events");
        auto const* analog2 = response.findLayout("analog2");
        auto const* intervals = response.findLayout("intervals");
        
        REQUIRE(analog1 != nullptr);
        REQUIRE(events != nullptr);
        REQUIRE(analog2 != nullptr);
        REQUIRE(intervals != nullptr);
        
        // Stackable series should be at different Y centers
        REQUIRE_THAT(analog1->result.allocated_y_center, 
                    !Catch::Matchers::WithinAbs(events->result.allocated_y_center, 0.01f));
        REQUIRE_THAT(events->result.allocated_y_center, 
                    !Catch::Matchers::WithinAbs(analog2->result.allocated_y_center, 0.01f));
        
        // Interval series (full canvas) should span more height
        // In stacked layout, non-stackable series get full viewport
        REQUIRE(intervals->result.allocated_height >= analog1->result.allocated_height);
    }
    
    SECTION("Combined spatial index from events at different Y positions") {
        // Create two event series at different Y positions
        auto events_a = createEventSeries({100, 200, 300}, "events_a", registry);
        events_a->setTimeFrame(time_frame);
        
        auto events_b = createEventSeries({150, 250, 350}, "events_b", registry);
        events_b->setTimeFrame(time_frame);
        
        // Layout them
        LayoutRequest layout_request;
        layout_request.viewport_y_min = -1.0f;
        layout_request.viewport_y_max = 1.0f;
        layout_request.series = {
            {"events_a", SeriesType::DigitalEvent, true},
            {"events_b", SeriesType::DigitalEvent, true}
        };
        
        StackedLayoutStrategy strategy;
        LayoutResponse response = strategy.compute(layout_request);
        
        auto const* layout_a = response.findLayout("events_a");
        auto const* layout_b = response.findLayout("events_b");
        
        REQUIRE(layout_a != nullptr);
        REQUIRE(layout_b != nullptr);
        
        BoundingBox bounds(0.0f, -2.0f, 500.0f, 2.0f);
        
        // Build combined index from both series
        std::vector<glm::vec2> all_positions;
        std::vector<EntityId> all_entity_ids;
        
        // Add events from series A
        for (auto const& event : events_a->view()) {
            float x = static_cast<float>(time_frame->getTimeAtIndex(event.event_time));
            all_positions.emplace_back(x, layout_a->result.allocated_y_center);
            all_entity_ids.push_back(event.entity_id);
        }
        
        // Add events from series B
        for (auto const& event : events_b->view()) {
            float x = static_cast<float>(time_frame->getTimeAtIndex(event.event_time));
            all_positions.emplace_back(x, layout_b->result.allocated_y_center);
            all_entity_ids.push_back(event.entity_id);
        }
        
        auto combined_index = EventSpatialAdapter::buildFromPositions(
            all_positions, all_entity_ids, bounds);
        
        REQUIRE(combined_index != nullptr);
        REQUIRE(combined_index->size() == 6); // 3 + 3 events
        
        // Query for event from series A (at time 200)
        auto const* result_a = combined_index->findNearest(
            200.0f, layout_a->result.allocated_y_center, 10.0f);
        REQUIRE(result_a != nullptr);
        REQUIRE(result_a->data == events_a->getEntityIds()[1]); // index 1 = time 200
        
        // Query for event from series B (at time 250)
        auto const* result_b = combined_index->findNearest(
            250.0f, layout_b->result.allocated_y_center, 10.0f);
        REQUIRE(result_b != nullptr);
        REQUIRE(result_b->data == events_b->getEntityIds()[1]); // index 1 = time 250
        
        // Y position distinguishes events - the two series are at different Y
        float y_a = layout_a->result.allocated_y_center;
        float y_b = layout_b->result.allocated_y_center;
        REQUIRE_THAT(y_a, !Catch::Matchers::WithinAbs(y_b, 0.01f));
        
        // The found events should be at their respective Y positions
        REQUIRE_THAT(result_a->y, Catch::Matchers::WithinAbs(y_a, 0.01f));
        REQUIRE_THAT(result_b->y, Catch::Matchers::WithinAbs(y_b, 0.01f));
    }
    
    SECTION("Analog series layout with events overlay") {
        // Simulate a common use case: analog traces with event markers
        LayoutRequest layout_request;
        layout_request.viewport_y_min = -1.0f;
        layout_request.viewport_y_max = 1.0f;
        layout_request.series = {
            {"neural_trace", SeriesType::Analog, true},
            {"spike_events", SeriesType::DigitalEvent, true},
            {"behavioral_trace", SeriesType::Analog, true},
            {"lick_events", SeriesType::DigitalEvent, true}
        };
        
        StackedLayoutStrategy strategy;
        LayoutResponse response = strategy.compute(layout_request);
        
        REQUIRE(response.layouts.size() == 4);
        
        // All should have non-zero heights
        for (auto const& layout : response.layouts) {
            REQUIRE(layout.result.allocated_height > 0.0f);
        }
        
        // Series should be ordered top to bottom
        float prev_y = std::numeric_limits<float>::max();
        for (auto const& layout : response.layouts) {
            // Each subsequent series should be at same or lower Y
            // (depends on layout strategy, but they should be distinct)
            REQUIRE_THAT(layout.result.allocated_y_center, 
                        !Catch::Matchers::WithinAbs(prev_y, 0.001f));
            prev_y = layout.result.allocated_y_center;
        }
    }
}
