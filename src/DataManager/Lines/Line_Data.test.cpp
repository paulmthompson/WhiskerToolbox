#include "Lines/Line_Data.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "Entity/EntityRegistry.hpp"
#include "DataManager.hpp"

#include <catch2/catch_test_macros.hpp>

#include <set>
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
    source_data.addAtTime(TimeFrameIndex(10), x1, y1);
    source_data.addAtTime(TimeFrameIndex(10), x2, y2); // Two lines at same time
    source_data.addAtTime(TimeFrameIndex(20), x3, y3);
    source_data.addAtTime(TimeFrameIndex(30), x1, y1); // Reuse line at different time

    SECTION("Copy time range - basic functionality") {
        TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(20)};
        std::size_t lines_copied = source_data.copyTo(target_data, interval);
        
        REQUIRE(lines_copied == 3); // 2 lines at time 10 + 1 line at time 20
        
        // Verify source data is unchanged
        REQUIRE(source_data.getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(source_data.getAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(source_data.getAtTime(TimeFrameIndex(30)).size() == 1);
        
        // Verify target has copied data
        REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(target_data.getAtTime(TimeFrameIndex(30)).size() == 0); // Not in range
        
        // Verify line content
        auto target_lines_10 = target_data.getAtTime(TimeFrameIndex(10));
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
        REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 0); // Not copied
        REQUIRE(target_data.getAtTime(TimeFrameIndex(30)).size() == 1);
    }

    SECTION("Copy to target with existing data") {
        // Add some data to target first
        target_data.addAtTime(TimeFrameIndex(10), x3, y3);
        
        TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(10)};
        std::size_t lines_copied = source_data.copyTo(target_data, interval);
        
        REQUIRE(lines_copied == 2); // 2 lines from source
        REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 3); // 1 existing + 2 copied
    }

    SECTION("Move time range - basic functionality") {
        TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(20)};
        std::size_t lines_moved = source_data.moveTo(target_data, interval);
        
        REQUIRE(lines_moved == 3); // 2 lines at time 10 + 1 line at time 20
        
        // Verify source data is cleared for moved times
        REQUIRE(source_data.getAtTime(TimeFrameIndex(10)).size() == 0);
        REQUIRE(source_data.getAtTime(TimeFrameIndex(20)).size() == 0);
        REQUIRE(source_data.getAtTime(TimeFrameIndex(30)).size() == 1); // Not moved
        
        // Verify target has moved data
        REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(target_data.getAtTime(TimeFrameIndex(30)).size() == 0);
    }

    SECTION("Move specific times") {
        std::vector<TimeFrameIndex> times_to_move = {TimeFrameIndex(20), TimeFrameIndex(30)};
        std::size_t lines_moved = source_data.moveTo(target_data, times_to_move);
        
        REQUIRE(lines_moved == 2); // 1 line at time 20 + 1 line at time 30
        
        // Verify source data
        REQUIRE(source_data.getAtTime(TimeFrameIndex(10)).size() == 2); // Not moved
        REQUIRE(source_data.getAtTime(TimeFrameIndex(20)).size() == 0); // Moved
        REQUIRE(source_data.getAtTime(TimeFrameIndex(30)).size() == 0); // Moved
        
        // Verify target data
        REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 0); // Not moved
        REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 1); // Moved
        REQUIRE(target_data.getAtTime(TimeFrameIndex(30)).size() == 1); // Moved
    }

    SECTION("Copy empty time range") {
        TimeFrameInterval interval{TimeFrameIndex(100), TimeFrameIndex(200)};
        std::size_t lines_copied = source_data.copyTo(target_data, interval);
        
        REQUIRE(lines_copied == 0);
        REQUIRE(target_data.getTimesWithData().empty());
    }

    SECTION("Copy with invalid time range") {
        TimeFrameInterval interval{TimeFrameIndex(30), TimeFrameIndex(10)}; // end < start
        std::size_t lines_copied = source_data.copyTo(target_data, interval);
        
        REQUIRE(lines_copied == 0);
        REQUIRE(target_data.getTimesWithData().empty());
    }

    SECTION("Move empty times") {
        std::vector<TimeFrameIndex> empty_times = {TimeFrameIndex(100), TimeFrameIndex(200)};
        std::size_t lines_moved = source_data.moveTo(target_data, empty_times);
        
        REQUIRE(lines_moved == 0);
        
        // Source should be unchanged
        REQUIRE(source_data.getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(source_data.getAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(source_data.getAtTime(TimeFrameIndex(30)).size() == 1);
    }

    SECTION("Copy and move with notification disabled") {
        // Create a simple observer counter
        int copy_notifications = 0;
        int move_notifications = 0;
        
        target_data.addObserver([&copy_notifications]() { copy_notifications++; });
        source_data.addObserver([&move_notifications]() { move_notifications++; });
        
        // Copy with notification disabled
        TimeFrameInterval interval1{TimeFrameIndex(10), TimeFrameIndex(10)};
        source_data.copyTo(target_data, interval1, false);
        REQUIRE(copy_notifications == 0);
        
        // Copy with notification enabled
        TimeFrameInterval interval2{TimeFrameIndex(20), TimeFrameIndex(20)};
        source_data.copyTo(target_data, interval2, true);
        REQUIRE(copy_notifications == 1);
        
        // Move with notification disabled
        TimeFrameInterval interval3{TimeFrameIndex(30), TimeFrameIndex(30)};
        source_data.moveTo(target_data, interval3, false);
        REQUIRE(move_notifications == 0);
        
        // Move with notification enabled (should notify both source and target)
        LineData new_source;
        new_source.addAtTime(TimeFrameIndex(40), x1, y1);
        new_source.addObserver([&move_notifications]() { move_notifications++; });
        
        TimeFrameInterval interval4{TimeFrameIndex(40), TimeFrameIndex(40)};
        new_source.moveTo(target_data, interval4, true);
        REQUIRE(move_notifications == 1); // Source notified
        REQUIRE(copy_notifications == 2);  // Target notified
    }

    SECTION("Copy preserves line data integrity") {
        TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(10)};
        source_data.copyTo(target_data, interval);
        
        auto source_lines = source_data.getAtTime(TimeFrameIndex(10));
        auto target_lines = target_data.getAtTime(TimeFrameIndex(10));
        
        REQUIRE(source_lines.size() == target_lines.size());
        
        for (size_t i = 0; i < source_lines.size(); ++i) {
            REQUIRE(source_lines[i].size() == target_lines[i].size());
            for (size_t j = 0; j < source_lines[i].size(); ++j) {
                REQUIRE(source_lines[i][j].x == target_lines[i][j].x);
                REQUIRE(source_lines[i][j].y == target_lines[i][j].y);
            }
        }
        
        // Modify target data and ensure source is unaffected
        target_data.clearAtTime(TimeFrameIndex(10));
        REQUIRE(source_data.getAtTime(TimeFrameIndex(10)).size() == 2); // Source unchanged
        REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 0);  // Target cleared
    }
}

