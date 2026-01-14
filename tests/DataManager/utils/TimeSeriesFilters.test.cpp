/**
 * @file TimeSeriesFilters.test.cpp
 * @brief Unit tests for TimeSeriesFilters.hpp
 * 
 * This file verifies the generic filter utilities work correctly with all
 * time series element types. Tests cover time range filtering, EntityId
 * filtering, combined filters, and utility functions.
 */

#include "utils/TimeSeriesFilters.hpp"
#include "utils/TimeSeriesConcepts.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DigitalTimeSeries/EventWithId.hpp"
#include "DigitalTimeSeries/IntervalWithId.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "utils/RaggedTimeSeries.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <ranges>

using namespace WhiskerToolbox::Filters;
using namespace WhiskerToolbox::Concepts;

// =============================================================================
// Time Range Filtering Tests
// =============================================================================

TEST_CASE("TimeSeriesFilters - filterByTimeRange", "[filters][time][unit]") {
    
    SECTION("Filter TimeValuePoint vector") {
        std::vector<AnalogTimeSeries::TimeValuePoint> points{
            {TimeFrameIndex(10), 1.0f},
            {TimeFrameIndex(20), 2.0f},
            {TimeFrameIndex(30), 3.0f},
            {TimeFrameIndex(40), 4.0f},
            {TimeFrameIndex(50), 5.0f}
        };
        
        auto filtered = filterByTimeRange(points, TimeFrameIndex(20), TimeFrameIndex(40));
        auto result = materializeToVector(filtered);
        
        REQUIRE(result.size() == 3);
        REQUIRE(result[0].time() == TimeFrameIndex(20));
        REQUIRE(result[1].time() == TimeFrameIndex(30));
        REQUIRE(result[2].time() == TimeFrameIndex(40));
    }
    
    SECTION("Filter FlatElement vector") {
        std::vector<RaggedAnalogTimeSeries::FlatElement> elements{
            {TimeFrameIndex(100), 1.0f},
            {TimeFrameIndex(200), 2.0f},
            {TimeFrameIndex(300), 3.0f}
        };
        
        auto filtered = filterByTimeRange(elements, TimeFrameIndex(150), TimeFrameIndex(250));
        auto result = materializeToVector(filtered);
        
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].time() == TimeFrameIndex(200));
    }
    
    SECTION("Filter EventWithId vector") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)},
            {TimeFrameIndex(30), EntityId(3)}
        };
        
        auto filtered = filterByTimeRange(events, TimeFrameIndex(15), TimeFrameIndex(25));
        auto result = materializeToVector(filtered);
        
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].time() == TimeFrameIndex(20));
        REQUIRE(result[0].id() == EntityId(2));
    }
    
    SECTION("Filter IntervalWithId vector") {
        std::vector<IntervalWithId> intervals{
            {Interval{100, 150}, EntityId(1)},
            {Interval{200, 250}, EntityId(2)},
            {Interval{300, 350}, EntityId(3)}
        };
        
        // Filter by start time (time() returns interval.start)
        auto filtered = filterByTimeRange(intervals, TimeFrameIndex(150), TimeFrameIndex(250));
        auto result = materializeToVector(filtered);
        
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].time() == TimeFrameIndex(200));
    }
    
    SECTION("Empty result when no elements in range") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)}
        };
        
        auto filtered = filterByTimeRange(events, TimeFrameIndex(100), TimeFrameIndex(200));
        auto result = materializeToVector(filtered);
        
        REQUIRE(result.empty());
    }
    
    SECTION("All elements when range covers all") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)},
            {TimeFrameIndex(30), EntityId(3)}
        };
        
        auto filtered = filterByTimeRange(events, TimeFrameIndex(0), TimeFrameIndex(100));
        auto result = materializeToVector(filtered);
        
        REQUIRE(result.size() == 3);
    }
    
    SECTION("Boundary inclusion (inclusive)") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)},
            {TimeFrameIndex(30), EntityId(3)}
        };
        
        // Exact boundaries should be included
        auto filtered = filterByTimeRange(events, TimeFrameIndex(10), TimeFrameIndex(30));
        auto result = materializeToVector(filtered);
        
        REQUIRE(result.size() == 3);
    }
}

