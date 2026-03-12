/**
 * @file GetEntityIdsInRange.test.cpp
 * @brief Unit tests for getEntityIdsInRange free functions
 */

#include "Commands/GetEntityIdsInRange.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "Entity/EntityRegistry.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace commands;

// =============================================================================
// RaggedTimeSeries (LineData) tests
// =============================================================================

TEST_CASE("getEntityIdsInRange with LineData returns entities in range",
          "[commands][getEntityIdsInRange]") {
    LineData lines;
    lines.addEntryAtTime(TimeFrameIndex(10),
                         Line2D(std::vector<float>{1.0f, 2.0f},
                                std::vector<float>{1.0f, 2.0f}),
                         EntityId(1), NotifyObservers::No);
    lines.addEntryAtTime(TimeFrameIndex(20),
                         Line2D(std::vector<float>{3.0f, 4.0f},
                                std::vector<float>{3.0f, 4.0f}),
                         EntityId(2), NotifyObservers::No);
    lines.addEntryAtTime(TimeFrameIndex(30),
                         Line2D(std::vector<float>{5.0f, 6.0f},
                                std::vector<float>{5.0f, 6.0f}),
                         EntityId(3), NotifyObservers::No);

    SECTION("all entries in range") {
        auto ids = getEntityIdsInRange(lines, TimeFrameIndex(0), TimeFrameIndex(100));
        REQUIRE(ids.size() == 3);
    }

    SECTION("subset in range") {
        auto ids = getEntityIdsInRange(lines, TimeFrameIndex(15), TimeFrameIndex(25));
        REQUIRE(ids.size() == 1);
        REQUIRE(ids.count(EntityId(2)) == 1);
    }

    SECTION("exact boundary match") {
        auto ids = getEntityIdsInRange(lines, TimeFrameIndex(10), TimeFrameIndex(10));
        REQUIRE(ids.size() == 1);
        REQUIRE(ids.count(EntityId(1)) == 1);
    }

    SECTION("no entries in range") {
        auto ids = getEntityIdsInRange(lines, TimeFrameIndex(40), TimeFrameIndex(50));
        REQUIRE(ids.empty());
    }

    SECTION("empty data") {
        LineData const empty;
        auto ids = getEntityIdsInRange(empty, TimeFrameIndex(0), TimeFrameIndex(100));
        REQUIRE(ids.empty());
    }
}

// =============================================================================
// RaggedTimeSeries (PointData) tests
// =============================================================================

TEST_CASE("getEntityIdsInRange with PointData returns entities in range",
          "[commands][getEntityIdsInRange]") {
    PointData points;
    points.addEntryAtTime(TimeFrameIndex(5), Point2D<float>{1.0f, 2.0f},
                          EntityId(10), NotifyObservers::No);
    points.addEntryAtTime(TimeFrameIndex(15), Point2D<float>{3.0f, 4.0f},
                          EntityId(11), NotifyObservers::No);

    auto ids = getEntityIdsInRange(points, TimeFrameIndex(0), TimeFrameIndex(10));
    REQUIRE(ids.size() == 1);
    REQUIRE(ids.count(EntityId(10)) == 1);
}

// =============================================================================
// RaggedTimeSeries (MaskData) tests
// =============================================================================

TEST_CASE("getEntityIdsInRange with MaskData returns entities in range",
          "[commands][getEntityIdsInRange]") {
    MaskData masks;
    masks.addEntryAtTime(TimeFrameIndex(10),
                         Mask2D(std::vector<uint32_t>{1, 2}, std::vector<uint32_t>{1, 2}),
                         EntityId(20), NotifyObservers::No);
    masks.addEntryAtTime(TimeFrameIndex(20),
                         Mask2D(std::vector<uint32_t>{3, 4}, std::vector<uint32_t>{3, 4}),
                         EntityId(21), NotifyObservers::No);

    auto ids = getEntityIdsInRange(masks, TimeFrameIndex(10), TimeFrameIndex(20));
    REQUIRE(ids.size() == 2);
}

