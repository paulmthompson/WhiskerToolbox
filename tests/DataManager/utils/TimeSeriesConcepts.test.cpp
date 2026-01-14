/**
 * @file TimeSeriesConcepts.test.cpp
 * @brief Unit tests for TimeSeriesConcepts.hpp
 * 
 * This file verifies that all time series element types satisfy their
 * respective concepts using static_assert at compile time, and provides
 * runtime tests for the concept-based utility functions.
 * 
 * Note: The main `elements()` method on containers returns pair-based types
 * for backward compatibility with TransformPipeline. The `elementsView()` 
 * method returns concept-compliant element types (TimeValuePoint, FlatElement, 
 * RaggedElement, etc.) for use with generic algorithms.
 */

#include "utils/TimeSeriesConcepts.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DigitalTimeSeries/EventWithId.hpp"
#include "DigitalTimeSeries/IntervalWithId.hpp"
#include "utils/RaggedTimeSeries.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace WhiskerToolbox::Concepts;

// =============================================================================
// Compile-Time Concept Verification (static_assert)
// =============================================================================

// ========== TimeValuePoint (AnalogTimeSeries) ==========
static_assert(TimeSeriesElement<AnalogTimeSeries::TimeValuePoint>,
    "TimeValuePoint must satisfy TimeSeriesElement concept");
static_assert(!EntityElement<AnalogTimeSeries::TimeValuePoint>,
    "TimeValuePoint must NOT satisfy EntityElement concept (no EntityId)");
static_assert(ValueElement<AnalogTimeSeries::TimeValuePoint, float>,
    "TimeValuePoint must satisfy ValueElement<float> concept");

// ========== FlatElement (RaggedAnalogTimeSeries) ==========
static_assert(TimeSeriesElement<RaggedAnalogTimeSeries::FlatElement>,
    "FlatElement must satisfy TimeSeriesElement concept");
static_assert(!EntityElement<RaggedAnalogTimeSeries::FlatElement>,
    "FlatElement must NOT satisfy EntityElement concept (no EntityId)");
static_assert(ValueElement<RaggedAnalogTimeSeries::FlatElement, float>,
    "FlatElement must satisfy ValueElement<float> concept");

// ========== EventWithId (DigitalEventSeries) ==========
static_assert(TimeSeriesElement<EventWithId>,
    "EventWithId must satisfy TimeSeriesElement concept");
static_assert(EntityElement<EventWithId>,
    "EventWithId must satisfy EntityElement concept");
static_assert(ValueElement<EventWithId, TimeFrameIndex>,
    "EventWithId must satisfy ValueElement<TimeFrameIndex> concept");
static_assert(FullElement<EventWithId, TimeFrameIndex>,
    "EventWithId must satisfy FullElement<TimeFrameIndex> concept");

// ========== IntervalWithId (DigitalIntervalSeries) ==========
static_assert(TimeSeriesElement<IntervalWithId>,
    "IntervalWithId must satisfy TimeSeriesElement concept");
static_assert(EntityElement<IntervalWithId>,
    "IntervalWithId must satisfy EntityElement concept");
static_assert(ValueElement<IntervalWithId, Interval const&>,
    "IntervalWithId must satisfy ValueElement<Interval const&> concept");
static_assert(FullElement<IntervalWithId, Interval const&>,
    "IntervalWithId must satisfy FullElement<Interval const&> concept");

// ========== RaggedElement<Line2D> (RaggedTimeSeries<Line2D>) ==========
static_assert(TimeSeriesElement<RaggedTimeSeries<Line2D>::RaggedElement>,
    "RaggedElement<Line2D> must satisfy TimeSeriesElement concept");
static_assert(EntityElement<RaggedTimeSeries<Line2D>::RaggedElement>,
    "RaggedElement<Line2D> must satisfy EntityElement concept");
static_assert(ValueElement<RaggedTimeSeries<Line2D>::RaggedElement, Line2D const&>,
    "RaggedElement<Line2D> must satisfy ValueElement<Line2D const&> concept");
static_assert(FullElement<RaggedTimeSeries<Line2D>::RaggedElement, Line2D const&>,
    "RaggedElement<Line2D> must satisfy FullElement<Line2D const&> concept");

// ========== RaggedElement<Mask2D> (RaggedTimeSeries<Mask2D>) ==========
static_assert(TimeSeriesElement<RaggedTimeSeries<Mask2D>::RaggedElement>,
    "RaggedElement<Mask2D> must satisfy TimeSeriesElement concept");
static_assert(EntityElement<RaggedTimeSeries<Mask2D>::RaggedElement>,
    "RaggedElement<Mask2D> must satisfy EntityElement concept");

