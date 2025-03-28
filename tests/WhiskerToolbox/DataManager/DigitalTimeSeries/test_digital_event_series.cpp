
#include <catch2/catch_test_macros.hpp>

#include "DigitalTimeSeries/Digital_Event_Series.hpp"

#include <fstream>

TEST_CASE("Digital Event Series - Constructor", "[DataManager]") {
    DigitalEventSeries des;
    REQUIRE(des.size() == 0);

    std::vector<float> events = {3.0f, 1.0f, 2.0f};
    DigitalEventSeries des2(events);

    // Verify that the constructor sorts the events
    auto data = des2.getEventSeries();
    REQUIRE(data.size() == 3);
    REQUIRE(data[0] == 1.0f);
    REQUIRE(data[1] == 2.0f);
    REQUIRE(data[2] == 3.0f);
}

TEST_CASE("Digital Event Series - Set Data", "[DataManager]") {
    DigitalEventSeries des;

    // Set data with unsorted events
    std::vector<float> events = {5.0f, 2.0f, 8.0f, 1.0f, 4.0f};
    des.setData(events);

    // Verify that setData sorts the events
    auto data = des.getEventSeries();
    REQUIRE(data.size() == 5);
    REQUIRE(data[0] == 1.0f);
    REQUIRE(data[1] == 2.0f);
    REQUIRE(data[2] == 4.0f);
    REQUIRE(data[3] == 5.0f);
    REQUIRE(data[4] == 8.0f);
}

TEST_CASE("Digital Event Series - Add Event", "[DataManager]") {
    DigitalEventSeries des;

    // Add events in arbitrary order
    des.addEvent(3.0f);
    des.addEvent(1.0f);
    des.addEvent(5.0f);
    des.addEvent(2.0f);

    // Verify that events are sorted after each addition
    auto data = des.getEventSeries();
    REQUIRE(data.size() == 4);
    REQUIRE(data[0] == 1.0f);
    REQUIRE(data[1] == 2.0f);
    REQUIRE(data[2] == 3.0f);
    REQUIRE(data[3] == 5.0f);
}

TEST_CASE("Digital Event Series - Remove Event", "[DataManager]") {
    std::vector<float> events = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    DigitalEventSeries des(events);

    // Remove an event that exists
    bool removed = des.removeEvent(3.0f);
    REQUIRE(removed);

    // Check that the event was removed
    auto data = des.getEventSeries();
    REQUIRE(data.size() == 4);
    REQUIRE(data[0] == 1.0f);
    REQUIRE(data[1] == 2.0f);
    REQUIRE(data[2] == 4.0f);
    REQUIRE(data[3] == 5.0f);

    // Try to remove an event that doesn't exist
    removed = des.removeEvent(10.0f);
    REQUIRE_FALSE(removed);
    REQUIRE(data.size() == 4);
}

TEST_CASE("Digital Event Series - Clear", "[DataManager]") {
    std::vector<float> events = {1.0f, 2.0f, 3.0f};
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

            auto des = DigitalEventSeriesLoader::loadFromCSV(filename);
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
    des.addEvent(2.0f);
    des.addEvent(1.0f);
    des.addEvent(2.0f);
    des.addEvent(3.0f);
    des.addEvent(1.0f);

    // Verify that duplicates are preserved and sorted
    auto data = des.getEventSeries();
    REQUIRE(data.size() == 5);
    REQUIRE(data[0] == 1.0f);
    REQUIRE(data[1] == 1.0f);
    REQUIRE(data[2] == 2.0f);
    REQUIRE(data[3] == 2.0f);
    REQUIRE(data[4] == 3.0f);
}

TEST_CASE("Digital Event Series - Empty Series", "[DataManager]") {
    DigitalEventSeries des;

    // Test with empty series
    auto data = des.getEventSeries();
    REQUIRE(data.empty());

    // Removing from empty series should return false
    bool removed = des.removeEvent(1.0f);
    REQUIRE_FALSE(removed);

    // Add and then remove to get back to empty
    des.addEvent(5.0f);
    REQUIRE(des.size() == 1);

    des.removeEvent(5.0f);
    REQUIRE(des.size() == 0);
}