TEST_CASE("TimeSeriesFilters - filterByTimeRangeExclusive", "[filters][time][unit]") {
    
    SECTION("Exclusive end boundary") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)},
            {TimeFrameIndex(30), EntityId(3)}
        };
        
        // End is exclusive, so 30 should NOT be included
        auto filtered = filterByTimeRangeExclusive(events, TimeFrameIndex(10), TimeFrameIndex(30));
        auto result = materializeToVector(filtered);
        
        REQUIRE(result.size() == 2);
        REQUIRE(result[0].time() == TimeFrameIndex(10));
        REQUIRE(result[1].time() == TimeFrameIndex(20));
    }
}

// =============================================================================
// EntityId Filtering Tests
// =============================================================================

TEST_CASE("TimeSeriesFilters - filterByEntityIds", "[filters][entity][unit]") {
    
    SECTION("Filter EventWithId by EntityId set") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)},
            {TimeFrameIndex(30), EntityId(3)},
            {TimeFrameIndex(40), EntityId(4)},
            {TimeFrameIndex(50), EntityId(5)}
        };
        
        std::unordered_set<EntityId> selected{EntityId(2), EntityId(4)};
        auto filtered = filterByEntityIds(events, selected);
        auto result = materializeToVector(filtered);
        
        REQUIRE(result.size() == 2);
        REQUIRE(result[0].id() == EntityId(2));
        REQUIRE(result[1].id() == EntityId(4));
    }
    
    SECTION("Filter IntervalWithId by EntityId set") {
        std::vector<IntervalWithId> intervals{
            {Interval{100, 150}, EntityId(10)},
            {Interval{200, 250}, EntityId(20)},
            {Interval{300, 350}, EntityId(30)}
        };
        
        std::unordered_set<EntityId> selected{EntityId(10), EntityId(30)};
        auto filtered = filterByEntityIds(intervals, selected);
        auto result = materializeToVector(filtered);
        
        REQUIRE(result.size() == 2);
        REQUIRE(result[0].id() == EntityId(10));
        REQUIRE(result[1].id() == EntityId(30));
    }
    
    SECTION("Empty result when no matching EntityIds") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)}
        };
        
        std::unordered_set<EntityId> selected{EntityId(100), EntityId(200)};
        auto filtered = filterByEntityIds(events, selected);
        auto result = materializeToVector(filtered);
        
        REQUIRE(result.empty());
    }
    
    SECTION("All elements when all EntityIds selected") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)},
            {TimeFrameIndex(30), EntityId(3)}
        };
        
        std::unordered_set<EntityId> selected{EntityId(1), EntityId(2), EntityId(3)};
        auto filtered = filterByEntityIds(events, selected);
        auto result = materializeToVector(filtered);
        
        REQUIRE(result.size() == 3);
    }
}

TEST_CASE("TimeSeriesFilters - filterByEntityId (single)", "[filters][entity][unit]") {
    
    SECTION("Filter by single EntityId") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)},
            {TimeFrameIndex(30), EntityId(1)},
            {TimeFrameIndex(40), EntityId(3)},
            {TimeFrameIndex(50), EntityId(1)}
        };
        
        auto filtered = filterByEntityId(events, EntityId(1));
        auto result = materializeToVector(filtered);
        
        REQUIRE(result.size() == 3);
        for (auto const& event : result) {
            REQUIRE(event.id() == EntityId(1));
        }
    }
}

// =============================================================================
// Combined Filtering Tests
// =============================================================================

