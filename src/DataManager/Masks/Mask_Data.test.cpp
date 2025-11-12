#include "Masks/Mask_Data.hpp"
#include "DataManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "fixtures/entity_id.hpp"
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

    Mask2D points = {
            {10, 10},
            {11, 10},
            {11, 11},
            {10, 11}};

    SECTION("Adding masks at time") {
        // Add first mask at time 0
        mask_data.addAtTime(TimeFrameIndex(0), x1, y1, NotifyObservers::No);

        auto masks_at_0 = mask_data.getAtTime(TimeFrameIndex(0));
        REQUIRE(masks_at_0.size() == 1);
        REQUIRE(masks_at_0[0].size() == 4);
        REQUIRE(masks_at_0[0][0].x == 1);
        REQUIRE(masks_at_0[0][0].y == 1);

        // Add second mask at time 0
        mask_data.addAtTime(TimeFrameIndex(0), x2, y2, NotifyObservers::No);
        masks_at_0 = mask_data.getAtTime(TimeFrameIndex(0));
        REQUIRE(masks_at_0.size() == 2);
        REQUIRE(masks_at_0[1].size() == 4);
        REQUIRE(masks_at_0[1][0].x == 4);

        // Add mask at new time 10
        mask_data.addAtTime(TimeFrameIndex(10), points, NotifyObservers::No);
        auto masks_at_10 = mask_data.getAtTime(TimeFrameIndex(10));
        REQUIRE(masks_at_10.size() == 1);
        REQUIRE(masks_at_10[0].size() == 4);
        REQUIRE(masks_at_10[0][0].x == 10);
    }

    SECTION("Clearing masks at time") {
        // Add masks and then clear them
        mask_data.addAtTime(TimeFrameIndex(0), x1, y1, NotifyObservers::No);
        mask_data.addAtTime(TimeFrameIndex(0), x2, y2, NotifyObservers::No);
        mask_data.addAtTime(TimeFrameIndex(10), points, NotifyObservers::No);

        static_cast<void>(mask_data.clearAtTime(TimeIndexAndFrame(0, timeframe.get()),
                                                NotifyObservers::No));

        auto masks_at_0 = mask_data.getAtTime(TimeFrameIndex(0));
        auto masks_at_10 = mask_data.getAtTime(TimeFrameIndex(10));

        REQUIRE(masks_at_0.empty());
        REQUIRE(masks_at_10.size() == 1);
    }

    SECTION("Setting and getting image size") {
        ImageSize size{640, 480};
        mask_data.setImageSize(size);

        auto retrieved_size = mask_data.getImageSize();
        REQUIRE(retrieved_size.width == 640);
        REQUIRE(retrieved_size.height == 480);
    }
}

