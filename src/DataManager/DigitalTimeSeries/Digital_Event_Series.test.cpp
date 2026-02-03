
#include <catch2/catch_test_macros.hpp>

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <fstream>
#include <vector>

namespace {
// Helper to create a simple TimeFrame for testing
std::shared_ptr<TimeFrame> makeTestTimeFrame(int64_t num_frames) {
    std::vector<int> times;
    times.reserve(static_cast<size_t>(num_frames));
    for (int64_t i = 0; i < num_frames; ++i) {
        times.push_back(static_cast<int>(i));
    }
    return std::make_shared<TimeFrame>(times);
}
} // namespace

TEST_CASE("Digital Event Series - Constructor", "[DataManager]") {
    DigitalEventSeries des;
    REQUIRE(des.size() == 0);

    std::vector<TimeFrameIndex> events = {TimeFrameIndex(3), TimeFrameIndex(1), TimeFrameIndex(2)};
    DigitalEventSeries des2(events);

    // Verify that the constructor sorts the events
    REQUIRE(des2.size() == 3);
    std::vector<TimeFrameIndex> collected;
    for (auto event : des2.view()) {
        collected.push_back(event.time());
    }
    REQUIRE(collected[0] == TimeFrameIndex(1));
    REQUIRE(collected[1] == TimeFrameIndex(2));
    REQUIRE(collected[2] == TimeFrameIndex(3));
}

TEST_CASE("Digital Event Series - Add Event", "[DataManager]") {
    DigitalEventSeries des;

    // Add events in arbitrary order
    des.addEvent(TimeFrameIndex(3));
    des.addEvent(TimeFrameIndex(1));
    des.addEvent(TimeFrameIndex(5));
    des.addEvent(TimeFrameIndex(2));

    // Verify that events are sorted after each addition
    REQUIRE(des.size() == 4);
    std::vector<TimeFrameIndex> collected;
    for (auto event : des.view()) {
        collected.push_back(event.time());
    }
    REQUIRE(collected[0] == TimeFrameIndex(1));
    REQUIRE(collected[1] == TimeFrameIndex(2));
    REQUIRE(collected[2] == TimeFrameIndex(3));
    REQUIRE(collected[3] == TimeFrameIndex(5));
}

TEST_CASE("Digital Event Series - Remove Event", "[DataManager]") {
    std::vector<TimeFrameIndex> events = {TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3), TimeFrameIndex(4), TimeFrameIndex(5)};
    DigitalEventSeries des(events);

    // Remove an event that exists
    bool removed = des.removeEvent(TimeFrameIndex(3));
    REQUIRE(removed);

    // Check that the event was removed
    REQUIRE(des.size() == 4);
    std::vector<TimeFrameIndex> collected;
    for (auto event : des.view()) {
        collected.push_back(event.time());
    }
    REQUIRE(collected[0] == TimeFrameIndex(1));
    REQUIRE(collected[1] == TimeFrameIndex(2));
    REQUIRE(collected[2] == TimeFrameIndex(4));
    REQUIRE(collected[3] == TimeFrameIndex(5));

    // Try to remove an event that doesn't exist
    removed = des.removeEvent(TimeFrameIndex(10));
    REQUIRE_FALSE(removed);
    REQUIRE(des.size() == 4);
}

TEST_CASE("Digital Event Series - Clear", "[DataManager]") {
    std::vector<TimeFrameIndex> events = {TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3)};
    DigitalEventSeries des(events);

    REQUIRE(des.size() == 3);

    des.clear();
    REQUIRE(des.size() == 0);
}

TEST_CASE("Digital Event Series - Duplicate Events", "[DataManager]") {
    DigitalEventSeries des;

    // Add duplicate events
    des.addEvent(TimeFrameIndex(2));
    des.addEvent(TimeFrameIndex(1));
    des.addEvent(TimeFrameIndex(2));
    des.addEvent(TimeFrameIndex(3));
    des.addEvent(TimeFrameIndex(1));

    // Verify that duplicates are not added and sorted
    REQUIRE(des.size() == 3);
    std::vector<TimeFrameIndex> collected;
    for (auto event : des.view()) {
        collected.push_back(event.time());
    }
    REQUIRE(collected[0] == TimeFrameIndex(1));
    REQUIRE(collected[1] == TimeFrameIndex(2));
    REQUIRE(collected[2] == TimeFrameIndex(3));
}

TEST_CASE("Digital Event Series - Empty Series", "[DataManager]") {
    DigitalEventSeries des;

    // Test with empty series
    REQUIRE(des.size() == 0);
    REQUIRE(std::ranges::empty(des.view()));

    // Removing from empty series should return false
    bool removed = des.removeEvent(TimeFrameIndex(1));
    REQUIRE_FALSE(removed);

    // Add and then remove to get back to empty
    des.addEvent(TimeFrameIndex(5));
    REQUIRE(des.size() == 1);

    des.removeEvent(TimeFrameIndex(5));
    REQUIRE(des.size() == 0);
}

