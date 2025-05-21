#include "Masks/Mask_Data.hpp"
#include <catch2/catch_test_macros.hpp>

#include <vector>
#include <algorithm>

TEST_CASE("MaskData - Core functionality", "[mask][data][core]") {
    MaskData mask_data;

    // Setup some test data
    std::vector<float> x1 = {1.0f, 2.0f, 3.0f, 1.0f};
    std::vector<float> y1 = {1.0f, 1.0f, 2.0f, 2.0f};

    std::vector<float> x2 = {4.0f, 5.0f, 6.0f, 4.0f};
    std::vector<float> y2 = {3.0f, 3.0f, 4.0f, 4.0f};

    std::vector<Point2D<float>> points = {
            {10.0f, 10.0f},
            {11.0f, 10.0f},
            {11.0f, 11.0f},
            {10.0f, 11.0f}
    };

    SECTION("Adding masks at time") {
        // Add first mask at time 0
        mask_data.addAtTime(0, x1, y1);

        auto masks_at_0 = mask_data.getAtTime(0);
        REQUIRE(masks_at_0.size() == 1);
        REQUIRE(masks_at_0[0].size() == 4);
        REQUIRE(masks_at_0[0][0].x == 1.0f);
        REQUIRE(masks_at_0[0][0].y == 1.0f);

        // Add second mask at time 0
        mask_data.addAtTime(0, x2, y2);
        masks_at_0 = mask_data.getAtTime(0);
        REQUIRE(masks_at_0.size() == 2);
        REQUIRE(masks_at_0[1].size() == 4);
        REQUIRE(masks_at_0[1][0].x == 4.0f);

        // Add mask at new time 10
        mask_data.addAtTime(10, points);
        auto masks_at_10 = mask_data.getAtTime(10);
        REQUIRE(masks_at_10.size() == 1);
        REQUIRE(masks_at_10[0].size() == 4);
        REQUIRE(masks_at_10[0][0].x == 10.0f);
    }

    SECTION("Clearing masks at time") {
        // Add masks and then clear them
        mask_data.addAtTime(0, x1, y1);
        mask_data.addAtTime(0, x2, y2);
        mask_data.addAtTime(10, points);

        mask_data.clearAtTime(0);

        auto masks_at_0 = mask_data.getAtTime(0);
        auto masks_at_10 = mask_data.getAtTime(10);

        REQUIRE(masks_at_0.empty());
        REQUIRE(masks_at_10.size() == 1);
    }

    SECTION("Clearing masks at non-existent time") {
        // Should not create an entry with empty vector
        mask_data.clearAtTime(42);

        auto masks = mask_data.getAtTime(42);
        REQUIRE(masks.empty());

        // Check that the time was NOT created
        auto range = mask_data.getAllAsRange();
        bool found = false;

        for (auto const& pair : range) {
            if (pair.time == 42) {
                found = true;
                break;
            }
        }

        REQUIRE_FALSE(found);
    }

    SECTION("Getting masks as range") {
        // Add multiple masks at different times
        mask_data.addAtTime(0, x1, y1);
        mask_data.addAtTime(0, x2, y2);
        mask_data.addAtTime(10, points);

        auto range = mask_data.getAllAsRange();

        // Count items in range
        size_t count = 0;
        int first_time = -1;
        size_t first_size = 0;

        for (auto const& pair : range) {
            if (count == 0) {
                first_time = pair.time;
                first_size = pair.masks.size();
            }
            count++;
        }

        REQUIRE(count == 2);  // 2 different times: 0 and 10
        REQUIRE(first_time == 0);
        REQUIRE(first_size == 2);  // 2 masks at time 0
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

    // Setup some test data
    std::vector<float> x1 = {1.0f, 2.0f, 3.0f, 1.0f};
    std::vector<float> y1 = {1.0f, 1.0f, 2.0f, 2.0f};

    int notification_count = 0;
    int observer_id = mask_data.addObserver([&notification_count]() {
        notification_count++;
    });

    SECTION("Notification on clearAtTime") {
        // First add a mask
        mask_data.addAtTime(0, x1, y1, false);  // Don't notify
        REQUIRE(notification_count == 0);

        // Clear with notification
        mask_data.clearAtTime(0);
        REQUIRE(notification_count == 1);

        // Clear with notification disabled
        mask_data.clearAtTime(0, false);
        REQUIRE(notification_count == 1);  // Still 1, not incremented

        // Clear non-existent time (shouldn't notify)
        mask_data.clearAtTime(42);
        REQUIRE(notification_count == 1);  // Still 1, not incremented
    }

    SECTION("Notification on addAtTime") {
        // Add with notification
        mask_data.addAtTime(0, x1, y1);
        REQUIRE(notification_count == 1);

        // Add with notification disabled
        mask_data.addAtTime(0, x1, y1, false);
        REQUIRE(notification_count == 1);  // Still 1, not incremented

        // Add using point vector with notification
        std::vector<Point2D<float>> points = {{1.0f, 1.0f}, {2.0f, 2.0f}};
        mask_data.addAtTime(1, points);
        REQUIRE(notification_count == 2);

        // Add using point vector with notification disabled
        mask_data.addAtTime(1, points, false);
        REQUIRE(notification_count == 2);  // Still 2, not incremented
    }

    SECTION("Multiple operations with single notification") {
        // Perform multiple operations without notifying
        mask_data.addAtTime(0, x1, y1, false);
        mask_data.addAtTime(1, x1, y1, false);

        REQUIRE(notification_count == 0);

        // Now manually notify once
        mask_data.notifyObservers();
        REQUIRE(notification_count == 1);
    }
}

TEST_CASE("MaskData - Edge cases and error handling", "[mask][data][error]") {
    MaskData mask_data;

    SECTION("Getting masks at non-existent time") {
        auto masks = mask_data.getAtTime(999);
        REQUIRE(masks.empty());
    }

    SECTION("Adding masks with empty point vectors") {
        std::vector<float> empty_x;
        std::vector<float> empty_y;

        // This shouldn't crash
        mask_data.addAtTime(0, empty_x, empty_y);

        auto masks = mask_data.getAtTime(0);
        REQUIRE(masks.size() == 1);
        REQUIRE(masks[0].empty());
    }

    SECTION("Clearing masks at non-existent time") {
        // Should create an entry with empty vector
        mask_data.clearAtTime(42);

        auto masks = mask_data.getAtTime(42);
        REQUIRE(masks.empty());

        // Check that the time was actually created
        auto range = mask_data.getAllAsRange();
        bool found = false;

        for (auto const& pair : range) {
            if (pair.time == 42) {
                found = true;
                break;
            }
        }

        REQUIRE(found);
    }

    SECTION("Empty range with no data") {
        // No data added yet
        auto range = mask_data.getAllAsRange();

        // Count items in range
        size_t count = 0;
        for (auto const& pair : range) {
            count++;
        }

        REQUIRE(count == 0);
    }

    SECTION("Multiple operations sequence") {
        // Add, clear, add again to test internal state consistency
        std::vector<Point2D<float>> points = {{1.0f, 1.0f}, {2.0f, 2.0f}};

        mask_data.addAtTime(5, points);
        mask_data.clearAtTime(5);
        mask_data.addAtTime(5, points);

        auto masks = mask_data.getAtTime(5);
        REQUIRE(masks.size() == 1);
        REQUIRE(masks[0].size() == 2);
    }
}