TEST_CASE("TimeSeriesFilters - filterByTimeRangeAndEntityIds", "[filters][combined][unit]") {
    
    SECTION("Combined time and entity filtering") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},  // In range, in set
            {TimeFrameIndex(20), EntityId(2)},  // In range, not in set
            {TimeFrameIndex(30), EntityId(1)},  // In range, in set
            {TimeFrameIndex(40), EntityId(3)},  // Not in range
            {TimeFrameIndex(50), EntityId(1)}   // Not in range
        };
        
        std::unordered_set<EntityId> selected{EntityId(1)};
        auto filtered = filterByTimeRangeAndEntityIds(
            events, 
            TimeFrameIndex(10), 
            TimeFrameIndex(35),
            selected
        );
        auto result = materializeToVector(filtered);
        
        REQUIRE(result.size() == 2);
        REQUIRE(result[0].time() == TimeFrameIndex(10));
        REQUIRE(result[1].time() == TimeFrameIndex(30));
    }
}

TEST_CASE("TimeSeriesFilters - Chained filtering", "[filters][composition][unit]") {
    
    SECTION("Chain time filter then entity filter") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)},
            {TimeFrameIndex(30), EntityId(1)},
            {TimeFrameIndex(40), EntityId(2)},
            {TimeFrameIndex(50), EntityId(1)}
        };
        
        std::unordered_set<EntityId> selected{EntityId(1)};
        
        // First filter by time, then by entity
        auto timeFiltered = filterByTimeRange(events, TimeFrameIndex(15), TimeFrameIndex(45));
        auto result = materializeToVector(filterByEntityIds(timeFiltered, selected));
        
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].time() == TimeFrameIndex(30));
        REQUIRE(result[0].id() == EntityId(1));
    }
}

// =============================================================================
// Count Utilities Tests
// =============================================================================

TEST_CASE("TimeSeriesFilters - countInTimeRange", "[filters][count][unit]") {
    
    SECTION("Count elements in time range") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)},
            {TimeFrameIndex(30), EntityId(3)},
            {TimeFrameIndex(40), EntityId(4)}
        };
        
        REQUIRE(countInTimeRange(events, TimeFrameIndex(15), TimeFrameIndex(35)) == 2);
        REQUIRE(countInTimeRange(events, TimeFrameIndex(0), TimeFrameIndex(100)) == 4);
        REQUIRE(countInTimeRange(events, TimeFrameIndex(100), TimeFrameIndex(200)) == 0);
    }
}

TEST_CASE("TimeSeriesFilters - countWithEntityIds", "[filters][count][unit]") {
    
    SECTION("Count elements with matching EntityIds") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)},
            {TimeFrameIndex(30), EntityId(1)},
            {TimeFrameIndex(40), EntityId(3)},
            {TimeFrameIndex(50), EntityId(1)}
        };
        
        std::unordered_set<EntityId> selected{EntityId(1)};
        REQUIRE(countWithEntityIds(events, selected) == 3);
        
        selected.insert(EntityId(2));
        REQUIRE(countWithEntityIds(events, selected) == 4);
    }
}

// =============================================================================
// Predicate Utilities Tests
// =============================================================================

TEST_CASE("TimeSeriesFilters - anyInTimeRange", "[filters][predicate][unit]") {
    
    SECTION("Returns true when any element in range") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)},
            {TimeFrameIndex(30), EntityId(3)}
        };
        
        REQUIRE(anyInTimeRange(events, TimeFrameIndex(15), TimeFrameIndex(25)));
        REQUIRE(anyInTimeRange(events, TimeFrameIndex(0), TimeFrameIndex(100)));
    }
    
    SECTION("Returns false when no element in range") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)}
        };
        
        REQUIRE_FALSE(anyInTimeRange(events, TimeFrameIndex(100), TimeFrameIndex(200)));
    }
    
    SECTION("Returns false for empty range") {
        std::vector<EventWithId> events;
        REQUIRE_FALSE(anyInTimeRange(events, TimeFrameIndex(0), TimeFrameIndex(100)));
    }
}