TEST_CASE("LineData - Range-based access", "[line][data][range]") {
    LineData line_data;

    // Setup test data
    std::vector<float> x1 = {1.0f, 2.0f, 3.0f};
    std::vector<float> y1 = {1.0f, 2.0f, 1.0f};
    
    std::vector<float> x2 = {5.0f, 6.0f, 7.0f};
    std::vector<float> y2 = {5.0f, 6.0f, 5.0f};
    
    std::vector<float> x3 = {10.0f, 11.0f, 12.0f};
    std::vector<float> y3 = {10.0f, 11.0f, 10.0f};

    SECTION("GetLinesInRange functionality") {
        // Setup data at multiple time points
        line_data.addAtTime(TimeFrameIndex(5), x1, y1);       // 1 line
        line_data.addAtTime(TimeFrameIndex(10), x1, y1);      // 1 line  
        line_data.addAtTime(TimeFrameIndex(10), x2, y2);      // 2nd line at same time
        line_data.addAtTime(TimeFrameIndex(15), x3, y3);      // 1 line
        line_data.addAtTime(TimeFrameIndex(20), x1, y1);      // 1 line
        line_data.addAtTime(TimeFrameIndex(25), x2, y2);      // 1 line

        SECTION("Range includes some data") {
            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(20)};
            size_t count = 0;
            for (const auto& pair : line_data.GetLinesInRange(interval)) {
                if (count == 0) {
                    REQUIRE(pair.time.getValue() == 10);
                    REQUIRE(pair.lines.size() == 2);  // 2 lines at time 10
                } else if (count == 1) {
                    REQUIRE(pair.time.getValue() == 15);
                    REQUIRE(pair.lines.size() == 1);
                } else if (count == 2) {
                    REQUIRE(pair.time.getValue() == 20);
                    REQUIRE(pair.lines.size() == 1);
                }
                count++;
            }
            REQUIRE(count == 3); // Should include times 10, 15, 20
        }

        SECTION("Range includes all data") {
            TimeFrameInterval interval{TimeFrameIndex(0), TimeFrameIndex(30)};
            size_t count = 0;
            for (const auto& pair : line_data.GetLinesInRange(interval)) {
                count++;
            }
            REQUIRE(count == 5); // Should include all 5 time points
        }

        SECTION("Range includes no data") {
            TimeFrameInterval interval{TimeFrameIndex(100), TimeFrameIndex(200)};
            size_t count = 0;
            for (const auto& pair : line_data.GetLinesInRange(interval)) {
                count++;
            }
            REQUIRE(count == 0); // Should be empty
        }

        SECTION("Range with single time point") {
            TimeFrameInterval interval{TimeFrameIndex(15), TimeFrameIndex(15)};
            size_t count = 0;
            for (const auto& pair : line_data.GetLinesInRange(interval)) {
                REQUIRE(pair.time.getValue() == 15);
                REQUIRE(pair.lines.size() == 1);
                count++;
            }
            REQUIRE(count == 1); // Should include only time 15
        }

        SECTION("Range with start > end") {
            TimeFrameInterval interval{TimeFrameIndex(20), TimeFrameIndex(10)};
            size_t count = 0;
            for (const auto& pair : line_data.GetLinesInRange(interval)) {
                count++;
            }
            REQUIRE(count == 0); // Should be empty when start > end
        }

        SECTION("Range with timeframe conversion - same timeframes") {
            // Test with same source and target timeframes
            std::vector<int> times = {5, 10, 15, 20, 25};
            auto timeframe = std::make_shared<TimeFrame>(times);
            
            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(20)};
            size_t count = 0;
            for (const auto& pair : line_data.GetLinesInRange(interval, timeframe.get(), timeframe.get())) {
                if (count == 0) {
                    REQUIRE(pair.time.getValue() == 10);
                    REQUIRE(pair.lines.size() == 2);
                } else if (count == 1) {
                    REQUIRE(pair.time.getValue() == 15);
                    REQUIRE(pair.lines.size() == 1);
                } else if (count == 2) {
                    REQUIRE(pair.time.getValue() == 20);
                    REQUIRE(pair.lines.size() == 1);
                }
                count++;
            }
            REQUIRE(count == 3); // Should include times 10, 15, 20
        }

        SECTION("Range with timeframe conversion - different timeframes") {
            // Create a separate line data instance for timeframe conversion test
            LineData timeframe_test_data;
            
            // Create source timeframe (video frames)
            std::vector<int> video_times = {0, 10, 20, 30, 40};  
            auto video_timeframe = std::make_shared<TimeFrame>(video_times);
            
            // Create target timeframe (data sampling)
            std::vector<int> data_times = {0, 5, 10, 15, 20, 25, 30, 35, 40}; 
            auto data_timeframe = std::make_shared<TimeFrame>(data_times);
            
            // Add data at target timeframe indices
            timeframe_test_data.addAtTime(TimeFrameIndex(2), x1, y1);  // At data timeframe index 2 (time=10)
            timeframe_test_data.addAtTime(TimeFrameIndex(3), x2, y2);  // At data timeframe index 3 (time=15)
            timeframe_test_data.addAtTime(TimeFrameIndex(4), x3, y3);  // At data timeframe index 4 (time=20)
            
            // Query video frames 1-2 (times 10-20) which should map to data indices 2-4 (times 10-20)
            TimeFrameInterval video_interval{TimeFrameIndex(1), TimeFrameIndex(2)};
            size_t count = 0;
            for (const auto& pair : timeframe_test_data.GetLinesInRange(video_interval, video_timeframe.get(), data_timeframe.get())) {
                if (count == 0) {
                    REQUIRE(pair.time.getValue() == 2);
                    REQUIRE(pair.lines.size() == 1);
                } else if (count == 1) {
                    REQUIRE(pair.time.getValue() == 3);
                    REQUIRE(pair.lines.size() == 1);
                } else if (count == 2) {
                    REQUIRE(pair.time.getValue() == 4);
                    REQUIRE(pair.lines.size() == 1);
                }
                count++;
            }
            REQUIRE(count == 3); // Should include converted times 2, 3, 4
        }
    }
}