// ========== RaggedElement<Point2D<float>> (RaggedTimeSeries<Point2D<float>>) ==========
static_assert(TimeSeriesElement<RaggedTimeSeries<Point2D<float>>::RaggedElement>,
    "RaggedElement<Point2D<float>> must satisfy TimeSeriesElement concept");
static_assert(EntityElement<RaggedTimeSeries<Point2D<float>>::RaggedElement>,
    "RaggedElement<Point2D<float>> must satisfy EntityElement concept");

// ========== Type Traits ==========
static_assert(is_time_series_element_v<EventWithId>, 
    "is_time_series_element_v should be true for EventWithId");
static_assert(is_entity_element_v<EventWithId>,
    "is_entity_element_v should be true for EventWithId");
static_assert(!is_entity_element_v<AnalogTimeSeries::TimeValuePoint>,
    "is_entity_element_v should be false for TimeValuePoint");

// =============================================================================
// Runtime Tests
// =============================================================================

TEST_CASE("TimeSeriesConcepts - Utility Functions", "[concepts][timeseries][unit]") {
    
    SECTION("getTime extracts time from TimeValuePoint") {
        AnalogTimeSeries::TimeValuePoint tvp{TimeFrameIndex(100), 3.14f};
        
        REQUIRE(getTime(tvp) == TimeFrameIndex(100));
        REQUIRE(tvp.time() == TimeFrameIndex(100));
        REQUIRE(tvp.value() == 3.14f);
    }
    
    SECTION("getTime extracts time from FlatElement") {
        RaggedAnalogTimeSeries::FlatElement elem{TimeFrameIndex(200), 2.71f};
        
        REQUIRE(getTime(elem) == TimeFrameIndex(200));
        REQUIRE(elem.time() == TimeFrameIndex(200));
        REQUIRE(elem.value() == 2.71f);
    }
    
    SECTION("getTime and getEntityId extract from EventWithId") {
        EventWithId event{TimeFrameIndex(300), EntityId(42)};
        
        REQUIRE(getTime(event) == TimeFrameIndex(300));
        REQUIRE(getEntityId(event) == EntityId(42));
        REQUIRE(event.time() == TimeFrameIndex(300));
        REQUIRE(event.id() == EntityId(42));
        REQUIRE(event.value() == TimeFrameIndex(300));
    }
    
    SECTION("getTime and getEntityId extract from IntervalWithId") {
        Interval interval{100, 200};
        IntervalWithId iwid{interval, EntityId(99)};
        
        // time() returns start of interval
        REQUIRE(getTime(iwid) == TimeFrameIndex(100));
        REQUIRE(getEntityId(iwid) == EntityId(99));
        REQUIRE(iwid.time() == TimeFrameIndex(100));
        REQUIRE(iwid.id() == EntityId(99));
        REQUIRE(iwid.value().start == 100);
        REQUIRE(iwid.value().end == 200);
    }
    
    SECTION("isInTimeRange checks time bounds") {
        AnalogTimeSeries::TimeValuePoint tvp{TimeFrameIndex(50), 1.0f};
        
        REQUIRE(isInTimeRange(tvp, TimeFrameIndex(0), TimeFrameIndex(100)));
        REQUIRE(isInTimeRange(tvp, TimeFrameIndex(50), TimeFrameIndex(50)));
        REQUIRE(isInTimeRange(tvp, TimeFrameIndex(50), TimeFrameIndex(100)));
        REQUIRE(isInTimeRange(tvp, TimeFrameIndex(0), TimeFrameIndex(50)));
        
        REQUIRE_FALSE(isInTimeRange(tvp, TimeFrameIndex(0), TimeFrameIndex(49)));
        REQUIRE_FALSE(isInTimeRange(tvp, TimeFrameIndex(51), TimeFrameIndex(100)));
    }
    
    SECTION("isInEntitySet checks EntityId membership") {
        EventWithId event1{TimeFrameIndex(0), EntityId(10)};
        EventWithId event2{TimeFrameIndex(0), EntityId(20)};
        EventWithId event3{TimeFrameIndex(0), EntityId(30)};
        
        std::unordered_set<EntityId> selected_ids{EntityId(10), EntityId(30)};
        
        REQUIRE(isInEntitySet(event1, selected_ids));
        REQUIRE_FALSE(isInEntitySet(event2, selected_ids));
        REQUIRE(isInEntitySet(event3, selected_ids));
    }
}