TEST_CASE("TimeSeriesFilters - allInTimeRange", "[filters][predicate][unit]") {
    
    SECTION("Returns true when all elements in range") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)},
            {TimeFrameIndex(30), EntityId(3)}
        };
        
        REQUIRE(allInTimeRange(events, TimeFrameIndex(0), TimeFrameIndex(100)));
        REQUIRE(allInTimeRange(events, TimeFrameIndex(10), TimeFrameIndex(30)));
    }
    
    SECTION("Returns false when not all elements in range") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)},
            {TimeFrameIndex(30), EntityId(3)}
        };
        
        REQUIRE_FALSE(allInTimeRange(events, TimeFrameIndex(15), TimeFrameIndex(25)));
    }
    
    SECTION("Returns true for empty range (vacuous truth)") {
        std::vector<EventWithId> events;
        REQUIRE(allInTimeRange(events, TimeFrameIndex(0), TimeFrameIndex(100)));
    }
}

TEST_CASE("TimeSeriesFilters - anyWithEntityIds", "[filters][predicate][unit]") {
    
    SECTION("Returns true when any element has matching EntityId") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)},
            {TimeFrameIndex(30), EntityId(3)}
        };
        
        std::unordered_set<EntityId> selected{EntityId(2)};
        REQUIRE(anyWithEntityIds(events, selected));
    }
    
    SECTION("Returns false when no element has matching EntityId") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)}
        };
        
        std::unordered_set<EntityId> selected{EntityId(100)};
        REQUIRE_FALSE(anyWithEntityIds(events, selected));
    }
}

// =============================================================================
// Extraction Utilities Tests
// =============================================================================

TEST_CASE("TimeSeriesFilters - extractTimes", "[filters][extract][unit]") {
    
    SECTION("Extract times from TimeValuePoint") {
        std::vector<AnalogTimeSeries::TimeValuePoint> points{
            {TimeFrameIndex(10), 1.0f},
            {TimeFrameIndex(20), 2.0f},
            {TimeFrameIndex(30), 3.0f}
        };
        
        auto times = extractTimes(points);
        auto result = materializeToVector(times);
        
        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == TimeFrameIndex(10));
        REQUIRE(result[1] == TimeFrameIndex(20));
        REQUIRE(result[2] == TimeFrameIndex(30));
    }
    
    SECTION("Extract times from EventWithId") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(100), EntityId(1)},
            {TimeFrameIndex(200), EntityId(2)}
        };
        
        auto times = extractTimes(events);
        auto result = materializeToVector(times);
        
        REQUIRE(result.size() == 2);
        REQUIRE(result[0] == TimeFrameIndex(100));
        REQUIRE(result[1] == TimeFrameIndex(200));
    }
}

TEST_CASE("TimeSeriesFilters - extractEntityIds", "[filters][extract][unit]") {
    
    SECTION("Extract EntityIds from EventWithId") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(100)},
            {TimeFrameIndex(20), EntityId(200)},
            {TimeFrameIndex(30), EntityId(300)}
        };
        
        auto ids = extractEntityIds(events);
        auto result = materializeToVector(ids);
        
        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == EntityId(100));
        REQUIRE(result[1] == EntityId(200));
        REQUIRE(result[2] == EntityId(300));
    }
}

TEST_CASE("TimeSeriesFilters - uniqueEntityIds", "[filters][extract][unit]") {
    
    SECTION("Get unique EntityIds from elements with duplicates") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(20), EntityId(2)},
            {TimeFrameIndex(30), EntityId(1)},
            {TimeFrameIndex(40), EntityId(3)},
            {TimeFrameIndex(50), EntityId(1)}
        };
        
        auto unique = uniqueEntityIds(events);
        
        REQUIRE(unique.size() == 3);
        REQUIRE(unique.contains(EntityId(1)));
        REQUIRE(unique.contains(EntityId(2)));
        REQUIRE(unique.contains(EntityId(3)));
    }
}

// =============================================================================
// Boundary Utilities Tests
// =============================================================================