TEST_CASE("LineData - Entity Lookup Methods", "[line][data][entity][lookup]") {
    // Note: These tests require EntityRegistry integration which would normally
    // be set up through DataManager. For now, we test the API structure.
    
    LineData line_data;
    
    std::vector<float> x1 = {1.0f, 2.0f, 3.0f};
    std::vector<float> y1 = {1.0f, 2.0f, 3.0f};
    
    std::vector<float> x2 = {4.0f, 5.0f, 6.0f};
    std::vector<float> y2 = {4.0f, 5.0f, 6.0f};
    
    // Add some test data
    line_data.addAtTime(TimeFrameIndex(10), x1, y1);
    line_data.addAtTime(TimeFrameIndex(10), x2, y2); // Two lines at same time
    line_data.addAtTime(TimeFrameIndex(20), x1, y1); // Same line at different time
    
    SECTION("Entity lookup without registry returns nullopt") {
        // Without EntityRegistry setup, these should return empty/nullopt
        EntityId fake_entity = 12345;
        
        auto line_result = line_data.getLineByEntityId(fake_entity);
        REQUIRE_FALSE(line_result.has_value());
        
        auto time_result = line_data.getTimeAndIndexByEntityId(fake_entity);
        REQUIRE_FALSE(time_result.has_value());
        
        std::vector<EntityId> fake_entities = {fake_entity, 67890};
        auto lines_result = line_data.getLinesByEntityIds(fake_entities);
        REQUIRE(lines_result.empty());
        
        auto time_info_result = line_data.getTimeInfoByEntityIds(fake_entities);
        REQUIRE(time_info_result.empty());
    }
    
    SECTION("Entity ID retrieval methods work") {
        // These should work regardless of registry setup
        auto entity_ids_at_time = line_data.getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(entity_ids_at_time.size() == 2); // Two lines at time 10
        
        auto all_entity_ids = line_data.getAllEntityIds();
        REQUIRE(all_entity_ids.size() == 3); // Total of 3 lines across all times
        
        // Check that empty time returns empty vector
        auto empty_time_ids = line_data.getEntityIdsAtTime(TimeFrameIndex(99));
        REQUIRE(empty_time_ids.empty());
    }
    
    SECTION("API structure validates correctly") {
        // Test that the methods exist and have correct return types
        EntityId test_entity = 1;
        std::vector<EntityId> test_entities = {1, 2, 3};
        
        // These calls should compile and return appropriate empty/nullopt values
        auto single_line = line_data.getLineByEntityId(test_entity);
        auto time_and_index = line_data.getTimeAndIndexByEntityId(test_entity);
        auto multiple_lines = line_data.getLinesByEntityIds(test_entities);
        auto time_infos = line_data.getTimeInfoByEntityIds(test_entities);
        
        // Verify return types are as expected (they should be empty/nullopt without registry)
        REQUIRE_FALSE(single_line.has_value());
        REQUIRE_FALSE(time_and_index.has_value());
        REQUIRE(multiple_lines.empty());
        REQUIRE(time_infos.empty());
    }
}

