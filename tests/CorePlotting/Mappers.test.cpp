#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/Mappers/MappedElement.hpp"
#include "CorePlotting/Mappers/MappedLineView.hpp"
#include "CorePlotting/Mappers/MapperConcepts.hpp"
#include "CorePlotting/Mappers/TimeSeriesMapper.hpp"
#include "CorePlotting/Mappers/SpatialMapper.hpp"
#include "CorePlotting/Mappers/RasterMapper.hpp"

#include "CorePlotting/Layout/SeriesLayout.hpp"
#include "CorePlotting/Layout/LayoutTransform.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <ranges>
#include <vector>

using namespace CorePlotting;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Test Fixtures
// ============================================================================

namespace {

/**
 * @brief Create a simple TimeFrame with times [0, 10, 20, 30, ...]
 */
std::shared_ptr<TimeFrame> createLinearTimeFrame(int count, int step = 10) {
    std::vector<int> times;
    times.reserve(count);
    for (int i = 0; i < count; ++i) {
        times.push_back(i * step);
    }
    return std::make_shared<TimeFrame>(times);
}

/**
 * @brief Create a simple layout with given center and height
 */
SeriesLayout createLayout(float y_center, float height, std::string id = "test", int index = 0) {
    // y_transform: offset=center, gain=half_height
    return SeriesLayout{
        std::move(id),
        LayoutTransform{y_center, height / 2.0f},
        index
    };
}

} // anonymous namespace

// ============================================================================
// MappedElement Tests
// ============================================================================

TEST_CASE("MappedElement - Basic operations", "[Mappers][MappedElement]") {
    SECTION("Default construction") {
        MappedElement elem;
        // Default constructed, values undefined but should compile
        (void)elem;
    }
    
    SECTION("Value construction") {
        MappedElement elem{1.5f, 2.5f, EntityId{42}};
        REQUIRE(elem.x == 1.5f);
        REQUIRE(elem.y == 2.5f);
        REQUIRE(elem.entity_id == EntityId{42});
    }
    
    SECTION("Position conversion") {
        MappedElement elem{3.0f, 4.0f, EntityId{1}};
        auto pos = elem.position();
        REQUIRE(pos.x == 3.0f);
        REQUIRE(pos.y == 4.0f);
    }
}

TEST_CASE("MappedRectElement - Basic operations", "[Mappers][MappedRectElement]") {
    SECTION("Value construction") {
        MappedRectElement rect{10.0f, 20.0f, 100.0f, 50.0f, EntityId{99}};
        REQUIRE(rect.x == 10.0f);
        REQUIRE(rect.y == 20.0f);
        REQUIRE(rect.width == 100.0f);
        REQUIRE(rect.height == 50.0f);
        REQUIRE(rect.entity_id == EntityId{99});
    }
    
    SECTION("Bounds conversion") {
        MappedRectElement rect{5.0f, 10.0f, 20.0f, 30.0f, EntityId{1}};
        auto bounds = rect.bounds();
        REQUIRE(bounds.x == 5.0f);
        REQUIRE(bounds.y == 10.0f);
        REQUIRE(bounds.z == 20.0f);  // width
        REQUIRE(bounds.w == 30.0f);  // height
    }
    
    SECTION("Center calculation") {
        MappedRectElement rect{0.0f, 0.0f, 100.0f, 50.0f, EntityId{1}};
        auto center = rect.center();
        REQUIRE(center.x == 50.0f);
        REQUIRE(center.y == 25.0f);
    }
}

TEST_CASE("MappedVertex - Basic operations", "[Mappers][MappedVertex]") {
    SECTION("Value construction") {
        MappedVertex v{1.0f, 2.0f};
        REQUIRE(v.x == 1.0f);
        REQUIRE(v.y == 2.0f);
    }
    
    SECTION("Position conversion") {
        MappedVertex v{5.0f, 10.0f};
        auto pos = v.position();
        REQUIRE(pos.x == 5.0f);
        REQUIRE(pos.y == 10.0f);
    }
}