TEST_CASE("MaskData - Copy and Move by EntityID", "[mask][data][entity][copy][move][by_id]") {
    // Setup test data vectors
    std::vector<uint32_t> x1 = {1, 2, 3, 1};
    std::vector<uint32_t> y1 = {1, 1, 2, 2};
    std::vector<uint32_t> x2 = {4, 5, 6, 4};
    std::vector<uint32_t> y2 = {3, 3, 4, 4};
    Mask2D points = {{10, 10}, {11, 10}, {11, 11}};

    auto data_manager = std::make_unique<DataManager>();
    auto time_frame = std::make_shared<TimeFrame>(std::vector<int>{0, 10, 20, 30});
    data_manager->setTime(TimeKey("test_time"), time_frame);

    SECTION("Copy masks by EntityID - basic functionality") {
        data_manager->setData<MaskData>("source_data", TimeKey("test_time"));
        data_manager->setData<MaskData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<MaskData>("source_data");
        auto target_data = data_manager->getData<MaskData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), x1, y1, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(10), x2, y2, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(20), points, NotifyObservers::No);

        auto entity_ids_10_view = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        std::vector<EntityId> entity_ids_10(entity_ids_10_view.begin(), entity_ids_10_view.end());
        REQUIRE(entity_ids_10.size() == 2);

        std::unordered_set<EntityId> ids_set_10c(entity_ids_10.begin(), entity_ids_10.end());
        std::size_t copied = source_data->copyByEntityIds(*target_data, ids_set_10c, NotifyObservers::No);
        REQUIRE(copied == 2);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(10)).size() == 2);
        auto target_entity_ids = get_all_entity_ids(*target_data);
        REQUIRE(target_entity_ids.size() == 2);
        REQUIRE(target_entity_ids != entity_ids_10);
    }

    SECTION("Copy masks by EntityID - mixed times") {
        data_manager->setData<MaskData>("source_data", TimeKey("test_time"));
        data_manager->setData<MaskData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<MaskData>("source_data");
        auto target_data = data_manager->getData<MaskData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), x1, y1, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(20), points, NotifyObservers::No);

        auto ids_10 = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto ids_20 = source_data->getEntityIdsAtTime(TimeFrameIndex(20));
        REQUIRE(ids_10.size() == 1);
        REQUIRE(ids_20.size() == 1);

        std::vector<EntityId> mixed = {ids_10[0], ids_20[0]};
        std::unordered_set<EntityId> ids_set_mixedc(mixed.begin(), mixed.end());
        std::size_t copied = source_data->copyByEntityIds(*target_data, ids_set_mixedc, NotifyObservers::No);
        REQUIRE(copied == 2);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(10)).size() == 1);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(20)).size() == 1);
    }

    SECTION("Copy masks by EntityID - non-existent EntityIDs") {
        data_manager->setData<MaskData>("source_data", TimeKey("test_time"));
        data_manager->setData<MaskData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<MaskData>("source_data");
        auto target_data = data_manager->getData<MaskData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), x1, y1, NotifyObservers::No);
        std::vector<EntityId> fake_ids = {EntityId(99999), EntityId(88888)};
        std::unordered_set<EntityId> ids_set_fakec(fake_ids.begin(), fake_ids.end());
        std::size_t copied = source_data->copyByEntityIds(*target_data, ids_set_fakec, NotifyObservers::No);
        REQUIRE(copied == 0);
        REQUIRE(target_data->getTimesWithData().empty());
    }

    SECTION("Move masks by EntityID - basic functionality") {
        data_manager->setData<MaskData>("source_data", TimeKey("test_time"));
        data_manager->setData<MaskData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<MaskData>("source_data");
        auto target_data = data_manager->getData<MaskData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), x1, y1, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(10), x2, y2, NotifyObservers::No);
        auto ids_10_view = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        std::vector<EntityId> ids_10(ids_10_view.begin(), ids_10_view.end());
        REQUIRE(ids_10.size() == 2);

        std::unordered_set<EntityId> const ids_set_10(ids_10.begin(), ids_10.end());
        std::size_t moved = source_data->moveByEntityIds(*target_data, ids_set_10, NotifyObservers::No);
        REQUIRE(moved == 2);
        REQUIRE(source_data->getAtTime(TimeFrameIndex(10)).size() == 0);
        REQUIRE(target_data->getAtTime(TimeFrameIndex(10)).size() == 2);
        auto target_entity_ids = get_all_entity_ids(*target_data);
        REQUIRE(target_entity_ids == ids_10);
    }

    SECTION("Move masks by EntityID - mixed times") {
        data_manager->setData<MaskData>("source_data", TimeKey("test_time"));
        data_manager->setData<MaskData>("target_data", TimeKey("test_time"));

        auto source_data = data_manager->getData<MaskData>("source_data");
        auto target_data = data_manager->getData<MaskData>("target_data");

        source_data->addAtTime(TimeFrameIndex(10), x1, y1, NotifyObservers::No);
        source_data->addAtTime(TimeFrameIndex(20), points, NotifyObservers::No);

        auto ids_10 = source_data->getEntityIdsAtTime(TimeFrameIndex(10));
        auto ids_20 = source_data->getEntityIdsAtTime(TimeFrameIndex(20));
        std::vector<EntityId> mixed = {ids_10[0], ids_20[0]};
        std::unordered_set<EntityId> const ids_set_mixed(mixed.begin(), mixed.end());
        std::size_t moved = source_data->moveByEntityIds(*target_data, ids_set_mixed, NotifyObservers::No);
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

        source_data->addAtTime(TimeFrameIndex(10), x1, y1, NotifyObservers::No);
        std::vector<EntityId> fake_ids = {EntityId(99999), EntityId(88888)};
        std::unordered_set<EntityId> const ids_set_fake(fake_ids.begin(), fake_ids.end());
        std::size_t moved = source_data->moveByEntityIds(*target_data, ids_set_fake, NotifyObservers::No);
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
        mask_data.addAtTime(TimeFrameIndex(0), x1, y1, NotifyObservers::No);// Don't notify
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
        mask_data.addAtTime(TimeFrameIndex(0), x1, y1, NotifyObservers::Yes);
        REQUIRE(notification_count == 1);

        // Add with notification disabled
        mask_data.addAtTime(TimeFrameIndex(0), x1, y1, NotifyObservers::No);
        REQUIRE(notification_count == 1);// Still 1, not incremented

        // Add using point vector with notification
        Mask2D points = {{1, 1}, {2, 2}};
        mask_data.addAtTime(TimeFrameIndex(1), points, NotifyObservers::Yes);
        REQUIRE(notification_count == 2);

        // Add using point vector with notification disabled
        mask_data.addAtTime(TimeFrameIndex(1), points, NotifyObservers::No);
        REQUIRE(notification_count == 2);// Still 2, not incremented
    }

    SECTION("Multiple operations with single notification") {
        // Perform multiple operations without notifying
        mask_data.addAtTime(TimeFrameIndex(0), x1, y1, NotifyObservers::No);
        mask_data.addAtTime(TimeFrameIndex(1), x1, y1, NotifyObservers::No);

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
        mask_data.addAtTime(TimeFrameIndex(0), empty_x, empty_y, NotifyObservers::No);

        auto masks = mask_data.getAtTime(TimeFrameIndex(0));
        REQUIRE(masks.size() == 1);
        REQUIRE(masks[0].empty());
    }

    SECTION("Multiple operations sequence") {
        // Add, clear, add again to test internal state consistency
        Mask2D points = {{1, 1}, {2, 2}};

        mask_data.addAtTime(TimeFrameIndex(5), points, NotifyObservers::No);
        REQUIRE(mask_data.clearAtTime(TimeIndexAndFrame(5, nullptr), NotifyObservers::No));
        mask_data.addAtTime(TimeFrameIndex(5), points, NotifyObservers::No);

        auto masks = mask_data.getAtTime(TimeFrameIndex(5));
        REQUIRE(masks.size() == 1);
        REQUIRE(masks[0].size() == 2);
    }
}
