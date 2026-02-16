#include "Lines/Line_Data.hpp"
#include "DataManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "fixtures/entity_id.hpp"
#include "fixtures/DataManagerFixture.hpp"
#include "fixtures/RaggedTimeSeriesTestTraits.hpp"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <vector>

TEST_CASE("LineData - Range-based access", "[line][data][range]") {
    LineData line_data;

    std::vector<int> times = {5, 10, 15, 20, 25};
    auto timeframe = std::make_shared<TimeFrame>(times);

    line_data.setTimeFrame(timeframe);

    // Setup test data using traits for convenience
    using Traits = RaggedDataTestTraits<LineData>;
    auto item1 = Traits::createSingleItem();
    auto item2 = Traits::createAnotherItem();

    SECTION("getElementsInRange functionality") {
        // Setup data at multiple time points
        line_data.addAtTime(TimeFrameIndex(5), item1, NotifyObservers::No); // 1 line
        line_data.addAtTime(TimeFrameIndex(10), item1, NotifyObservers::No);// 1 line
        line_data.addAtTime(TimeFrameIndex(10), item2, NotifyObservers::No);// 2nd line at same time
        line_data.addAtTime(TimeFrameIndex(15), item2, NotifyObservers::No);// 1 line
        line_data.addAtTime(TimeFrameIndex(20), item1, NotifyObservers::No);// 1 line
        line_data.addAtTime(TimeFrameIndex(25), item2, NotifyObservers::No);// 1 line

        SECTION("Range includes some data") {
            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(20)};
            size_t count = 0;
            size_t entries_at_time_10 = 0;
            size_t entries_at_time_15 = 0;
            size_t entries_at_time_20 = 0;
            
            for (auto [time, entity_id, line_ref]: line_data.getElementsInRange(interval)) {
                (void)entity_id;
                (void)line_ref;
                if (time.getValue() == 10) entries_at_time_10++;
                else if (time.getValue() == 15) entries_at_time_15++;
                else if (time.getValue() == 20) entries_at_time_20++;
                count++;
            }
            REQUIRE(entries_at_time_10 == 2);// 2 lines at time 10
            REQUIRE(entries_at_time_15 == 1);
            REQUIRE(entries_at_time_20 == 1);
            REQUIRE(count == 4);// Total 4 entries across times 10, 15, 20
        }

        SECTION("Range includes all data") {
            TimeFrameInterval interval{TimeFrameIndex(0), TimeFrameIndex(30)};
            size_t count = 0;
            for (auto [time, entity_id, line_ref]: line_data.getElementsInRange(interval)) {
                (void)time;
                (void)entity_id;
                (void)line_ref;
                count++;
            }
            REQUIRE(count == 6);// Should include all 6 entries
        }

        SECTION("Range includes no data") {
            TimeFrameInterval interval{TimeFrameIndex(100), TimeFrameIndex(200)};
            size_t count = 0;
            for (auto [time, entity_id, line_ref]: line_data.getElementsInRange(interval)) {
                (void)time;
                (void)entity_id;
                (void)line_ref;
                count++;
            }
            REQUIRE(count == 0);// Should be empty
        }

        SECTION("Range with single time point") {
            TimeFrameInterval interval{TimeFrameIndex(15), TimeFrameIndex(15)};
            size_t count = 0;
            for (auto [time, entity_id, line_ref]: line_data.getElementsInRange(interval)) {
                (void)entity_id;
                (void)line_ref;
                REQUIRE(time.getValue() == 15);
                count++;
            }
            REQUIRE(count == 1);// Should include only time 15
        }

        SECTION("Range with start > end") {
            TimeFrameInterval interval{TimeFrameIndex(20), TimeFrameIndex(10)};
            size_t count = 0;
            for (auto [time, entity_id, line_ref]: line_data.getElementsInRange(interval)) {
                (void)time;
                (void)entity_id;
                (void)line_ref;
                count++;
            }
            REQUIRE(count == 0);// Should be empty when start > end
        }

        SECTION("Range with timeframe conversion - same timeframes") {

            // Test with same source and target timeframes
            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(20)};
            size_t count = 0;
            size_t entries_at_time_10 = 0;
            size_t entries_at_time_15 = 0;
            size_t entries_at_time_20 = 0;
            
            for (auto [time, entity_id, line_ref]: line_data.getElementsInRange(interval, *timeframe)) {
                (void)entity_id;
                (void)line_ref;
                if (time.getValue() == 10) entries_at_time_10++;
                else if (time.getValue() == 15) entries_at_time_15++;
                else if (time.getValue() == 20) entries_at_time_20++;
                count++;
            }
            REQUIRE(entries_at_time_10 == 2);
            REQUIRE(entries_at_time_15 == 1);
            REQUIRE(entries_at_time_20 == 1);
            REQUIRE(count == 4);// Total 4 entries across times 10, 15, 20
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
            timeframe_test_data.addAtTime(TimeFrameIndex(2), item1, NotifyObservers::No);// At data timeframe index 2 (time=10)
            timeframe_test_data.addAtTime(TimeFrameIndex(3), item2, NotifyObservers::No);// At data timeframe index 3 (time=15)
            timeframe_test_data.addAtTime(TimeFrameIndex(4), item1, NotifyObservers::No);// At data timeframe index 4 (time=20)

            timeframe_test_data.setTimeFrame(data_timeframe);

            // Query video frames 1-2 (times 10-20) which should map to data indices 2-4 (times 10-20)
            TimeFrameInterval video_interval{TimeFrameIndex(1), TimeFrameIndex(2)};
            size_t count = 0;
            size_t entries_at_idx_2 = 0;
            size_t entries_at_idx_3 = 0;
            size_t entries_at_idx_4 = 0;
            
            for (auto [time, entity_id, line_ref]: timeframe_test_data.getElementsInRange(video_interval, *video_timeframe)) {
                (void)entity_id;
                (void)line_ref;
                if (time.getValue() == 2) entries_at_idx_2++;
                else if (time.getValue() == 3) entries_at_idx_3++;
                else if (time.getValue() == 4) entries_at_idx_4++;
                count++;
            }
            REQUIRE(entries_at_idx_2 == 1);
            REQUIRE(entries_at_idx_3 == 1);
            REQUIRE(entries_at_idx_4 == 1);
            REQUIRE(count == 3);// Should include converted times 2, 3, 4
        }
    }
}