// ============================================================================
// Concept Tests
// ============================================================================

TEST_CASE("MapperConcepts - Type checks", "[Mappers][Concepts]") {
    SECTION("MappedElementLike concept") {
        static_assert(MappedElementLike<MappedElement>);
        static_assert(!MappedElementLike<MappedVertex>);  // No entity_id
        static_assert(!MappedElementLike<int>);
    }
    
    SECTION("MappedRectLike concept") {
        static_assert(MappedRectLike<MappedRectElement>);
        static_assert(!MappedRectLike<MappedElement>);  // No width/height
    }
    
    SECTION("MappedVertexLike concept") {
        static_assert(MappedVertexLike<MappedVertex>);
        static_assert(MappedVertexLike<MappedElement>);  // Also has x,y
    }
}

// ============================================================================
// MappedLineView Tests
// ============================================================================

TEST_CASE("OwningLineView - Basic operations", "[Mappers][LineView]") {
    SECTION("Construction with vertices") {
        std::vector<MappedVertex> verts = {
            {0.0f, 0.0f}, {1.0f, 1.0f}, {2.0f, 0.0f}
        };
        
        OwningLineView view{EntityId{42}, std::move(verts)};
        
        REQUIRE(view.entity_id == EntityId{42});
        auto vertices = view.vertices();
        REQUIRE(vertices.size() == 3);
        REQUIRE(vertices[0].x == 0.0f);
        REQUIRE(vertices[1].x == 1.0f);
        REQUIRE(vertices[2].x == 2.0f);
    }
}

TEST_CASE("SpanLineView - Non-owning view", "[Mappers][LineView]") {
    std::vector<MappedVertex> verts = {
        {0.0f, 0.0f}, {1.0f, 2.0f}, {3.0f, 4.0f}
    };
    
    SpanLineView view{EntityId{10}, verts};
    
    REQUIRE(view.entity_id == EntityId{10});
    auto vertices = view.vertices();
    REQUIRE(vertices.size() == 3);
    REQUIRE(vertices[0].y == 0.0f);
    REQUIRE(vertices[2].y == 4.0f);
}

TEST_CASE("makeLineView - Lazy transformation", "[Mappers][LineView]") {
    std::vector<Point2D<float>> points = {
        {0.0f, 0.0f}, {10.0f, 20.0f}, {30.0f, 40.0f}
    };
    
    SECTION("Identity transform") {
        auto view = makeLineView(EntityId{5}, points);
        
        REQUIRE(view.entity_id == EntityId{5});
        
        std::vector<MappedVertex> collected;
        for (auto const & v : view.vertices()) {
            collected.push_back(v);
        }
        
        REQUIRE(collected.size() == 3);
        REQUIRE(collected[0].x == 0.0f);
        REQUIRE(collected[1].x == 10.0f);
        REQUIRE(collected[2].y == 40.0f);
    }
    
    SECTION("With scaling") {
        auto view = makeLineView(EntityId{5}, points, 2.0f, 0.5f, 0.0f, 0.0f);
        
        std::vector<MappedVertex> collected;
        for (auto const & v : view.vertices()) {
            collected.push_back(v);
        }
        
        REQUIRE(collected[1].x == 20.0f);  // 10 * 2
        REQUIRE(collected[1].y == 10.0f);  // 20 * 0.5
    }
    
    SECTION("With offset") {
        auto view = makeLineView(EntityId{5}, points, 1.0f, 1.0f, 100.0f, 50.0f);
        
        std::vector<MappedVertex> collected;
        for (auto const & v : view.vertices()) {
            collected.push_back(v);
        }
        
        REQUIRE(collected[0].x == 100.0f);  // 0 + 100
        REQUIRE(collected[0].y == 50.0f);   // 0 + 50
    }
}