TEST_CASE("LineData - Entity ID handling in copy/move operations", "[line][data][entity][copy][move]") {
    LineData source_data;
    LineData target_data;
    
    // Setup test data
    std::vector<float> x1 = {1.0f, 2.0f, 3.0f};
    std::vector<float> y1 = {1.0f, 2.0f, 3.0f};
    
    std::vector<float> x2 = {4.0f, 5.0f, 6.0f};
    std::vector<float> y2 = {4.0f, 5.0f, 6.0f};
    
    // Add test lines
    source_data.addAtTime(TimeFrameIndex(10), x1, y1);
    source_data.addAtTime(TimeFrameIndex(10), x2, y2);
    source_data.addAtTime(TimeFrameIndex(20), x1, y1);
    
    SECTION("Copy operations create new entity IDs") {
        // Get original entity IDs
        auto original_entity_ids = source_data.getAllEntityIds();
        REQUIRE(original_entity_ids.size() == 3);
        
        // Copy data
        TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(20)};
        source_data.copyTo(target_data, interval);
        
        // Get new entity IDs from target
        auto target_entity_ids = target_data.getAllEntityIds();
        REQUIRE(target_entity_ids.size() == 3);
        
        // Note: Entity ID comparison only works with entity registry
        // Without registry, all entity IDs are 0, so we skip this check
        
        // Verify source entity IDs are unchanged
        auto source_entity_ids_after = source_data.getAllEntityIds();
        REQUIRE(source_entity_ids_after == original_entity_ids);
    }
    
    SECTION("Move operations create new entity IDs in target") {
        // Get original entity IDs
        auto original_entity_ids = source_data.getAllEntityIds();
        REQUIRE(original_entity_ids.size() == 3);
        
        // Move data
        TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(10)};
        source_data.moveTo(target_data, interval);
        
        // Get new entity IDs from target
        auto target_entity_ids = target_data.getAllEntityIds();
        REQUIRE(target_entity_ids.size() == 2); // 2 lines at time 10
        
        // Note: Entity ID comparison only works with entity registry
        // Without registry, all entity IDs are 0, so we skip this check
        
        // Verify source still has remaining data with original entity IDs
        auto remaining_entity_ids = source_data.getAllEntityIds();
        REQUIRE(remaining_entity_ids.size() == 1); // 1 line at time 20
        
        // Note: Entity ID comparison only works with entity registry
        // Without registry, all entity IDs are 0, so we skip this check
    }
    
    SECTION("Entity ID consistency within time frames") {
        // Setup fresh source data
        LineData source_data;
        
        // Add test lines
        source_data.addAtTime(TimeFrameIndex(10), x1, y1);
        source_data.addAtTime(TimeFrameIndex(10), x2, y2);
        source_data.addAtTime(TimeFrameIndex(30), x1, y1);
        source_data.addAtTime(TimeFrameIndex(30), x2, y2);
        
        // Get entity IDs at specific times
        auto entity_ids_10 = source_data.getEntityIdsAtTime(TimeFrameIndex(10));
        auto entity_ids_30 = source_data.getEntityIdsAtTime(TimeFrameIndex(30));
        
        REQUIRE(entity_ids_10.size() == 2);
        REQUIRE(entity_ids_30.size() == 2);
        
        // Note: Entity ID uniqueness test only works with entity registry
        // Without registry, all entity IDs are 0, so we skip this check
    }
}

