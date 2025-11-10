#include "Masks/Mask_Data.hpp"
#include "DataManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

TEST_CASE("MaskData - Core functionality", "[mask][data][core]") {
    MaskData mask_data;

    // Test with same source and target timeframes
    std::vector<int> times = {5, 10, 15, 20, 25};
    auto timeframe = std::make_shared<TimeFrame>(times);

    mask_data.setTimeFrame(timeframe);

    // Setup some test data
    std::vector<uint32_t> x1 = {1, 2, 3, 1};
    std::vector<uint32_t> y1 = {1, 1, 2, 2};

    std::vector<uint32_t> x2 = {4, 5, 6, 4};
    std::vector<uint32_t> y2 = {3, 3, 4, 4};

    std::vector<Point2D<uint32_t>> points = {
            {10, 10},
            {11, 10},
            {11, 11},
            {10, 11}};

    SECTION("Adding masks at time") {
        // Add first mask at time 0
        mask_data.addAtTime(TimeFrameIndex(0), x1, y1);

        auto masks_at_0 = mask_data.getAtTime(TimeFrameIndex(0));
        REQUIRE(masks_at_0.size() == 1);
        REQUIRE(masks_at_0[0].size() == 4);
        REQUIRE(masks_at_0[0][0].x == 1);
        REQUIRE(masks_at_0[0][0].y == 1);

        // Add second mask at time 0
        mask_data.addAtTime(TimeFrameIndex(0), x2, y2);
        masks_at_0 = mask_data.getAtTime(TimeFrameIndex(0));
        REQUIRE(masks_at_0.size() == 2);
        REQUIRE(masks_at_0[1].size() == 4);
        REQUIRE(masks_at_0[1][0].x == 4);

        // Add mask at new time 10
        mask_data.addAtTime(TimeFrameIndex(10), points);
        auto masks_at_10 = mask_data.getAtTime(TimeFrameIndex(10));
        REQUIRE(masks_at_10.size() == 1);
        REQUIRE(masks_at_10[0].size() == 4);
        REQUIRE(masks_at_10[0][0].x == 10);
    }

    SECTION("Clearing masks at time") {
        // Add masks and then clear them
        mask_data.addAtTime(TimeFrameIndex(0), x1, y1);
        mask_data.addAtTime(TimeFrameIndex(0), x2, y2);
        mask_data.addAtTime(TimeFrameIndex(10), points);

        static_cast<void>(mask_data.clearAtTime(TimeIndexAndFrame(0, timeframe.get()),
                                                NotifyObservers::No));

        auto masks_at_0 = mask_data.getAtTime(TimeFrameIndex(0));
        auto masks_at_10 = mask_data.getAtTime(TimeFrameIndex(10));

        REQUIRE(masks_at_0.empty());
        REQUIRE(masks_at_10.size() == 1);
    }

    SECTION("Getting masks as range") {
        // Add multiple masks at different times
        mask_data.addAtTime(TimeFrameIndex(0), x1, y1);
        mask_data.addAtTime(TimeFrameIndex(0), x2, y2);
        mask_data.addAtTime(TimeFrameIndex(10), points);

        auto range = mask_data.getAllAsRange();

        // Count items in range
        size_t count = 0;
        TimeFrameIndex first_time = TimeFrameIndex(-1);
        size_t first_size = 0;

        for (auto const & pair: range) {
            if (count == 0) {
                first_time = pair.time;
                first_size = pair.masks.size();
            }
            count++;
        }

        REQUIRE(count == 2);// 2 different times: 0 and 10
        REQUIRE(first_time == TimeFrameIndex(0));
        REQUIRE(first_size == 2);// 2 masks at time 0
    }

    SECTION("Setting and getting image size") {
        ImageSize size{640, 480};
        mask_data.setImageSize(size);

        auto retrieved_size = mask_data.getImageSize();
        REQUIRE(retrieved_size.width == 640);
        REQUIRE(retrieved_size.height == 480);
    }

    SECTION("GetMasksInRange functionality") {
        // Setup data at multiple time points
        mask_data.addAtTime(TimeFrameIndex(5), x1, y1); // 1 mask
        mask_data.addAtTime(TimeFrameIndex(10), x1, y1);// 1 mask
        mask_data.addAtTime(TimeFrameIndex(10), x2, y2);// 2nd mask at same time
        mask_data.addAtTime(TimeFrameIndex(15), points);// 1 mask
        mask_data.addAtTime(TimeFrameIndex(20), points);// 1 mask
        mask_data.addAtTime(TimeFrameIndex(25), x1, y1);// 1 mask

        SECTION("Range includes some data") {
            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(20)};
            size_t count = 0;
            for (auto const & pair: mask_data.GetMasksInRange(interval)) {
                if (count == 0) {
                    REQUIRE(pair.time.getValue() == 10);
                    REQUIRE(pair.masks.size() == 2);// 2 masks at time 10
                } else if (count == 1) {
                    REQUIRE(pair.time.getValue() == 15);
                    REQUIRE(pair.masks.size() == 1);
                } else if (count == 2) {
                    REQUIRE(pair.time.getValue() == 20);
                    REQUIRE(pair.masks.size() == 1);
                }
                count++;
            }
            REQUIRE(count == 3);// Should include times 10, 15, 20
        }

        SECTION("Range includes all data") {
            TimeFrameInterval interval{TimeFrameIndex(0), TimeFrameIndex(30)};
            size_t count = 0;
            for (auto const & pair: mask_data.GetMasksInRange(interval)) {
                count++;
            }
            REQUIRE(count == 5);// Should include all 5 time points
        }

        SECTION("Range includes no data") {
            TimeFrameInterval interval{TimeFrameIndex(100), TimeFrameIndex(200)};
            size_t count = 0;
            for (auto const & pair: mask_data.GetMasksInRange(interval)) {
                count++;
            }
            REQUIRE(count == 0);// Should be empty
        }

        SECTION("Range with single time point") {
            TimeFrameInterval interval{TimeFrameIndex(15), TimeFrameIndex(15)};
            size_t count = 0;
            for (auto const & pair: mask_data.GetMasksInRange(interval)) {
                REQUIRE(pair.time.getValue() == 15);
                REQUIRE(pair.masks.size() == 1);
                count++;
            }
            REQUIRE(count == 1);// Should include only time 15
        }

        SECTION("Range with start > end") {
            TimeFrameInterval interval{TimeFrameIndex(20), TimeFrameIndex(10)};
            size_t count = 0;
            for (auto const & pair: mask_data.GetMasksInRange(interval)) {
                count++;
            }
            REQUIRE(count == 0);// Should be empty when start > end
        }

        SECTION("Range with timeframe conversion - same timeframes") {


            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(20)};
            size_t count = 0;
            for (auto const & pair: mask_data.GetMasksInRange(interval, *timeframe)) {
                if (count == 0) {
                    REQUIRE(pair.time.getValue() == 10);
                    REQUIRE(pair.masks.size() == 2);
                } else if (count == 1) {
                    REQUIRE(pair.time.getValue() == 15);
                    REQUIRE(pair.masks.size() == 1);
                } else if (count == 2) {
                    REQUIRE(pair.time.getValue() == 20);
                    REQUIRE(pair.masks.size() == 1);
                }
                count++;
            }
            REQUIRE(count == 3);// Should include times 10, 15, 20
        }

        SECTION("Range with timeframe conversion - different timeframes") {
            // Create a separate mask data instance for timeframe conversion test
            MaskData timeframe_test_data;

            // Create source timeframe (video frames)
            std::vector<int> video_times = {0, 10, 20, 30, 40};
            auto video_timeframe = std::make_shared<TimeFrame>(video_times);

            // Create target timeframe (data sampling)
            std::vector<int> data_times = {0, 5, 10, 15, 20, 25, 30, 35, 40};
            auto data_timeframe = std::make_shared<TimeFrame>(data_times);

            // Add data at target timeframe indices
            timeframe_test_data.addAtTime(TimeFrameIndex(2), x1, y1);// At data timeframe index 2 (time=10)
            timeframe_test_data.addAtTime(TimeFrameIndex(3), points);// At data timeframe index 3 (time=15)
            timeframe_test_data.addAtTime(TimeFrameIndex(4), x2, y2);// At data timeframe index 4 (time=20)

            timeframe_test_data.setTimeFrame(data_timeframe);

            // Query video frames 1-2 (times 10-20) which should map to data indices 2-4 (times 10-20)
            TimeFrameInterval video_interval{TimeFrameIndex(1), TimeFrameIndex(2)};
            size_t count = 0;
            for (auto const & pair: timeframe_test_data.GetMasksInRange(video_interval, *video_timeframe)) {
                if (count == 0) {
                    REQUIRE(pair.time.getValue() == 2);
                    REQUIRE(pair.masks.size() == 1);
                } else if (count == 1) {
                    REQUIRE(pair.time.getValue() == 3);
                    REQUIRE(pair.masks.size() == 1);
                } else if (count == 2) {
                    REQUIRE(pair.time.getValue() == 4);
                    REQUIRE(pair.masks.size() == 1);
                }
                count++;
            }
            REQUIRE(count == 3);// Should include converted times 2, 3, 4
        }
    }
}