// ============================================================================
// TimeSeriesMapper Tests
// ============================================================================

TEST_CASE("TimeSeriesMapper::mapEvents", "[Mappers][TimeSeriesMapper]") {
    auto tf = createLinearTimeFrame(100);  // [0, 10, 20, ..., 990]
    
    // Create event series
    DigitalEventSeries events({
        TimeFrameIndex{5},   // time = 50
        TimeFrameIndex{10},  // time = 100
        TimeFrameIndex{20}   // time = 200
    });
    events.setTimeFrame(tf);
    
    auto layout = createLayout(0.5f, 0.2f);
    
    SECTION("Maps all events correctly") {
        auto mapped = TimeSeriesMapper::mapEvents(events, layout, *tf);
        
        std::vector<MappedElement> collected;
        for (auto const & elem : mapped) {
            collected.push_back(elem);
        }
        
        REQUIRE(collected.size() == 3);
        
        // Check X positions (absolute time)
        REQUIRE(collected[0].x == 50.0f);
        REQUIRE(collected[1].x == 100.0f);
        REQUIRE(collected[2].x == 200.0f);
        
        // All events at same Y (layout center)
        REQUIRE(collected[0].y == 0.5f);
        REQUIRE(collected[1].y == 0.5f);
        REQUIRE(collected[2].y == 0.5f);
    }
    
    SECTION("Materialized version") {
        auto vec = TimeSeriesMapper::mapEventsToVector(events, layout, *tf);
        
        REQUIRE(vec.size() == 3);
        REQUIRE(vec[0].x == 50.0f);
    }
}

TEST_CASE("TimeSeriesMapper::mapIntervals", "[Mappers][TimeSeriesMapper]") {
    auto tf = createLinearTimeFrame(100);
    
    // Create interval series
    DigitalIntervalSeries intervals;
    intervals.setTimeFrame(tf);
    intervals.addEvent(TimeFrameIndex{0}, TimeFrameIndex{10});   // [0, 100]
    intervals.addEvent(TimeFrameIndex{20}, TimeFrameIndex{30});  // [200, 300]
    
    auto layout = createLayout(0.0f, 1.0f);  // center=0, height=1 -> y_bottom=-0.5
    
    SECTION("Maps intervals to rectangles") {
        auto mapped = TimeSeriesMapper::mapIntervals(intervals, layout, *tf);
        
        std::vector<MappedRectElement> collected;
        for (auto const & rect : mapped) {
            collected.push_back(rect);
        }
        
        REQUIRE(collected.size() == 2);
        
        // First interval: [0, 100]
        REQUIRE(collected[0].x == 0.0f);
        REQUIRE(collected[0].width == 100.0f);
        REQUIRE_THAT(collected[0].y, WithinAbs(-0.5f, 0.001));
        REQUIRE(collected[0].height == 1.0f);
        
        // Second interval: [200, 300]
        REQUIRE(collected[1].x == 200.0f);
        REQUIRE(collected[1].width == 100.0f);
    }
}