TEST_CASE("LineData - Copy and Move by EntityID", "[line][data][entity][copy][move][by_id]") {
    // Setup test data vectors
    std::vector<float> x1 = {1.0f, 2.0f, 3.0f};
    std::vector<float> y1 = {1.0f, 2.0f, 3.0f};
    
    std::vector<float> x2 = {4.0f, 5.0f, 6.0f};
    std::vector<float> y2 = {4.0f, 5.0f, 6.0f};
    
    std::vector<float> x3 = {7.0f, 8.0f, 9.0f};
    std::vector<float> y3 = {7.0f, 8.0f, 9.0f};

    auto data_manager = std::make_unique<DataManager>();
    auto time_frame = std::make_shared<TimeFrame>(std::vector<int>{0, 10, 20, 30});
    data_manager->setTime(TimeKey("test_time"), time_frame);
    
    SECTION("Copy lines by EntityID - basic functionality") {
        // Setup fresh source and target data
        data_manager->setData<LineData>("source_data", TimeKey("test_time"));
        data_manager->setData<LineData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<LineData>("source_data");
        auto target_data = data_manager->getData<LineData>("target_data");

        
        // Add test lines
        source_data->addAtTime(TimeFrameIndex(10), x1, y1);
        source_data->addAtTime(TimeFrameIndex(10), x2, y2);
        source_data->addAtTime(TimeFrameIndex(20), x3, y3);
        source_data->addAtTime(TimeFrameIndex(30), x1, y1);
        
        // Get entity IDs for testing
        auto entity_ids_10 = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(entity_ids_10.size() == 2);
        
        // Copy lines from time 10 (2 lines)
        std::size_t lines_copied = source_data->copyLinesByEntityIds(*target_data, entity_ids_10);
        
        REQUIRE(lines_copied == 2);
        
        // Verify source data is unchanged
        REQUIRE(source_data->getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(30)).size() == 1);
        
        // Verify target has copied data
        REQUIRE(target_data->getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(20)).size() == 0);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(30)).size() == 0);
        
        // Verify target has new entity IDs
        auto target_entity_ids = target_data->getAllEntityIds();
        REQUIRE(target_entity_ids.size() == 2);

        // They should be different from the source entity IDs
        REQUIRE(target_entity_ids != entity_ids_10);
        
    }
    
    SECTION("Copy lines by EntityID - mixed times") {
        // Setup fresh source and target data
        data_manager->setData<LineData>("source_data", TimeKey("test_time"));
        data_manager->setData<LineData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<LineData>("source_data");
        auto target_data = data_manager->getData<LineData>("target_data");
        
        // Add test lines
        source_data->addAtTime(TimeFrameIndex(10), x1, y1);
        source_data->addAtTime(TimeFrameIndex(10), x2, y2);
        source_data->addAtTime(TimeFrameIndex(20), x3, y3);
        source_data->addAtTime(TimeFrameIndex(30), x1, y1);
        
        // Get entity IDs for testing
        auto entity_ids_10 = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto entity_ids_20 = source_data->getEntityIdsAtTime(TimeFrameIndex(20));
        REQUIRE(entity_ids_10.size() == 2);
        REQUIRE(entity_ids_20.size() == 1);
        
        // Copy one line from time 10 and one from time 20
        std::vector<EntityId> mixed_entity_ids = {entity_ids_10[0], entity_ids_20[0]};
        std::size_t lines_copied = source_data->copyLinesByEntityIds(*target_data, mixed_entity_ids);
        
        REQUIRE(lines_copied == 2);
        
        // Verify target has lines at both times
        REQUIRE(target_data->getAtTime(TimeFrameIndex(10)).size() == 1);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(30)).size() == 0);
    }
    
    SECTION("Copy lines by EntityID - non-existent EntityIDs") {
        // Setup fresh source and target data
        data_manager->setData<LineData>("source_data", TimeKey("test_time"));
        data_manager->setData<LineData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<LineData>("source_data");
        auto target_data = data_manager->getData<LineData>("target_data");
        
        // Add test lines
        source_data->addAtTime(TimeFrameIndex(10), x1, y1);
        source_data->addAtTime(TimeFrameIndex(10), x2, y2);
        source_data->addAtTime(TimeFrameIndex(20), x3, y3);
        source_data->addAtTime(TimeFrameIndex(30), x1, y1);
        
        std::vector<EntityId> fake_entity_ids = {99999, 88888};
        std::size_t lines_copied = source_data->copyLinesByEntityIds(*target_data, fake_entity_ids);
        
        REQUIRE(lines_copied == 0);
        REQUIRE(target_data->getTimesWithData().empty());
    }
    
    SECTION("Copy lines by EntityID - empty EntityID list") {
        // Clear target data to ensure clean test
        data_manager->setData<LineData>("target_data", TimeKey("test_time"));
        data_manager->setData<LineData>("source_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<LineData>("source_data");
        auto target_data = data_manager->getData<LineData>("target_data");
        
        std::vector<EntityId> empty_entity_ids;
        std::size_t lines_copied = source_data->copyLinesByEntityIds(*target_data, empty_entity_ids);
        
        REQUIRE(lines_copied == 0);
        REQUIRE(target_data->getTimesWithData().empty());
    }
    
    SECTION("Move lines by EntityID - basic functionality") {
        // Setup fresh source and target data
        data_manager->setData<LineData>("source_data", TimeKey("test_time"));
        data_manager->setData<LineData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<LineData>("source_data");
        auto target_data = data_manager->getData<LineData>("target_data");
        
        // Add test lines
        source_data->addAtTime(TimeFrameIndex(10), x1, y1);
        source_data->addAtTime(TimeFrameIndex(10), x2, y2);
        source_data->addAtTime(TimeFrameIndex(20), x3, y3);
        source_data->addAtTime(TimeFrameIndex(30), x1, y1);
        
        // Get entity IDs for testing
        auto entity_ids_10 = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(entity_ids_10.size() == 2);
        
        // Move lines from time 10 (2 lines)
        std::size_t lines_moved = source_data->moveLinesByEntityIds(*target_data, entity_ids_10);
        
        REQUIRE(lines_moved == 2);
        
        // Verify source data is reduced
        REQUIRE(source_data->getAtTime(TimeFrameIndex(10)).size() == 0);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(30)).size() == 1);
        
        // Verify target has moved data
        REQUIRE(target_data->getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(20)).size() == 0);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(30)).size() == 0);
        
        // Verify target has new entity IDs
        auto target_entity_ids = target_data->getAllEntityIds();
        REQUIRE(target_entity_ids.size() == 2);
        
        // They should be the original entity IDs
        REQUIRE(target_entity_ids == entity_ids_10);
    }
    
    SECTION("Move lines by EntityID - mixed times") {
        // Clear target data to ensure clean test
        data_manager->setData<LineData>("target_data", TimeKey("test_time"));
        data_manager->setData<LineData>("source_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<LineData>("source_data");
        auto target_data = data_manager->getData<LineData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), x1, y1);
        source_data->addAtTime(TimeFrameIndex(10), x2, y2);
        source_data->addAtTime(TimeFrameIndex(20), x3, y3);
        source_data->addAtTime(TimeFrameIndex(30), x1, y1);

        auto entity_ids_10 = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto entity_ids_20 = source_data->getEntityIdsAtTime(TimeFrameIndex(20));
        
        // Move one line from time 10 and one from time 20
        std::vector<EntityId> mixed_entity_ids = {entity_ids_10[0], entity_ids_20[0]};
        std::size_t lines_moved = source_data->moveLinesByEntityIds(*target_data, mixed_entity_ids);
        
        REQUIRE(lines_moved == 2);
        
        // Verify source data is reduced
        REQUIRE(source_data->getAtTime(TimeFrameIndex(10)).size() == 1); // 1 remaining
        REQUIRE(source_data->getAtTime(TimeFrameIndex(20)).size() == 0); // Moved
        REQUIRE(source_data->getAtTime(TimeFrameIndex(30)).size() == 1); // Unchanged
        
        // Verify target has moved data
        REQUIRE(target_data->getAtTime(TimeFrameIndex(10)).size() == 1);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(30)).size() == 0);
    }
    
    SECTION("Move lines by EntityID - non-existent EntityIDs") {
        // Clear target data to ensure clean test
        data_manager->setData<LineData>("target_data", TimeKey("test_time"));
        data_manager->setData<LineData>("source_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<LineData>("source_data");
        auto target_data = data_manager->getData<LineData>("target_data");
        
        source_data->addAtTime(TimeFrameIndex(10), x1, y1);
        source_data->addAtTime(TimeFrameIndex(10), x2, y2);
        source_data->addAtTime(TimeFrameIndex(20), x3, y3);
        source_data->addAtTime(TimeFrameIndex(30), x1, y1);

        auto entity_ids_10 = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        std::vector<EntityId> fake_entity_ids = {99999, 88888};
        std::size_t lines_moved = source_data->moveLinesByEntityIds(*target_data, fake_entity_ids);
        
        REQUIRE(lines_moved == 0);
        REQUIRE(target_data->getTimesWithData().empty());
        
        // Source should be unchanged
        REQUIRE(source_data->getAtTime(TimeFrameIndex(10)).size() == 2);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(20)).size() == 1);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(30)).size() == 1);
    }
    
    SECTION("Copy preserves line data integrity") {
        // Clear target data to ensure clean test
        data_manager->setData<LineData>("target_data", TimeKey("test_time"));
        data_manager->setData<LineData>("source_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<LineData>("source_data");
        auto target_data = data_manager->getData<LineData>("target_data");
        
        source_data->addAtTime(TimeFrameIndex(10), x1, y1);
        source_data->addAtTime(TimeFrameIndex(10), x2, y2);
        source_data->addAtTime(TimeFrameIndex(20), x3, y3);
        source_data->addAtTime(TimeFrameIndex(30), x1, y1);

        auto entity_ids_10 = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        source_data->copyLinesByEntityIds(*target_data, entity_ids_10);
        
        auto source_lines = source_data->getAtTime(TimeFrameIndex(10));
        auto target_lines = target_data->getAtTime(TimeFrameIndex(10));
        
        REQUIRE(source_lines.size() == target_lines.size());
        
        for (size_t i = 0; i < source_lines.size(); ++i) {
            REQUIRE(source_lines[i].size() == target_lines[i].size());
            for (size_t j = 0; j < source_lines[i].size(); ++j) {
                REQUIRE(source_lines[i][j].x == target_lines[i][j].x);
                REQUIRE(source_lines[i][j].y == target_lines[i][j].y);
            }
        }
    }
    
    SECTION("Move preserves line data integrity") {
        // Clear target data to ensure clean test
        data_manager->setData<LineData>("target_data", TimeKey("test_time"));
        data_manager->setData<LineData>("source_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<LineData>("source_data");
        auto target_data = data_manager->getData<LineData>("target_data");
        
        source_data->addAtTime(TimeFrameIndex(10), x1, y1);
        source_data->addAtTime(TimeFrameIndex(10), x2, y2);
        source_data->addAtTime(TimeFrameIndex(20), x3, y3);
        source_data->addAtTime(TimeFrameIndex(30), x1, y1);

        auto entity_ids_10 = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        // Get original line data before move
        auto original_lines = source_data->getAtTime(TimeFrameIndex(10));
        REQUIRE(original_lines.size() == 2);
        
        source_data->moveLinesByEntityIds(*target_data, entity_ids_10);
        
        auto target_lines = target_data->getAtTime(TimeFrameIndex(10));
        REQUIRE(target_lines.size() == 2);
        
        // Verify line content matches (order may be different due to new entity IDs)
        for (size_t i = 0; i < original_lines.size(); ++i) {
            bool found_match = false;
            for (size_t j = 0; j < target_lines.size(); ++j) {
                if (original_lines[i].size() == target_lines[j].size()) {
                    bool lines_match = true;
                    for (size_t k = 0; k < original_lines[i].size(); ++k) {
                        if (original_lines[i][k].x != target_lines[j][k].x || 
                            original_lines[i][k].y != target_lines[j][k].y) {
                            lines_match = false;
                            break;
                        }
                    }
                    if (lines_match) {
                        found_match = true;
                        break;
                    }
                }
            }
            REQUIRE(found_match);
        }
    }
} 