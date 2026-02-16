#include "Masks/Mask_Data.hpp"
#include "DataManager.hpp"
#include "Entity/EntityRegistry.hpp"
#include "fixtures/entity_id.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"
#include "fixtures/RaggedTimeSeriesTestTraits.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

TEST_CASE("MaskData - Core functionality", "[mask][data][core]") {
    MaskData mask_data;

    // Test with same source and target timeframes
    std::vector<int> times = {5, 10, 15, 20, 25};
    auto timeframe = std::make_shared<TimeFrame>(times);

    mask_data.setTimeFrame(timeframe);

    using Traits = RaggedDataTestTraits<MaskData>;
    auto item1 = Traits::createSingleItem();
    auto item2 = Traits::createAnotherItem();

    SECTION("Adding masks at time") {
        // Add first mask at time 0
        mask_data.addAtTime(TimeFrameIndex(0), item1, NotifyObservers::No);

        auto masks_at_0 = mask_data.getAtTime(TimeFrameIndex(0));
        REQUIRE(masks_at_0.size() == 1);

        // Add second mask at time 0
        mask_data.addAtTime(TimeFrameIndex(0), item2, NotifyObservers::No);
        masks_at_0 = mask_data.getAtTime(TimeFrameIndex(0));
        REQUIRE(masks_at_0.size() == 2);

        // Add mask at new time 10
        mask_data.addAtTime(TimeFrameIndex(10), item1, NotifyObservers::No);
        auto masks_at_10 = mask_data.getAtTime(TimeFrameIndex(10));
        REQUIRE(masks_at_10.size() == 1);
    }

    SECTION("Clearing masks at time") {
        // Add masks and then clear them
        mask_data.addAtTime(TimeFrameIndex(0), item1, NotifyObservers::No);
        mask_data.addAtTime(TimeFrameIndex(0), item2, NotifyObservers::No);
        mask_data.addAtTime(TimeFrameIndex(10), item1, NotifyObservers::No);

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

TEST_CASE("MaskData - Observer notification", "[mask][data][observer]") {
    MaskData mask_data;
    using Traits = RaggedDataTestTraits<MaskData>;
    auto item = Traits::createSingleItem();

    int notification_count = 0;
    int observer_id = mask_data.addObserver([&notification_count]() {
        notification_count++;
    });

    SECTION("Notification on clearAtTime") {
        // First add a mask
        mask_data.addAtTime(TimeFrameIndex(0), item, NotifyObservers::No);// Don't notify
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
        mask_data.addAtTime(TimeFrameIndex(0), item, NotifyObservers::Yes);
        REQUIRE(notification_count == 1);

        // Add with notification disabled
        mask_data.addAtTime(TimeFrameIndex(0), item, NotifyObservers::No);
        REQUIRE(notification_count == 1);// Still 1, not incremented
    }

    SECTION("Multiple operations with single notification") {
        // Perform multiple operations without notifying
        mask_data.addAtTime(TimeFrameIndex(0), item, NotifyObservers::No);
        mask_data.addAtTime(TimeFrameIndex(1), item, NotifyObservers::No);

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
        // Manually create empty mask
        Mask2D empty_mask(std::vector<uint32_t>{}, std::vector<uint32_t>{});

        // This shouldn't crash
        mask_data.addAtTime(TimeFrameIndex(0), empty_mask, NotifyObservers::No);

        auto masks = mask_data.getAtTime(TimeFrameIndex(0));
        REQUIRE(masks.size() == 1);
        // Accessing the first element from the view
        REQUIRE((*masks.begin()).empty());
    }

    SECTION("Multiple operations sequence") {
        using Traits = RaggedDataTestTraits<MaskData>;
        auto item = Traits::createSingleItem();

        mask_data.addAtTime(TimeFrameIndex(5), item, NotifyObservers::No);
        REQUIRE(mask_data.clearAtTime(TimeIndexAndFrame(5, nullptr), NotifyObservers::No));
        mask_data.addAtTime(TimeFrameIndex(5), item, NotifyObservers::No);

        auto masks = mask_data.getAtTime(TimeFrameIndex(5));
        REQUIRE(masks.size() == 1);
    }
}