TEST_CASE("TimeSeriesMapper::mapAnalogSeries", "[Mappers][TimeSeriesMapper]") {
    auto tf = createLinearTimeFrame(10);  // [0, 10, 20, ..., 90]
    
    // Create analog series
    std::vector<float> values = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};
    std::vector<TimeFrameIndex> times;
    for (int i = 0; i < 10; ++i) {
        times.emplace_back(i);
    }
    AnalogTimeSeries analog(std::move(values), std::move(times));
    analog.setTimeFrame(tf);
    
    auto layout = createLayout(0.0f, 2.0f);
    
    SECTION("Maps with identity scale") {
        auto mapped = TimeSeriesMapper::mapAnalogSeries(
            analog, layout, *tf, 1.0f,
            TimeFrameIndex{0}, TimeFrameIndex{9}
        );
        
        std::vector<MappedVertex> collected;
        for (auto const & v : mapped) {
            collected.push_back(v);
        }
        
        REQUIRE(collected.size() == 10);
        
        // X = absolute time
        REQUIRE(collected[0].x == 0.0f);
        REQUIRE(collected[5].x == 50.0f);
        
        // Y = value * scale + offset
        REQUIRE(collected[0].y == 0.0f);   // 0 * 1 + 0
        REQUIRE(collected[5].y == 5.0f);   // 5 * 1 + 0
    }
    
    SECTION("Maps with custom scale and offset") {
        auto layout_offset = createLayout(10.0f, 2.0f);  // offset = 10
        auto mapped = TimeSeriesMapper::mapAnalogSeries(
            analog, layout_offset, *tf, 0.5f,  // scale = 0.5
            TimeFrameIndex{0}, TimeFrameIndex{9}
        );
        
        std::vector<MappedVertex> collected;
        for (auto const & v : mapped) {
            collected.push_back(v);
        }
        
        // Y = value * 0.5 + 10
        REQUIRE_THAT(collected[0].y, WithinAbs(10.0f, 0.001));    // 0 * 0.5 + 10
        REQUIRE_THAT(collected[4].y, WithinAbs(12.0f, 0.001));    // 4 * 0.5 + 10
    }
}

// ============================================================================
// RasterMapper Tests
// ============================================================================

TEST_CASE("RasterMapper::mapEventsRelative", "[Mappers][RasterMapper]") {
    auto tf = createLinearTimeFrame(100);
    
    // Events at times 40, 50, 60, 70
    DigitalEventSeries events({
        TimeFrameIndex{4},
        TimeFrameIndex{5},
        TimeFrameIndex{6},
        TimeFrameIndex{7}
    });
    events.setTimeFrame(tf);
    
    auto layout = createLayout(0.5f, 0.1f);
    TimeFrameIndex reference{5};  // Reference at time 50
    
    SECTION("Computes relative time") {
        auto mapped = RasterMapper::mapEventsRelative(events, layout, *tf, reference);
        
        std::vector<MappedElement> collected;
        for (auto const & elem : mapped) {
            collected.push_back(elem);
        }
        
        REQUIRE(collected.size() == 4);
        
        // X = event_time - reference_time
        REQUIRE(collected[0].x == -10.0f);  // 40 - 50
        REQUIRE(collected[1].x == 0.0f);    // 50 - 50
        REQUIRE(collected[2].x == 10.0f);   // 60 - 50
        REQUIRE(collected[3].x == 20.0f);   // 70 - 50
        
        // Y = layout center
        REQUIRE(collected[0].y == 0.5f);
    }
}

TEST_CASE("RasterMapper::mapEventsInWindow", "[Mappers][RasterMapper]") {
    auto tf = createLinearTimeFrame(100);
    
    // Events spread over time
    DigitalEventSeries events({
        TimeFrameIndex{0},   // time 0
        TimeFrameIndex{4},   // time 40
        TimeFrameIndex{5},   // time 50 (reference)
        TimeFrameIndex{6},   // time 60
        TimeFrameIndex{10}   // time 100
    });
    events.setTimeFrame(tf);
    
    auto layout = createLayout(0.0f, 0.1f);
    TimeFrameIndex reference{5};  // time 50
    
    SECTION("Filters events outside window") {
        // Window: [50-30, 50+20] = [20, 70]
        auto mapped = RasterMapper::mapEventsInWindow(
            events, layout, *tf, reference, 30, 20
        );
        
        std::vector<MappedElement> collected;
        for (auto const & elem : mapped) {
            collected.push_back(elem);
        }
        
        // Only events at times 40, 50, 60 should pass (20 <= t <= 70)
        REQUIRE(collected.size() == 3);
        REQUIRE(collected[0].x == -10.0f);  // 40 - 50
        REQUIRE(collected[1].x == 0.0f);    // 50 - 50
        REQUIRE(collected[2].x == 10.0f);   // 60 - 50
    }
}