TEST_CASE("DigitalEventSeries - Range-based access (C++20)", "[DataManager]") {
    std::vector<TimeFrameIndex> events = {TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3), TimeFrameIndex(5), TimeFrameIndex(7), TimeFrameIndex(10)};
    DigitalEventSeries des(events);
    auto tf = makeTestTimeFrame(100);
    des.setTimeFrame(tf);

    SECTION("viewTimesInRange returns correct view") {
        // Get events between 2 and 8 inclusive
        auto range = des.viewTimesInRange(TimeFrameIndex(2), TimeFrameIndex(8), *tf);

        // Count elements in the range
        size_t count = 0;
        std::vector<TimeFrameIndex> collected;
        for (TimeFrameIndex event : range) {
            collected.push_back(event);
            count++;
        }

        REQUIRE(count == 4);
        REQUIRE(collected[0] == TimeFrameIndex(2));
        REQUIRE(collected[1] == TimeFrameIndex(3));
        REQUIRE(collected[2] == TimeFrameIndex(5));
        REQUIRE(collected[3] == TimeFrameIndex(7));
    }

    SECTION("viewInRange returns EventWithId objects") {
        // Get events between 3 and 9 inclusive
        auto range = des.viewInRange(TimeFrameIndex(3), TimeFrameIndex(9), *tf);

        std::vector<TimeFrameIndex> collected;
        for (auto event : range) {
            collected.push_back(event.time());
        }

        REQUIRE(collected.size() == 3);
        REQUIRE(collected[0] == TimeFrameIndex(3));
        REQUIRE(collected[1] == TimeFrameIndex(5));
        REQUIRE(collected[2] == TimeFrameIndex(7));
    }
}

TEST_CASE("DigitalEventSeries - Range edge cases", "[DataManager]") {
    std::vector<TimeFrameIndex> events = {TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3), TimeFrameIndex(4), TimeFrameIndex(5)};
    DigitalEventSeries des(events);
    auto tf = makeTestTimeFrame(100);
    des.setTimeFrame(tf);

    SECTION("Exact boundary matches") {
        auto range = des.viewTimesInRange(TimeFrameIndex(2), TimeFrameIndex(4), *tf);
        std::vector<TimeFrameIndex> collected;
        for (auto t : range) { collected.push_back(t); }
        REQUIRE(collected.size() == 3);
        REQUIRE(collected[0] == TimeFrameIndex(2));
        REQUIRE(collected[1] == TimeFrameIndex(3));
        REQUIRE(collected[2] == TimeFrameIndex(4));
    }

    SECTION("Range includes all events") {
        auto range = des.viewTimesInRange(TimeFrameIndex(0), TimeFrameIndex(10), *tf);
        REQUIRE(std::ranges::distance(range) == 5);
    }

    SECTION("Range outside all events (before)") {
        auto range = des.viewTimesInRange(TimeFrameIndex(-5), TimeFrameIndex(0), *tf);
        REQUIRE(std::ranges::empty(range));
    }

    SECTION("Range outside all events (after)") {
        auto range = des.viewTimesInRange(TimeFrameIndex(6), TimeFrameIndex(10), *tf);
        REQUIRE(std::ranges::empty(range));
    }

    SECTION("Single point range") {
        auto range = des.viewTimesInRange(TimeFrameIndex(3), TimeFrameIndex(3), *tf);
        std::vector<TimeFrameIndex> collected;
        for (auto t : range) { collected.push_back(t); }
        REQUIRE(collected.size() == 1);
        REQUIRE(collected[0] == TimeFrameIndex(3));
    }
}

TEST_CASE("DigitalEventSeries - Range with empty series", "[DataManager]") {
    DigitalEventSeries des;
    auto tf = makeTestTimeFrame(100);
    des.setTimeFrame(tf);

    auto range = des.viewTimesInRange(TimeFrameIndex(1), TimeFrameIndex(10), *tf);
    REQUIRE(std::ranges::empty(range));

    auto range_with_ids = des.viewInRange(TimeFrameIndex(1), TimeFrameIndex(10), *tf);
    REQUIRE(std::ranges::empty(range_with_ids));
}

TEST_CASE("DigitalEventSeries - Range with duplicate events", "[DataManager]") {
    // Note: DigitalEventSeries allows duplicates when constructed from vector
    // (only addEvent() rejects duplicates)
    std::vector<TimeFrameIndex> events = {TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(2), TimeFrameIndex(3), TimeFrameIndex(3), TimeFrameIndex(3)};
    DigitalEventSeries des(events);
    auto tf = makeTestTimeFrame(100);
    des.setTimeFrame(tf);

    // All duplicates are preserved from the constructor
    auto range = des.viewTimesInRange(TimeFrameIndex(2), TimeFrameIndex(3), *tf);
    std::vector<TimeFrameIndex> collected;
    for (auto t : range) { collected.push_back(t); }
    REQUIRE(collected.size() == 5);
    REQUIRE(collected[0] == TimeFrameIndex(2));
    REQUIRE(collected[1] == TimeFrameIndex(2));
    REQUIRE(collected[2] == TimeFrameIndex(3));
    REQUIRE(collected[3] == TimeFrameIndex(3));
    REQUIRE(collected[4] == TimeFrameIndex(3));
}