TEST_CASE("MaskData - Copy and Move by EntityID", "[mask][data][entity][copy][move][by_id]") {
    // Setup test data vectors
    std::vector<uint32_t> x1 = {1, 2, 3, 1};
    std::vector<uint32_t> y1 = {1, 1, 2, 2};
    std::vector<uint32_t> x2 = {4, 5, 6, 4};
    std::vector<uint32_t> y2 = {3, 3, 4, 4};
    std::vector<Point2D<uint32_t>> points = {{10, 10}, {11, 10}, {11, 11}};

    auto data_manager = std::make_unique<DataManager>();
    auto time_frame = std::make_shared<TimeFrame>(std::vector<int>{0, 10, 20, 30});
    data_manager->setTime(TimeKey("test_time"), time_frame);

    SECTION("Copy masks by EntityID - basic functionality") {
        data_manager->setData<MaskData>("source_data", TimeKey("test_time"));
        data_manager->setData<MaskData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<MaskData>("source_data");
        auto target_data = data_manager->getData<MaskData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), x1, y1);
        source_data->addAtTime(TimeFrameIndex(10), x2, y2);
        source_data->addAtTime(TimeFrameIndex(20), points);

        auto entity_ids_10 = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(entity_ids_10.size() == 2);

        std::unordered_set<EntityId> ids_set_10c(entity_ids_10.begin(), entity_ids_10.end());
        std::size_t copied = source_data->copyByEntityIds(*target_data, ids_set_10c);
        REQUIRE(copied == 2);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(10)).size() == 2);
        auto target_entity_ids = target_data->getAllEntityIds();
        REQUIRE(target_entity_ids.size() == 2);
        REQUIRE(target_entity_ids != entity_ids_10);
    }

    SECTION("Copy masks by EntityID - mixed times") {
        data_manager->setData<MaskData>("source_data", TimeKey("test_time"));
        data_manager->setData<MaskData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<MaskData>("source_data");
        auto target_data = data_manager->getData<MaskData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), x1, y1);
        source_data->addAtTime(TimeFrameIndex(20), points);

        auto ids_10 = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto ids_20 = source_data->getEntityIdsAtTime(TimeFrameIndex(20));
        REQUIRE(ids_10.size() == 1);
        REQUIRE(ids_20.size() == 1);

        std::vector<EntityId> mixed = {ids_10[0], ids_20[0]};
        std::unordered_set<EntityId> ids_set_mixedc(mixed.begin(), mixed.end());
        std::size_t copied = source_data->copyByEntityIds(*target_data, ids_set_mixedc);
        REQUIRE(copied == 2);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(10)).size() == 1);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(20)).size() == 1);
    }

    SECTION("Copy masks by EntityID - non-existent EntityIDs") {
        data_manager->setData<MaskData>("source_data", TimeKey("test_time"));
        data_manager->setData<MaskData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<MaskData>("source_data");
        auto target_data = data_manager->getData<MaskData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), x1, y1);
        std::vector<EntityId> fake_ids = {EntityId(99999), EntityId(88888)};
        std::unordered_set<EntityId> ids_set_fakec(fake_ids.begin(), fake_ids.end());
        std::size_t copied = source_data->copyByEntityIds(*target_data, ids_set_fakec);
        REQUIRE(copied == 0);
        REQUIRE(target_data->getTimesWithData().empty());
    }

    SECTION("Move masks by EntityID - basic functionality") {
        data_manager->setData<MaskData>("source_data", TimeKey("test_time"));
        data_manager->setData<MaskData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<MaskData>("source_data");
        auto target_data = data_manager->getData<MaskData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), x1, y1);
        source_data->addAtTime(TimeFrameIndex(10), x2, y2);
        auto ids_10 = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        REQUIRE(ids_10.size() == 2);

        std::unordered_set<EntityId> const ids_set_10(ids_10.begin(), ids_10.end());
        std::size_t moved = source_data->moveByEntityIds(*target_data, ids_set_10);
        REQUIRE(moved == 2);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(10)).size() == 0);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(10)).size() == 2);
        auto target_entity_ids = target_data->getAllEntityIds();
        REQUIRE(target_entity_ids == ids_10);
    }

    SECTION("Move masks by EntityID - mixed times") {
        data_manager->setData<MaskData>("source_data", TimeKey("test_time"));
        data_manager->setData<MaskData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<MaskData>("source_data");
        auto target_data = data_manager->getData<MaskData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), x1, y1);
        source_data->addAtTime(TimeFrameIndex(20), points);

        auto ids_10 = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto ids_20 = source_data->getEntityIdsAtTime(TimeFrameIndex(20));
        std::vector<EntityId> mixed = {ids_10[0], ids_20[0]};
        std::unordered_set<EntityId> const ids_set_mixed(mixed.begin(), mixed.end());
        std::size_t moved = source_data->moveByEntityIds(*target_data, ids_set_mixed);
        REQUIRE(moved == 2);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(10)).size() == 0);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(20)).size() == 0);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(10)).size() == 1);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(20)).size() == 1);
    }

    SECTION("Move masks by EntityID - non-existent EntityIDs") {
        data_manager->setData<MaskData>("source_data", TimeKey("test_time"));
        data_manager->setData<MaskData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<MaskData>("source_data");
        auto target_data = data_manager->getData<MaskData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), x1, y1);
        std::vector<EntityId> fake_ids = {EntityId(99999), EntityId(88888)};
        std::unordered_set<EntityId> const ids_set_fake(fake_ids.begin(), fake_ids.end());
        std::size_t moved = source_data->moveByEntityIds(*target_data, ids_set_fake);
        REQUIRE(moved == 0);
        REQUIRE(target_data->getTimesWithData().empty());
        REQUIRE(source_data->getAtTime(TimeFrameIndex(10)).size() == 1);
    }
}