TEST_CASE("RasterMapper::computeRowYCenter", "[Mappers][RasterMapper]") {
    SECTION("Single row") {
        float y = RasterMapper::computeRowYCenter(0, 1, -1.0f, 1.0f);
        REQUIRE(y == 0.0f);  // Center of [-1, 1]
    }
    
    SECTION("Two rows") {
        float y0 = RasterMapper::computeRowYCenter(0, 2, -1.0f, 1.0f);
        float y1 = RasterMapper::computeRowYCenter(1, 2, -1.0f, 1.0f);
        
        REQUIRE_THAT(y0, WithinAbs(0.5f, 0.001));   // Top row center
        REQUIRE_THAT(y1, WithinAbs(-0.5f, 0.001));  // Bottom row center
    }
    
    SECTION("Four rows") {
        float y0 = RasterMapper::computeRowYCenter(0, 4, 0.0f, 1.0f);
        float y1 = RasterMapper::computeRowYCenter(1, 4, 0.0f, 1.0f);
        float y2 = RasterMapper::computeRowYCenter(2, 4, 0.0f, 1.0f);
        float y3 = RasterMapper::computeRowYCenter(3, 4, 0.0f, 1.0f);
        
        // Row height = 0.25, centers at 0.875, 0.625, 0.375, 0.125
        REQUIRE_THAT(y0, WithinAbs(0.875f, 0.001));
        REQUIRE_THAT(y1, WithinAbs(0.625f, 0.001));
        REQUIRE_THAT(y2, WithinAbs(0.375f, 0.001));
        REQUIRE_THAT(y3, WithinAbs(0.125f, 0.001));
    }
}

TEST_CASE("RasterMapper::makeRowLayout", "[Mappers][RasterMapper]") {
    auto layout = RasterMapper::makeRowLayout(1, 4, "trial_1", -1.0f, 1.0f);
    
    REQUIRE(layout.series_id == "trial_1");
    REQUIRE(layout.series_index == 1);
    // y_transform.gain is half_height, so height = gain * 2
    REQUIRE(layout.y_transform.gain * 2.0f == 0.5f);  // 2.0 / 4
    
    // Y center for row 1 of 4 in [-1, 1]
    float expected_y = RasterMapper::computeRowYCenter(1, 4, -1.0f, 1.0f);
    REQUIRE_THAT(layout.y_transform.offset, WithinAbs(expected_y, 0.001));
}

TEST_CASE("RasterMapper::mapTrials", "[Mappers][RasterMapper]") {
    auto tf = createLinearTimeFrame(100);
    
    // Trial 1: events at times 45, 55 relative to reference 50
    DigitalEventSeries trial1_events({TimeFrameIndex{4}, TimeFrameIndex{6}});
    trial1_events.setTimeFrame(tf);
    
    // Trial 2: events at times 75, 85 relative to reference 80
    DigitalEventSeries trial2_events({TimeFrameIndex{7}, TimeFrameIndex{9}});
    trial2_events.setTimeFrame(tf);
    
    std::vector<RasterMapper::TrialConfig> trials = {
        {&trial1_events, TimeFrameIndex{5}, RasterMapper::makeRowLayout(0, 2, "trial1")},
        {&trial2_events, TimeFrameIndex{8}, RasterMapper::makeRowLayout(1, 2, "trial2")}
    };
    
    auto mapped = RasterMapper::mapTrials(trials, *tf);
    
    REQUIRE(mapped.size() == 4);
    
    // Trial 1 events: relative to time 50
    REQUIRE(mapped[0].x == -10.0f);  // 40 - 50
    REQUIRE(mapped[1].x == 10.0f);   // 60 - 50
    
    // Trial 2 events: relative to time 80
    REQUIRE(mapped[2].x == -10.0f);  // 70 - 80
    REQUIRE(mapped[3].x == 10.0f);   // 90 - 80
    
    // Different Y positions for different trials
    REQUIRE(mapped[0].y == mapped[1].y);  // Same trial
    REQUIRE(mapped[2].y == mapped[3].y);  // Same trial
    REQUIRE(mapped[0].y != mapped[2].y);  // Different trials
}