TEST_CASE("LineData - Entity Lookup Methods", "[line][data][entity][lookup]") {
    // Note: These tests require EntityRegistry integration which would normally
    // be set up through DataManager. For now, we test the API structure.

    LineData line_data;
    using Traits = RaggedDataTestTraits<LineData>;
    auto item1 = Traits::createSingleItem();
    auto item2 = Traits::createAnotherItem();

    // Add some test data
    line_data.addAtTime(TimeFrameIndex(10), item1, NotifyObservers::No);
    line_data.addAtTime(TimeFrameIndex(10), item2, NotifyObservers::No);// Two lines at same time
    line_data.addAtTime(TimeFrameIndex(20), item1, NotifyObservers::No);// Same line at different time

    SECTION("Entity lookup without registry returns nullopt") {
        // Without EntityRegistry setup, these should return empty/nullopt
        EntityId fake_entity = EntityId(12345);

        auto line_result = line_data.getDataByEntityId(fake_entity);
        REQUIRE_FALSE(line_result.has_value());

        std::vector<EntityId> fake_entities = {fake_entity, EntityId(67890)};
        auto lines_result = line_data.getDataByEntityIds(fake_entities);
        REQUIRE(std::ranges::distance(lines_result) == 0);
    }

    SECTION("Entity ID retrieval methods work") {
        // These should work regardless of registry setup
        auto entity_ids_at_time = line_data.getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(std::ranges::distance(entity_ids_at_time) == 2);// Two lines at time 10

        // Check that empty time returns empty vector
        auto empty_time_ids = line_data.getEntityIdsAtTime(TimeFrameIndex(99));
        REQUIRE(std::ranges::distance(empty_time_ids) == 0);
    }

    SECTION("API structure validates correctly") {
        // Test that the methods exist and have correct return types
        EntityId test_entity = EntityId(1);
        std::vector<EntityId> test_entities = {EntityId(1), EntityId(2), EntityId(3)};

        // These calls should compile and return appropriate empty/nullopt values
        auto single_line = line_data.getDataByEntityId(test_entity);
        auto multiple_lines = line_data.getDataByEntityIds(test_entities);

        // Verify return types are as expected (they should be empty/nullopt without registry)
        REQUIRE_FALSE(single_line.has_value());
        REQUIRE(std::ranges::distance(multiple_lines) == 0);
    }
}