TEST_CASE("MaskData - Observer notification", "[mask][data][observer]") {
    MaskData mask_data;

    // Setup some test data
    std::vector<uint32_t> x1 = {1, 2, 3, 1};
    std::vector<uint32_t> y1 = {1, 1, 2, 2};

    int notification_count = 0;
    int observer_id = mask_data.addObserver([&notification_count]() {
        notification_count++;
    });

    SECTION("Notification on clearAtTime") {
        // First add a mask
        mask_data.addAtTime(TimeFrameIndex(0), x1, y1, false);// Don't notify
        REQUIRE(notification_count == 0);

        // Clear with notification
        REQUIRE(mask_data.clearAtTime(TimeIndexAndFrame(0, nullptr), NotifyObservers::Yes));
        REQUIRE(notification_count == 1);

        // Clear with notification disabled (should return false - nothing to clear)
        REQUIRE_FALSE(mask_data.clearAtTime(TimeIndexAndFrame(0, nullptr), NotifyObservers::No));
        REQUIRE(notification_count == 1);// Still 1, not incremented

        // Clear non-existent time (shouldn't notify)
        REQUIRE_FALSE(mask_data.clearAtTime(TimeIndexAndFrame(42, nullptr), NotifyObservers::No));
        REQUIRE(notification_count == 1);// Still 1, not incremented
    }

    SECTION("Notification on addAtTime") {
        // Add with notification
        mask_data.addAtTime(TimeFrameIndex(0), x1, y1);
        REQUIRE(notification_count == 1);

        // Add with notification disabled
        mask_data.addAtTime(TimeFrameIndex(0), x1, y1, false);
        REQUIRE(notification_count == 1);// Still 1, not incremented

        // Add using point vector with notification
        std::vector<Point2D<uint32_t>> points = {{1, 1}, {2, 2}};
        mask_data.addAtTime(TimeFrameIndex(1), points);
        REQUIRE(notification_count == 2);

        // Add using point vector with notification disabled
        mask_data.addAtTime(TimeFrameIndex(1), points, false);
        REQUIRE(notification_count == 2);// Still 2, not incremented
    }

    SECTION("Multiple operations with single notification") {
        // Perform multiple operations without notifying
        mask_data.addAtTime(TimeFrameIndex(0), x1, y1, false);
        mask_data.addAtTime(TimeFrameIndex(1), x1, y1, false);

        REQUIRE(notification_count == 0);

        // Now manually notify once
        mask_data.notifyObservers();
        REQUIRE(notification_count == 1);
    }
}