TEST_CASE("TimeSeriesFilters - minTime", "[filters][bounds][unit]") {
    
    SECTION("Find minimum time in range") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(30), EntityId(1)},
            {TimeFrameIndex(10), EntityId(2)},
            {TimeFrameIndex(20), EntityId(3)}
        };
        
        auto min = minTime(events);
        REQUIRE(min.has_value());
        REQUIRE(*min == TimeFrameIndex(10));
    }
    
    SECTION("Returns nullopt for empty range") {
        std::vector<EventWithId> events;
        auto min = minTime(events);
        REQUIRE_FALSE(min.has_value());
    }
}

TEST_CASE("TimeSeriesFilters - maxTime", "[filters][bounds][unit]") {
    
    SECTION("Find maximum time in range") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(10), EntityId(1)},
            {TimeFrameIndex(30), EntityId(2)},
            {TimeFrameIndex(20), EntityId(3)}
        };
        
        auto max = maxTime(events);
        REQUIRE(max.has_value());
        REQUIRE(*max == TimeFrameIndex(30));
    }
    
    SECTION("Returns nullopt for empty range") {
        std::vector<EventWithId> events;
        auto max = maxTime(events);
        REQUIRE_FALSE(max.has_value());
    }
}

TEST_CASE("TimeSeriesFilters - timeBounds", "[filters][bounds][unit]") {
    
    SECTION("Find both min and max time") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(20), EntityId(1)},
            {TimeFrameIndex(10), EntityId(2)},
            {TimeFrameIndex(50), EntityId(3)},
            {TimeFrameIndex(30), EntityId(4)}
        };
        
        auto bounds = timeBounds(events);
        REQUIRE(bounds.has_value());
        REQUIRE(bounds->first == TimeFrameIndex(10));
        REQUIRE(bounds->second == TimeFrameIndex(50));
    }
    
    SECTION("Works with single element") {
        std::vector<EventWithId> events{
            {TimeFrameIndex(42), EntityId(1)}
        };
        
        auto bounds = timeBounds(events);
        REQUIRE(bounds.has_value());
        REQUIRE(bounds->first == TimeFrameIndex(42));
        REQUIRE(bounds->second == TimeFrameIndex(42));
    }
    
    SECTION("Returns nullopt for empty range") {
        std::vector<EventWithId> events;
        auto bounds = timeBounds(events);
        REQUIRE_FALSE(bounds.has_value());
    }
}

// =============================================================================
// Integration with Actual Series Types
// =============================================================================

TEST_CASE("TimeSeriesFilters - Integration with DigitalEventSeries", "[filters][integration][unit]") {
    
    SECTION("Filter DigitalEventSeries view by time") {
        auto series = std::make_shared<DigitalEventSeries>();
        series->addEvent(TimeFrameIndex(10));
        series->addEvent(TimeFrameIndex(20));
        series->addEvent(TimeFrameIndex(30));
        series->addEvent(TimeFrameIndex(40));
        series->addEvent(TimeFrameIndex(50));
        
        auto filtered = filterByTimeRange(series->view(), TimeFrameIndex(20), TimeFrameIndex(40));
        auto result = materializeToVector(filtered);
        
        REQUIRE(result.size() == 3);
        REQUIRE(result[0].time() == TimeFrameIndex(20));
        REQUIRE(result[1].time() == TimeFrameIndex(30));
        REQUIRE(result[2].time() == TimeFrameIndex(40));
    }
    
    SECTION("Filter DigitalEventSeries by EntityId") {
        // Use DataManager pattern - it owns the EntityRegistry and sets it up
        auto data_manager = std::make_unique<DataManager>();
        auto time_frame = std::make_shared<TimeFrame>(std::vector<int>{0, 10, 20, 30, 40, 50});
        data_manager->setTime(TimeKey("test_time"), time_frame);
        data_manager->setData<DigitalEventSeries>("events", TimeKey("test_time"));
        
        auto series = data_manager->getData<DigitalEventSeries>("events");
        
        // Add events - each will get a unique EntityId from the DataManager's registry
        series->addEvent(TimeFrameIndex(10));
        series->addEvent(TimeFrameIndex(20));
        series->addEvent(TimeFrameIndex(30));
        
        // Get the actual EntityIds assigned
        auto all_ids = uniqueEntityIds(series->view());
        REQUIRE(all_ids.size() == 3);
        
        // Filter by first EntityId
        auto first_id = series->view() 
            | std::views::take(1) 
            | std::views::transform([](auto const& e) { return e.id(); });
        auto first = *first_id.begin();
        
        auto filtered = filterByEntityId(series->view(), first);
        auto result = materializeToVector(filtered);
        
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].id() == first);
    }
}