TEST_CASE("DigitalEventSeries - Range interaction with add/remove", "[DataManager]") {
    DigitalEventSeries des;
    auto tf = makeTestTimeFrame(100);
    des.setTimeFrame(tf);

    // Add events
    des.addEvent(TimeFrameIndex(1));
    des.addEvent(TimeFrameIndex(3));
    des.addEvent(TimeFrameIndex(5));

    // Helper to collect range
    auto collectRange = [&](TimeFrameIndex start, TimeFrameIndex stop) {
        std::vector<TimeFrameIndex> result;
        for (auto t : des.viewTimesInRange(start, stop, *tf)) {
            result.push_back(t);
        }
        return result;
    };

    // Initial check
    auto collected = collectRange(TimeFrameIndex(2), TimeFrameIndex(6));
    REQUIRE(collected.size() == 2);
    REQUIRE(collected[0] == TimeFrameIndex(3));
    REQUIRE(collected[1] == TimeFrameIndex(5));

    // Add another event in range
    des.addEvent(TimeFrameIndex(4));
    collected = collectRange(TimeFrameIndex(2), TimeFrameIndex(6));
    REQUIRE(collected.size() == 3);
    REQUIRE(collected[0] == TimeFrameIndex(3));
    REQUIRE(collected[1] == TimeFrameIndex(4));
    REQUIRE(collected[2] == TimeFrameIndex(5));

    // Remove an event
    des.removeEvent(TimeFrameIndex(3));
    collected = collectRange(TimeFrameIndex(2), TimeFrameIndex(6));
    REQUIRE(collected.size() == 2);
    REQUIRE(collected[0] == TimeFrameIndex(4));
    REQUIRE(collected[1] == TimeFrameIndex(5));
}

// =============================================================================
// Test view() method - primary iteration interface
// =============================================================================

TEST_CASE("DigitalEventSeries - view() method", "[DataManager][view]") {
    std::vector<TimeFrameIndex> events = {TimeFrameIndex(1), TimeFrameIndex(3), TimeFrameIndex(5)};
    DigitalEventSeries des(events);

    SECTION("Basic iteration with EventWithId accessors") {
        std::vector<TimeFrameIndex> collected_times;
        std::vector<TimeFrameIndex> collected_values;
        std::vector<EntityId> collected_ids;

        for (auto event : des.view()) {
            collected_times.push_back(event.time());
            collected_values.push_back(event.value());
            collected_ids.push_back(event.id());
        }

        REQUIRE(collected_times.size() == 3);
        REQUIRE(collected_times[0] == TimeFrameIndex(1));
        REQUIRE(collected_times[1] == TimeFrameIndex(3));
        REQUIRE(collected_times[2] == TimeFrameIndex(5));

        // For events, value() returns the same as time()
        REQUIRE(collected_times[0] == collected_values[0]);
        REQUIRE(collected_times[1] == collected_values[1]);
        REQUIRE(collected_times[2] == collected_values[2]);
    }

    SECTION("Empty series") {
        DigitalEventSeries empty_des;

        size_t count = 0;
        for ([[maybe_unused]] auto event : empty_des.view()) {
            count++;
        }
        REQUIRE(count == 0);
    }

    SECTION("Direct member access via event_time and entity_id") {
        auto v = des.view();
        auto it = std::ranges::begin(v);
        auto first_event = *it;

        // Test direct member access
        REQUIRE(first_event.event_time == TimeFrameIndex(1));
        REQUIRE(first_event.time() == TimeFrameIndex(1));
    }

    SECTION("Works with range algorithms") {
        // Count elements in range
        auto count = std::ranges::distance(des.view());
        REQUIRE(count == 3);
    }
}

TEST_CASE("DigitalEventSeries - view() concept-compliant iteration", "[DataManager][view]") {
    std::vector<TimeFrameIndex> events = {TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};
    DigitalEventSeries des(events);

    SECTION("EventWithId satisfies concept requirements") {
        std::vector<TimeFrameIndex> collected_times;
        std::vector<EntityId> collected_ids;
        std::vector<TimeFrameIndex> collected_values;

        for (auto event : des.view()) {
            collected_times.push_back(event.time());
            collected_ids.push_back(event.id());
            collected_values.push_back(event.value());
        }

        REQUIRE(collected_times.size() == 3);
        REQUIRE(collected_times[0] == TimeFrameIndex(10));
        REQUIRE(collected_times[1] == TimeFrameIndex(20));
        REQUIRE(collected_times[2] == TimeFrameIndex(30));

        // For events, value() returns the same as time()
        REQUIRE(collected_times[0] == collected_values[0]);
        REQUIRE(collected_times[1] == collected_values[1]);
        REQUIRE(collected_times[2] == collected_values[2]);
    }
}
