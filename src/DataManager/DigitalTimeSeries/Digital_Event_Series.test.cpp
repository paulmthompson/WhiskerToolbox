
#include <catch2/catch_test_macros.hpp>

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "loaders/CSV_Loaders.hpp"

#include <fstream>
#include <vector>

TEST_CASE("Digital Event Series - Constructor", "[DataManager]") {
    DigitalEventSeries des;
    REQUIRE(des.size() == 0);

    std::vector<TimeFrameIndex> events = {TimeFrameIndex(3), TimeFrameIndex(1), TimeFrameIndex(2)};
    DigitalEventSeries des2(events);

    // Verify that the constructor sorts the events
    auto data = des2.getEventSeries();
    REQUIRE(data.size() == 3);
    REQUIRE(data[0] == TimeFrameIndex(1));
    REQUIRE(data[1] == TimeFrameIndex(2));
    REQUIRE(data[2] == TimeFrameIndex(3));
}

TEST_CASE("Digital Event Series - Add Event", "[DataManager]") {
    DigitalEventSeries des;

    // Add events in arbitrary order
    des.addEvent(TimeFrameIndex(3));
    des.addEvent(TimeFrameIndex(1));
    des.addEvent(TimeFrameIndex(5));
    des.addEvent(TimeFrameIndex(2));

    // Verify that events are sorted after each addition
    auto data = des.getEventSeries();
    REQUIRE(data.size() == 4);
    REQUIRE(data[0] == TimeFrameIndex(1));
    REQUIRE(data[1] == TimeFrameIndex(2));
    REQUIRE(data[2] == TimeFrameIndex(3));
    REQUIRE(data[3] == TimeFrameIndex(5));
}

TEST_CASE("Digital Event Series - Remove Event", "[DataManager]") {
    std::vector<TimeFrameIndex> events = {TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3), TimeFrameIndex(4), TimeFrameIndex(5)};
    DigitalEventSeries des(events);

    // Remove an event that exists
    bool removed = des.removeEvent(TimeFrameIndex(3));
    REQUIRE(removed);

    // Check that the event was removed
    auto data = des.getEventSeries();
    REQUIRE(data.size() == 4);
    REQUIRE(data[0] == TimeFrameIndex(1));
    REQUIRE(data[1] == TimeFrameIndex(2));
    REQUIRE(data[2] == TimeFrameIndex(4));
    REQUIRE(data[3] == TimeFrameIndex(5));

    // Try to remove an event that doesn't exist
    removed = des.removeEvent(TimeFrameIndex(10));
    REQUIRE_FALSE(removed);
    REQUIRE(data.size() == 4);
}

TEST_CASE("Digital Event Series - Clear", "[DataManager]") {
    std::vector<TimeFrameIndex> events = {TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3)};
    DigitalEventSeries des(events);

    REQUIRE(des.size() == 3);

    des.clear();
    REQUIRE(des.size() == 0);
}

TEST_CASE("Digital Event Series - Load From CSV", "[DataManager]") {
    // This test assumes a CSV file exists at the specified path
    // You may need to adjust the path or create a test file

    std::string filename = "data/DigitalEvents/events.csv";

    // Assuming the loadFromCSV function works as expected
    // and the CSV file contains values 3.0, 1.0, 5.0, 2.0, 4.0
    // This test will be skipped if the file is not found

    SECTION("Loading from CSV (skip if file not found)") {
        std::ifstream file(filename);
        if (file.good()) {
            file.close();

            auto opts = Loader::CSVSingleColumnOptions{.filename = filename};

            auto events_float = Loader::loadSingleColumnCSV(opts);
            
            // Convert float to TimeFrameIndex
            std::vector<TimeFrameIndex> events;
            events.reserve(events_float.size());
            for (auto e : events_float) {
                events.push_back(TimeFrameIndex(static_cast<int64_t>(e)));
            }

            auto des = DigitalEventSeries(events);
            auto data = des.getEventSeries();

            // Check that the data is loaded and sorted
            REQUIRE(data.size() > 0);

            // Verify data is sorted
            for (size_t i = 1; i < data.size(); ++i) {
                REQUIRE(data[i - 1] <= data[i]);
            }
        }
    }
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
    auto data = des.getEventSeries();
    REQUIRE(data.size() == 3);
    REQUIRE(data[0] == TimeFrameIndex(1));
    REQUIRE(data[1] == TimeFrameIndex(2));
    REQUIRE(data[2] == TimeFrameIndex(3));
}