TEST_CASE("TimeSeriesFilters - Integration with DigitalIntervalSeries", "[filters][integration][unit]") {
    
    SECTION("Filter DigitalIntervalSeries view by time") {
        auto series = std::make_shared<DigitalIntervalSeries>();
        series->addEvent(Interval{100, 150});
        series->addEvent(Interval{200, 250});
        series->addEvent(Interval{300, 350});
        
        // Filter by start time (time() returns interval.start)
        auto filtered = filterByTimeRange(series->view(), TimeFrameIndex(150), TimeFrameIndex(250));
        auto result = materializeToVector(filtered);
        
        REQUIRE(result.size() == 1);
        REQUIRE(result[0].time() == TimeFrameIndex(200));
        REQUIRE(result[0].value().start == 200);
        REQUIRE(result[0].value().end == 250);
    }
}

TEST_CASE("TimeSeriesFilters - Integration with AnalogTimeSeries", "[filters][integration][unit]") {
    
    SECTION("Filter AnalogTimeSeries elementsView by time") {
        // Create series with explicit time indices (indices 0-4)
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> times{
            TimeFrameIndex(0), TimeFrameIndex(1), TimeFrameIndex(2), 
            TimeFrameIndex(3), TimeFrameIndex(4)
        };
        auto series = std::make_shared<AnalogTimeSeries>(data, times);
        
        // elementsView returns TimeValuePoint with the time indices we provided
        auto filtered = filterByTimeRange(series->elementsView(), TimeFrameIndex(1), TimeFrameIndex(3));
        auto result = materializeToVector(filtered);
        
        REQUIRE(result.size() == 3);
        REQUIRE(result[0].value() == 2.0f);  // Index 1
        REQUIRE(result[1].value() == 3.0f);  // Index 2
        REQUIRE(result[2].value() == 4.0f);  // Index 3
    }
}

// =============================================================================
// Compile-Time Constraint Verification
// =============================================================================

// These static_asserts verify that the concepts properly constrain the filter functions.
// The code should NOT compile if we try to use filterByEntityIds with non-entity types.

// Verify that TimeValuePoint does NOT satisfy EntityElement
static_assert(!EntityElement<AnalogTimeSeries::TimeValuePoint>,
    "TimeValuePoint must NOT satisfy EntityElement - no EntityId filtering allowed");

// Verify that FlatElement does NOT satisfy EntityElement  
static_assert(!EntityElement<RaggedAnalogTimeSeries::FlatElement>,
    "FlatElement must NOT satisfy EntityElement - no EntityId filtering allowed");

// Verify that EventWithId DOES satisfy EntityElement
static_assert(EntityElement<EventWithId>,
    "EventWithId must satisfy EntityElement - EntityId filtering allowed");

// Verify that IntervalWithId DOES satisfy EntityElement
static_assert(EntityElement<IntervalWithId>,
    "IntervalWithId must satisfy EntityElement - EntityId filtering allowed");

// The following would cause a compile error if uncommented:
// auto bad_filter = filterByEntityIds(timeValuePoints, entityIds);
// This is the desired behavior - compile-time enforcement of concept constraints.