// ============================================================================
// SpatialMapper Tests
// ============================================================================

TEST_CASE("SpatialMapper::mapPoint", "[Mappers][SpatialMapper]") {
    Point2D<float> pt{10.0f, 20.0f};
    
    SECTION("Identity transform") {
        auto mapped = SpatialMapper::mapPoint(pt, EntityId{1});
        REQUIRE(mapped.x == 10.0f);
        REQUIRE(mapped.y == 20.0f);
        REQUIRE(mapped.entity_id == EntityId{1});
    }
    
    SECTION("With scaling") {
        auto mapped = SpatialMapper::mapPoint(pt, EntityId{1}, 2.0f, 0.5f);
        REQUIRE(mapped.x == 20.0f);   // 10 * 2
        REQUIRE(mapped.y == 10.0f);   // 20 * 0.5
    }
    
    SECTION("With offset") {
        auto mapped = SpatialMapper::mapPoint(pt, EntityId{1}, 1.0f, 1.0f, 5.0f, -5.0f);
        REQUIRE(mapped.x == 15.0f);   // 10 + 5
        REQUIRE(mapped.y == 15.0f);   // 20 - 5
    }
}

TEST_CASE("SpatialMapper::mapLine", "[Mappers][SpatialMapper]") {
    Line2D line;
    line.push_back(Point2D<float>{0.0f, 0.0f});
    line.push_back(Point2D<float>{10.0f, 20.0f});
    line.push_back(Point2D<float>{30.0f, 40.0f});
    
    SECTION("Creates OwningLineView with transformed vertices") {
        auto view = SpatialMapper::mapLine(line, EntityId{42}, 2.0f, 0.5f, 10.0f, 5.0f);
        
        REQUIRE(view.entity_id == EntityId{42});
        
        auto vertices = view.vertices();
        REQUIRE(vertices.size() == 3);
        
        // First vertex: (0,0) -> (0*2+10, 0*0.5+5) = (10, 5)
        REQUIRE(vertices[0].x == 10.0f);
        REQUIRE(vertices[0].y == 5.0f);
        
        // Second vertex: (10,20) -> (10*2+10, 20*0.5+5) = (30, 15)
        REQUIRE(vertices[1].x == 30.0f);
        REQUIRE(vertices[1].y == 15.0f);
    }
}

TEST_CASE("SpatialMapper::extractPositions", "[Mappers][SpatialMapper]") {
    std::vector<MappedElement> elements = {
        {1.0f, 2.0f, EntityId{1}},
        {3.0f, 4.0f, EntityId{2}},
        {5.0f, 6.0f, EntityId{3}}
    };
    
    auto positions = SpatialMapper::extractPositions(elements);
    
    REQUIRE(positions.size() == 3);
    REQUIRE(positions[0] == glm::vec2{1.0f, 2.0f});
    REQUIRE(positions[1] == glm::vec2{3.0f, 4.0f});
    REQUIRE(positions[2] == glm::vec2{5.0f, 6.0f});
}

TEST_CASE("SpatialMapper::extractEntityIds", "[Mappers][SpatialMapper]") {
    std::vector<MappedElement> elements = {
        {1.0f, 2.0f, EntityId{10}},
        {3.0f, 4.0f, EntityId{20}},
        {5.0f, 6.0f, EntityId{30}}
    };
    
    auto ids = SpatialMapper::extractEntityIds(elements);
    
    REQUIRE(ids.size() == 3);
    REQUIRE(ids[0] == EntityId{10});
    REQUIRE(ids[1] == EntityId{20});
    REQUIRE(ids[2] == EntityId{30});
}