TEST_CASE("MaskData - Edge cases and error handling", "[mask][data][error]") {
    MaskData mask_data;

    SECTION("Getting masks at non-existent time") {
        auto masks = mask_data.getAtTime(TimeFrameIndex(999));
        REQUIRE(masks.empty());
    }

    SECTION("Adding masks with empty point vectors") {
        std::vector<uint32_t> empty_x;
        std::vector<uint32_t> empty_y;

        // This shouldn't crash
        mask_data.addAtTime(TimeFrameIndex(0), empty_x, empty_y);

        auto masks = mask_data.getAtTime(TimeFrameIndex(0));
        REQUIRE(masks.size() == 1);
        REQUIRE(masks[0].empty());
    }

    SECTION("Clearing masks at non-existent time") {
        // Should not create an entry with empty vector
        REQUIRE_FALSE(mask_data.clearAtTime(TimeIndexAndFrame(42, nullptr), NotifyObservers::No));

        auto masks = mask_data.getAtTime(TimeFrameIndex(42));
        REQUIRE(masks.empty());

        // Check that the time was NOT created
        auto range = mask_data.getAllAsRange();
        bool found = false;

        for (auto const & pair: range) {
            if (pair.time == TimeFrameIndex(42)) {
                found = true;
                break;
            }
        }

        REQUIRE_FALSE(found);
    }

    SECTION("Empty range with no data") {
        // No data added yet
        auto range = mask_data.getAllAsRange();

        // Count items in range
        size_t count = 0;
        for (auto const & pair: range) {
            count++;
        }

        REQUIRE(count == 0);
    }

    SECTION("Multiple operations sequence") {
        // Add, clear, add again to test internal state consistency
        std::vector<Point2D<uint32_t>> points = {{1, 1}, {2, 2}};

        mask_data.addAtTime(TimeFrameIndex(5), points);
        REQUIRE(mask_data.clearAtTime(TimeIndexAndFrame(5, nullptr), NotifyObservers::No));
        mask_data.addAtTime(TimeFrameIndex(5), points);

        auto masks = mask_data.getAtTime(TimeFrameIndex(5));
        REQUIRE(masks.size() == 1);
        REQUIRE(masks[0].size() == 2);
    }
}