TEST_CASE("Digital Event Series - Empty Series", "[DataManager]") {
    DigitalEventSeries des;

    // Test with empty series
    auto data = des.getEventSeries();
    REQUIRE(data.empty());

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

    SECTION("getEventsInRange returns correct view") {
        // Get events between 2.0 and 7.5 inclusive
        auto range = des.getEventsInRange(TimeFrameIndex(2), TimeFrameIndex(8));

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

    SECTION("getEventsAsVector returns correct vector") {
        // Get events between 3.0 and 9.0 inclusive
        auto vector_range = des.getEventsAsVector(TimeFrameIndex(3), TimeFrameIndex(9));

        REQUIRE(vector_range.size() == 3);
        REQUIRE(vector_range[0] == TimeFrameIndex(3));
        REQUIRE(vector_range[1] == TimeFrameIndex(5));
        REQUIRE(vector_range[2] == TimeFrameIndex(7));
    }
}

TEST_CASE("DigitalEventSeries - Range edge cases", "[DataManager]") {
    std::vector<TimeFrameIndex> events = {TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(3), TimeFrameIndex(4), TimeFrameIndex(5)};
    DigitalEventSeries des(events);

    SECTION("Exact boundary matches") {
        auto vector_range = des.getEventsAsVector(TimeFrameIndex(2), TimeFrameIndex(4));
        REQUIRE(vector_range.size() == 3);
        REQUIRE(vector_range[0] == TimeFrameIndex(2));
        REQUIRE(vector_range[1] == TimeFrameIndex(3));
        REQUIRE(vector_range[2] == TimeFrameIndex(4));
    }

    SECTION("Range includes all events") {
        auto vector_range = des.getEventsAsVector(TimeFrameIndex(0), TimeFrameIndex(10));
        REQUIRE(vector_range.size() == 5);
    }

    SECTION("Range outside all events (before)") {
        auto range = des.getEventsInRange(TimeFrameIndex(-5), TimeFrameIndex(0));
        bool is_empty = std::ranges::empty(range);
        REQUIRE(is_empty);

        auto vector_range = des.getEventsAsVector(TimeFrameIndex(-5), TimeFrameIndex(0));
        REQUIRE(vector_range.empty());
    }

    SECTION("Range outside all events (after)") {
        auto vector_range = des.getEventsAsVector(TimeFrameIndex(6), TimeFrameIndex(10));
        REQUIRE(vector_range.empty());
    }

    SECTION("Single point range") {
        auto vector_range = des.getEventsAsVector(TimeFrameIndex(3), TimeFrameIndex(3));
        REQUIRE(vector_range.size() == 1);
        REQUIRE(vector_range[0] == TimeFrameIndex(3));
    }

    SECTION("Empty range (start > stop)") {
        auto vector_range = des.getEventsAsVector(TimeFrameIndex(4), TimeFrameIndex(2));
        REQUIRE(vector_range.empty());
    }
}

TEST_CASE("DigitalEventSeries - Range with empty series", "[DataManager]") {
    DigitalEventSeries des;

    auto range = des.getEventsInRange(TimeFrameIndex(1), TimeFrameIndex(10));
    bool is_empty = std::ranges::empty(range);
    REQUIRE(is_empty);

    auto vector_range = des.getEventsAsVector(TimeFrameIndex(1), TimeFrameIndex(10));
    REQUIRE(vector_range.empty());
}

TEST_CASE("DigitalEventSeries - Range with duplicate events", "[DataManager]") {
    std::vector<TimeFrameIndex> events = {TimeFrameIndex(1), TimeFrameIndex(2), TimeFrameIndex(2), TimeFrameIndex(3), TimeFrameIndex(3), TimeFrameIndex(3)};
    DigitalEventSeries des(events);

    auto vector_range = des.getEventsAsVector(TimeFrameIndex(2), TimeFrameIndex(3));
    REQUIRE(vector_range.size() == 5);
    REQUIRE(vector_range[0] == TimeFrameIndex(2));
    REQUIRE(vector_range[1] == TimeFrameIndex(2));
    REQUIRE(vector_range[2] == TimeFrameIndex(3));
    REQUIRE(vector_range[3] == TimeFrameIndex(3));
    REQUIRE(vector_range[4] == TimeFrameIndex(3));
}

TEST_CASE("DigitalEventSeries - Range interaction with add/remove", "[DataManager]") {
    DigitalEventSeries des;

    // Add events
    des.addEvent(TimeFrameIndex(1));
    des.addEvent(TimeFrameIndex(3));
    des.addEvent(TimeFrameIndex(5));

    // Initial check
    auto vector_range = des.getEventsAsVector(TimeFrameIndex(2), TimeFrameIndex(6));
    REQUIRE(vector_range.size() == 2);
    REQUIRE(vector_range[0] == TimeFrameIndex(3));
    REQUIRE(vector_range[1] == TimeFrameIndex(5));

    // Add another event in range
    des.addEvent(TimeFrameIndex(4));
    vector_range = des.getEventsAsVector(TimeFrameIndex(2), TimeFrameIndex(6));
    REQUIRE(vector_range.size() == 3);
    REQUIRE(vector_range[0] == TimeFrameIndex(3));
    REQUIRE(vector_range[1] == TimeFrameIndex(4));
    REQUIRE(vector_range[2] == TimeFrameIndex(5));

    // Remove an event
    des.removeEvent(TimeFrameIndex(3));
    vector_range = des.getEventsAsVector(TimeFrameIndex(2), TimeFrameIndex(6));
    REQUIRE(vector_range.size() == 2);
    REQUIRE(vector_range[0] == TimeFrameIndex(4));
    REQUIRE(vector_range[1] == TimeFrameIndex(5));
}

// =============================================================================
// Test elements() and elementsView() methods (Phase 4.4 Step 4)
// =============================================================================

TEST_CASE("DigitalEventSeries - elements() method", "[DataManager][elements]") {
    std::vector<TimeFrameIndex> events = {TimeFrameIndex(1), TimeFrameIndex(3), TimeFrameIndex(5)};
    DigitalEventSeries des(events);

    SECTION("Basic iteration with structured bindings") {
        std::vector<TimeFrameIndex> collected_times;
        std::vector<TimeFrameIndex> collected_event_times;
        std::vector<EntityId> collected_ids;

        for (auto const [time, event] : des.elements()) {
            collected_times.push_back(time);
            collected_event_times.push_back(event.event_time);
            collected_ids.push_back(event.entity_id);
        }

        REQUIRE(collected_times.size() == 3);
        REQUIRE(collected_times[0] == TimeFrameIndex(1));
        REQUIRE(collected_times[1] == TimeFrameIndex(3));
        REQUIRE(collected_times[2] == TimeFrameIndex(5));

        // Time in pair should match event time
        REQUIRE(collected_times[0] == collected_event_times[0]);
        REQUIRE(collected_times[1] == collected_event_times[1]);
        REQUIRE(collected_times[2] == collected_event_times[2]);
    }

    SECTION("Empty series") {
        DigitalEventSeries empty_des;

        size_t count = 0;
        for ([[maybe_unused]] auto const [time, event] : empty_des.elements()) {
            count++;
        }
        REQUIRE(count == 0);
    }

    SECTION("Pair accessors (.first, .second)") {
        auto view = des.elements();
        auto it = std::ranges::begin(view);
        auto first_element = *it;

        // Test .first and .second access
        REQUIRE(first_element.first == TimeFrameIndex(1));
        REQUIRE(first_element.second.event_time == TimeFrameIndex(1));
    }

    SECTION("Works with range algorithms") {
        // Count elements in range
        auto count = std::ranges::distance(des.elements());
        REQUIRE(count == 3);
    }
}

TEST_CASE("DigitalEventSeries - elementsView() method", "[DataManager][elements]") {
    std::vector<TimeFrameIndex> events = {TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30)};
    DigitalEventSeries des(events);

    SECTION("Concept-compliant iteration") {
        std::vector<TimeFrameIndex> collected_times;
        std::vector<EntityId> collected_ids;
        std::vector<TimeFrameIndex> collected_values;

        for (auto event : des.elementsView()) {
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

    SECTION("elementsView() returns same data as view()") {
        auto view1 = des.view();
        auto view2 = des.elementsView();

        auto it1 = std::ranges::begin(view1);
        auto it2 = std::ranges::begin(view2);

        for (size_t i = 0; i < des.size(); ++i, ++it1, ++it2) {
            REQUIRE((*it1).time() == (*it2).time());
            REQUIRE((*it1).id() == (*it2).id());
        }
    }
}

TEST_CASE("DigitalEventSeries - elements() consistency with view()", "[DataManager][elements]") {
    std::vector<TimeFrameIndex> events = {TimeFrameIndex(100), TimeFrameIndex(200)};
    DigitalEventSeries des(events);

    // elements() time should match elementsView() time
    auto elements = des.elements();
    auto elements_view = des.elementsView();

    auto it_elem = std::ranges::begin(elements);
    auto it_view = std::ranges::begin(elements_view);

    for (size_t i = 0; i < des.size(); ++i, ++it_elem, ++it_view) {
        auto [time, event] = *it_elem;
        auto view_event = *it_view;

        REQUIRE(time == view_event.time());
        REQUIRE(event.event_time == view_event.event_time);
        REQUIRE(event.entity_id == view_event.entity_id);
    }
}