TEST_CASE("TimeSeriesConcepts - RaggedElement Accessors", "[concepts][ragged][unit]") {
    
    SECTION("RaggedElement provides all accessors for Line2D") {
        Line2D line{{0, 0}, {10, 10}};
        RaggedTimeSeries<Line2D>::RaggedElement elem{TimeFrameIndex(500), line, EntityId(77)};
        
        REQUIRE(elem.time() == TimeFrameIndex(500));
        REQUIRE(elem.id() == EntityId(77));
        REQUIRE(elem.entity_id() == EntityId(77));  // Legacy accessor
        REQUIRE(elem.data().size() == 2);
        REQUIRE(elem.value().size() == 2);  // Alias for data()
        
        // Verify concept functions work
        REQUIRE(getTime(elem) == TimeFrameIndex(500));
        REQUIRE(getEntityId(elem) == EntityId(77));
    }
    
    SECTION("RaggedElement provides all accessors for Mask2D") {
        Mask2D mask{{1, 2}, {3, 4}, {5, 6}};
        RaggedTimeSeries<Mask2D>::RaggedElement elem{TimeFrameIndex(600), mask, EntityId(88)};
        
        REQUIRE(elem.time() == TimeFrameIndex(600));
        REQUIRE(elem.id() == EntityId(88));
        REQUIRE(elem.data().size() == 3);
        
        // Verify concept functions work
        REQUIRE(getTime(elem) == TimeFrameIndex(600));
        REQUIRE(getEntityId(elem) == EntityId(88));
    }
    
    SECTION("RaggedElement provides all accessors for Point2D<float>") {
        Point2D<float> pt{1.5f, 2.5f};
        RaggedTimeSeries<Point2D<float>>::RaggedElement elem{TimeFrameIndex(700), pt, EntityId(99)};
        
        REQUIRE(elem.time() == TimeFrameIndex(700));
        REQUIRE(elem.id() == EntityId(99));
        REQUIRE(elem.data().x == 1.5f);
        REQUIRE(elem.data().y == 2.5f);
        
        // Verify concept functions work
        REQUIRE(getTime(elem) == TimeFrameIndex(700));
        REQUIRE(getEntityId(elem) == EntityId(99));
    }
}

TEST_CASE("TimeSeriesConcepts - Generic Algorithm Simulation", "[concepts][algorithm][unit]") {
    
    SECTION("Filter by time range - generic across types") {
        // This simulates a generic filter algorithm using concepts
        auto checkTimeInRange = []<TimeSeriesElement T>(
            std::vector<T> const& elements,
            TimeFrameIndex start,
            TimeFrameIndex end) {
            size_t count = 0;
            for (auto const& elem : elements) {
                if (isInTimeRange(elem, start, end)) {
                    ++count;
                }
            }
            return count;
        };
        
        // Test with TimeValuePoint
        std::vector<AnalogTimeSeries::TimeValuePoint> tvps{
            {TimeFrameIndex(10), 1.0f},
            {TimeFrameIndex(20), 2.0f},
            {TimeFrameIndex(30), 3.0f}
        };
        REQUIRE(checkTimeInRange(tvps, TimeFrameIndex(15), TimeFrameIndex(25)) == 1);
        REQUIRE(checkTimeInRange(tvps, TimeFrameIndex(0), TimeFrameIndex(100)) == 3);
        
        // Test with EventWithId
        std::vector<EventWithId> events{
            EventWithId{TimeFrameIndex(10), EntityId(1)},
            EventWithId{TimeFrameIndex(20), EntityId(2)},
            EventWithId{TimeFrameIndex(30), EntityId(3)}
        };
        REQUIRE(checkTimeInRange(events, TimeFrameIndex(15), TimeFrameIndex(25)) == 1);
        REQUIRE(checkTimeInRange(events, TimeFrameIndex(0), TimeFrameIndex(100)) == 3);
    }
    
    SECTION("Filter by EntityId - only for EntityElement types") {
        auto filterByEntitySet = []<EntityElement T>(
            std::vector<T> const& elements,
            std::unordered_set<EntityId> const& ids) {
            std::vector<T> result;
            for (auto const& elem : elements) {
                if (isInEntitySet(elem, ids)) {
                    result.push_back(elem);
                }
            }
            return result;
        };
        
        std::vector<EventWithId> events{
            EventWithId{TimeFrameIndex(10), EntityId(1)},
            EventWithId{TimeFrameIndex(20), EntityId(2)},
            EventWithId{TimeFrameIndex(30), EntityId(3)}
        };
        
        std::unordered_set<EntityId> selected{EntityId(1), EntityId(3)};
        auto filtered = filterByEntitySet(events, selected);
        
        REQUIRE(filtered.size() == 2);
        REQUIRE(filtered[0].id() == EntityId(1));
        REQUIRE(filtered[1].id() == EntityId(3));
    }
}
