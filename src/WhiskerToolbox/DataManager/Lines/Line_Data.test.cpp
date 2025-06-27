#include "Lines/Line_Data.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

TEST_CASE("LineData - Copy and Move operations", "[line][data][copy][move]") {
    LineData source_data;
    LineData target_data;

    // Setup test data
    std::vector<float> x1 = {1.0f, 2.0f, 3.0f, 1.0f}; // Closed line
    std::vector<float> y1 = {1.0f, 1.0f, 2.0f, 2.0f};
    
    std::vector<float> x2 = {5.0f, 6.0f, 7.0f};
    std::vector<float> y2 = {5.0f, 6.0f, 5.0f};
    
    std::vector<float> x3 = {10.0f, 11.0f, 12.0f, 13.0f};
    std::vector<float> y3 = {10.0f, 11.0f, 10.0f, 11.0f};

    // Add test lines to source data
    source_data.addLineAtTime(TimeFrameIndex(10), x1, y1);
    source_data.addLineAtTime(TimeFrameIndex(10), x2, y2); // Two lines at same time
    source_data.addLineAtTime(TimeFrameIndex(20), x3, y3);
    source_data.addLineAtTime(TimeFrameIndex(30), x1, y1); // Reuse line at different time

    SECTION("Copy time range - basic functionality") {
        std::size_t lines_copied = source_data.copyTo(target_data, TimeFrameIndex(10), TimeFrameIndex(20));
        
        REQUIRE(lines_copied == 3); // 2 lines at time 10 + 1 line at time 20
        
        // Verify source data is unchanged
        REQUIRE(source_data.getLinesAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(source_data.getLinesAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(source_data.getLinesAtTime(TimeFrameIndex(30)).size() == 1);
        
        // Verify target has copied data
        REQUIRE(target_data.getLinesAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(target_data.getLinesAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(target_data.getLinesAtTime(TimeFrameIndex(30)).size() == 0); // Not in range
        
        // Verify line content
        auto target_lines_10 = target_data.getLinesAtTime(TimeFrameIndex(10));
        REQUIRE(target_lines_10[0].size() == 4); // First line has 4 points
        REQUIRE(target_lines_10[1].size() == 3); // Second line has 3 points
        REQUIRE(target_lines_10[0][0].x == 1.0f);
        REQUIRE(target_lines_10[0][0].y == 1.0f);
    }

    SECTION("Copy specific times") {
        std::vector<TimeFrameIndex> times_to_copy = {TimeFrameIndex(10), TimeFrameIndex(30)};
        std::size_t lines_copied = source_data.copyTo(target_data, times_to_copy);
        
        REQUIRE(lines_copied == 3); // 2 lines at time 10 + 1 line at time 30
        
        // Verify correct times were copied
        REQUIRE(target_data.getLinesAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(target_data.getLinesAtTime(TimeFrameIndex(20)).size() == 0); // Not copied
        REQUIRE(target_data.getLinesAtTime(TimeFrameIndex(30)).size() == 1);
    }

    SECTION("Copy to target with existing data") {
        // Add some data to target first
        target_data.addLineAtTime(TimeFrameIndex(10), x3, y3);
        
        std::size_t lines_copied = source_data.copyTo(target_data, TimeFrameIndex(10), TimeFrameIndex(10));
        
        REQUIRE(lines_copied == 2); // 2 lines from source
        REQUIRE(target_data.getLinesAtTime(TimeFrameIndex(10)).size() == 3); // 1 existing + 2 copied
    }

    SECTION("Move time range - basic functionality") {
        std::size_t lines_moved = source_data.moveTo(target_data, TimeFrameIndex(10), TimeFrameIndex(20));
        
        REQUIRE(lines_moved == 3); // 2 lines at time 10 + 1 line at time 20
        
        // Verify source data is cleared for moved times
        REQUIRE(source_data.getLinesAtTime(TimeFrameIndex(10)).size() == 0);
        REQUIRE(source_data.getLinesAtTime(TimeFrameIndex(20)).size() == 0);
        REQUIRE(source_data.getLinesAtTime(TimeFrameIndex(30)).size() == 1); // Not moved
        
        // Verify target has moved data
        REQUIRE(target_data.getLinesAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(target_data.getLinesAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(target_data.getLinesAtTime(TimeFrameIndex(30)).size() == 0);
    }

    SECTION("Move specific times") {
        std::vector<TimeFrameIndex> times_to_move = {TimeFrameIndex(20), TimeFrameIndex(30)};
        std::size_t lines_moved = source_data.moveTo(target_data, times_to_move);
        
        REQUIRE(lines_moved == 2); // 1 line at time 20 + 1 line at time 30
        
        // Verify source data
        REQUIRE(source_data.getLinesAtTime(TimeFrameIndex(10)).size() == 2); // Not moved
        REQUIRE(source_data.getLinesAtTime(TimeFrameIndex(20)).size() == 0); // Moved
        REQUIRE(source_data.getLinesAtTime(TimeFrameIndex(30)).size() == 0); // Moved
        
        // Verify target data
        REQUIRE(target_data.getLinesAtTime(TimeFrameIndex(10)).size() == 0); // Not moved
        REQUIRE(target_data.getLinesAtTime(TimeFrameIndex(20)).size() == 1); // Moved
        REQUIRE(target_data.getLinesAtTime(TimeFrameIndex(30)).size() == 1); // Moved
    }

    SECTION("Copy empty time range") {
        std::size_t lines_copied = source_data.copyTo(target_data, TimeFrameIndex(100), TimeFrameIndex(200));
        
        REQUIRE(lines_copied == 0);
        REQUIRE(target_data.getTimesWithData().empty());
    }

    SECTION("Copy with invalid time range") {
        std::size_t lines_copied = source_data.copyTo(target_data, TimeFrameIndex(30), TimeFrameIndex(10)); // end < start
        
        REQUIRE(lines_copied == 0);
        REQUIRE(target_data.getTimesWithData().empty());
    }

    SECTION("Move empty times") {
        std::vector<TimeFrameIndex> empty_times = {TimeFrameIndex(100), TimeFrameIndex(200)};
        std::size_t lines_moved = source_data.moveTo(target_data, empty_times);
        
        REQUIRE(lines_moved == 0);
        
        // Source should be unchanged
        REQUIRE(source_data.getLinesAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(source_data.getLinesAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(source_data.getLinesAtTime(TimeFrameIndex(30)).size() == 1);
    }

    SECTION("Copy and move with notification disabled") {
        // Create a simple observer counter
        int copy_notifications = 0;
        int move_notifications = 0;
        
        target_data.addObserver([&copy_notifications]() { copy_notifications++; });
        source_data.addObserver([&move_notifications]() { move_notifications++; });
        
        // Copy with notification disabled
        source_data.copyTo(target_data, TimeFrameIndex(10), TimeFrameIndex(10), false);
        REQUIRE(copy_notifications == 0);
        
        // Copy with notification enabled
        source_data.copyTo(target_data, TimeFrameIndex(20), TimeFrameIndex(20), true);
        REQUIRE(copy_notifications == 1);
        
        // Move with notification disabled
        source_data.moveTo(target_data, TimeFrameIndex(30), TimeFrameIndex(30), false);
        REQUIRE(move_notifications == 0);
        
        // Move with notification enabled (should notify both source and target)
        LineData new_source;
        new_source.addLineAtTime(TimeFrameIndex(40), x1, y1);
        new_source.addObserver([&move_notifications]() { move_notifications++; });
        
        new_source.moveTo(target_data, TimeFrameIndex(40), TimeFrameIndex(40), true);
        REQUIRE(move_notifications == 1); // Source notified
        REQUIRE(copy_notifications == 2);  // Target notified
    }

    SECTION("Copy preserves line data integrity") {
        source_data.copyTo(target_data, TimeFrameIndex(10), TimeFrameIndex(10));
        
        auto source_lines = source_data.getLinesAtTime(TimeFrameIndex(10));
        auto target_lines = target_data.getLinesAtTime(TimeFrameIndex(10));
        
        REQUIRE(source_lines.size() == target_lines.size());
        
        for (size_t i = 0; i < source_lines.size(); ++i) {
            REQUIRE(source_lines[i].size() == target_lines[i].size());
            for (size_t j = 0; j < source_lines[i].size(); ++j) {
                REQUIRE(source_lines[i][j].x == target_lines[i][j].x);
                REQUIRE(source_lines[i][j].y == target_lines[i][j].y);
            }
        }
        
        // Modify target data and ensure source is unaffected
        target_data.clearLinesAtTime(TimeFrameIndex(10));
        REQUIRE(source_data.getLinesAtTime(TimeFrameIndex(10)).size() == 2); // Source unchanged
        REQUIRE(target_data.getLinesAtTime(TimeFrameIndex(10)).size() == 0);  // Target cleared
    }
} 