// =============================================================================
// DigitalEventSeries tests
// =============================================================================

TEST_CASE("getEntityIdsInRange with DigitalEventSeries returns events in range",
          "[commands][getEntityIdsInRange]") {
    DigitalEventSeries events;
    events.addEvent(TimeFrameIndex(10), EntityId(100), NotifyObservers::No);
    events.addEvent(TimeFrameIndex(20), EntityId(101), NotifyObservers::No);
    events.addEvent(TimeFrameIndex(30), EntityId(102), NotifyObservers::No);
    events.addEvent(TimeFrameIndex(40), EntityId(103), NotifyObservers::No);

    SECTION("all events in range") {
        auto ids = getEntityIdsInRange(events, TimeFrameIndex(0), TimeFrameIndex(100));
        REQUIRE(ids.size() == 4);
    }

    SECTION("subset in range") {
        auto ids = getEntityIdsInRange(events, TimeFrameIndex(15), TimeFrameIndex(35));
        REQUIRE(ids.size() == 2);
        REQUIRE(ids.count(EntityId(101)) == 1);
        REQUIRE(ids.count(EntityId(102)) == 1);
    }

    SECTION("exact boundary match") {
        auto ids = getEntityIdsInRange(events, TimeFrameIndex(20), TimeFrameIndex(20));
        REQUIRE(ids.size() == 1);
        REQUIRE(ids.count(EntityId(101)) == 1);
    }

    SECTION("no events in range") {
        auto ids = getEntityIdsInRange(events, TimeFrameIndex(50), TimeFrameIndex(60));
        REQUIRE(ids.empty());
    }

    SECTION("empty series") {
        DigitalEventSeries const empty;
        auto ids = getEntityIdsInRange(empty, TimeFrameIndex(0), TimeFrameIndex(100));
        REQUIRE(ids.empty());
    }
}

// =============================================================================
// DigitalIntervalSeries tests
// =============================================================================

TEST_CASE("getEntityIdsInRange with DigitalIntervalSeries returns overlapping intervals",
          "[commands][getEntityIdsInRange]") {
    EntityRegistry registry;
    DigitalIntervalSeries intervals;
    intervals.setIdentityContext("test_intervals", &registry);
    intervals.addEvent(Interval{10, 20});
    intervals.addEvent(Interval{30, 40});
    intervals.addEvent(Interval{50, 60});

    SECTION("all intervals overlap range") {
        auto ids = getEntityIdsInRange(intervals, TimeFrameIndex(0), TimeFrameIndex(100));
        REQUIRE(ids.size() == 3);
    }

    SECTION("partial overlap at end") {
        auto ids = getEntityIdsInRange(intervals, TimeFrameIndex(15), TimeFrameIndex(25));
        REQUIRE(ids.size() == 1);
    }

    SECTION("partial overlap at start") {
        auto ids = getEntityIdsInRange(intervals, TimeFrameIndex(5), TimeFrameIndex(15));
        REQUIRE(ids.size() == 1);
    }

    SECTION("range fully inside one interval") {
        auto ids = getEntityIdsInRange(intervals, TimeFrameIndex(12), TimeFrameIndex(18));
        REQUIRE(ids.size() == 1);
    }

    SECTION("range spanning two intervals") {
        auto ids = getEntityIdsInRange(intervals, TimeFrameIndex(15), TimeFrameIndex(35));
        REQUIRE(ids.size() == 2);
    }

    SECTION("no overlap") {
        auto ids = getEntityIdsInRange(intervals, TimeFrameIndex(21), TimeFrameIndex(29));
        REQUIRE(ids.empty());
    }

    SECTION("exact boundary touch") {
        auto ids = getEntityIdsInRange(intervals, TimeFrameIndex(20), TimeFrameIndex(30));
        REQUIRE(ids.size() == 2);
    }

    SECTION("empty series") {
        DigitalIntervalSeries const empty;
        auto ids = getEntityIdsInRange(empty, TimeFrameIndex(0), TimeFrameIndex(100));
        REQUIRE(ids.empty());
    }
}
