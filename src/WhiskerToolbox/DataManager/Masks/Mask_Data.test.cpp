#include "Masks/Mask_Data.hpp"
#include "TimeFrame/interval_data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include <catch2/catch_test_macros.hpp>

#include <vector>
#include <algorithm>

TEST_CASE("MaskData - Core functionality", "[mask][data][core]") {
    MaskData mask_data;

    // Setup some test data
    std::vector<uint32_t> x1 = {1, 2, 3, 1};
    std::vector<uint32_t> y1 = {1, 1, 2, 2};

    std::vector<uint32_t> x2 = {4, 5, 6, 4};
    std::vector<uint32_t> y2 = {3, 3, 4, 4};

    std::vector<Point2D<uint32_t>> points = {
            {10, 10},
            {11, 10},
            {11, 11},
            {10, 11}
    };

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

        mask_data.clearAtTime(TimeFrameIndex(0));

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

        for (auto const& pair : range) {
            if (count == 0) {
                first_time = pair.time;
                first_size = pair.masks.size();
            }
            count++;
        }

        REQUIRE(count == 2);  // 2 different times: 0 and 10
        REQUIRE(first_time == TimeFrameIndex(0));
        REQUIRE(first_size == 2);  // 2 masks at time 0
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
        mask_data.addAtTime(TimeFrameIndex(5), x1, y1);       // 1 mask
        mask_data.addAtTime(TimeFrameIndex(10), x1, y1);      // 1 mask  
        mask_data.addAtTime(TimeFrameIndex(10), x2, y2);      // 2nd mask at same time
        mask_data.addAtTime(TimeFrameIndex(15), points);      // 1 mask
        mask_data.addAtTime(TimeFrameIndex(20), points);      // 1 mask
        mask_data.addAtTime(TimeFrameIndex(25), x1, y1);      // 1 mask

        SECTION("Range includes some data") {
            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(20)};
            size_t count = 0;
            for (const auto& pair : mask_data.GetMasksInRange(interval)) {
                if (count == 0) {
                    REQUIRE(pair.time.getValue() == 10);
                    REQUIRE(pair.masks.size() == 2);  // 2 masks at time 10
                } else if (count == 1) {
                    REQUIRE(pair.time.getValue() == 15);
                    REQUIRE(pair.masks.size() == 1);
                } else if (count == 2) {
                    REQUIRE(pair.time.getValue() == 20);
                    REQUIRE(pair.masks.size() == 1);
                }
                count++;
            }
            REQUIRE(count == 3); // Should include times 10, 15, 20
        }

        SECTION("Range includes all data") {
            TimeFrameInterval interval{TimeFrameIndex(0), TimeFrameIndex(30)};
            size_t count = 0;
            for (const auto& pair : mask_data.GetMasksInRange(interval)) {
                count++;
            }
            REQUIRE(count == 5); // Should include all 5 time points
        }

        SECTION("Range includes no data") {
            TimeFrameInterval interval{TimeFrameIndex(100), TimeFrameIndex(200)};
            size_t count = 0;
            for (const auto& pair : mask_data.GetMasksInRange(interval)) {
                count++;
            }
            REQUIRE(count == 0); // Should be empty
        }

        SECTION("Range with single time point") {
            TimeFrameInterval interval{TimeFrameIndex(15), TimeFrameIndex(15)};
            size_t count = 0;
            for (const auto& pair : mask_data.GetMasksInRange(interval)) {
                REQUIRE(pair.time.getValue() == 15);
                REQUIRE(pair.masks.size() == 1);
                count++;
            }
            REQUIRE(count == 1); // Should include only time 15
        }

        SECTION("Range with start > end") {
            TimeFrameInterval interval{TimeFrameIndex(20), TimeFrameIndex(10)};
            size_t count = 0;
            for (const auto& pair : mask_data.GetMasksInRange(interval)) {
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
            for (const auto& pair : mask_data.GetMasksInRange(interval, timeframe, timeframe)) {
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
            REQUIRE(count == 3); // Should include times 10, 15, 20
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
            timeframe_test_data.addAtTime(TimeFrameIndex(2), x1, y1);  // At data timeframe index 2 (time=10)
            timeframe_test_data.addAtTime(TimeFrameIndex(3), points);  // At data timeframe index 3 (time=15)
            timeframe_test_data.addAtTime(TimeFrameIndex(4), x2, y2);  // At data timeframe index 4 (time=20)
            
            // Query video frames 1-2 (times 10-20) which should map to data indices 2-4 (times 10-20)
            TimeFrameInterval video_interval{TimeFrameIndex(1), TimeFrameIndex(2)};
            size_t count = 0;
            for (const auto& pair : timeframe_test_data.GetMasksInRange(video_interval, video_timeframe, data_timeframe)) {
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
            REQUIRE(count == 3); // Should include converted times 2, 3, 4
        }
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
        mask_data.addAtTime(TimeFrameIndex(0), x1, y1, false);  // Don't notify
        REQUIRE(notification_count == 0);

        // Clear with notification
        mask_data.clearAtTime(TimeFrameIndex(0));
        REQUIRE(notification_count == 1);

        // Clear with notification disabled
        mask_data.clearAtTime(TimeFrameIndex(0), false);
        REQUIRE(notification_count == 1);  // Still 1, not incremented

        // Clear non-existent time (shouldn't notify)
        mask_data.clearAtTime(TimeFrameIndex(42));
        REQUIRE(notification_count == 1);  // Still 1, not incremented
    }

    SECTION("Notification on addAtTime") {
        // Add with notification
        mask_data.addAtTime(TimeFrameIndex(0), x1, y1);
        REQUIRE(notification_count == 1);

        // Add with notification disabled
        mask_data.addAtTime(TimeFrameIndex(0), x1, y1, false);
        REQUIRE(notification_count == 1);  // Still 1, not incremented

        // Add using point vector with notification
        std::vector<Point2D<uint32_t>> points = {{1, 1}, {2, 2}};
        mask_data.addAtTime(TimeFrameIndex(1), points);
        REQUIRE(notification_count == 2);

        // Add using point vector with notification disabled
        mask_data.addAtTime(TimeFrameIndex(1), points, false);
        REQUIRE(notification_count == 2);  // Still 2, not incremented
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
        mask_data.clearAtTime(TimeFrameIndex(42));

        auto masks = mask_data.getAtTime(TimeFrameIndex(42));
        REQUIRE(masks.empty());

        // Check that the time was NOT created
        auto range = mask_data.getAllAsRange();
        bool found = false;

        for (auto const& pair : range) {
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
        for (auto const& pair : range) {
            count++;
        }

        REQUIRE(count == 0);
    }

    SECTION("Multiple operations sequence") {
        // Add, clear, add again to test internal state consistency
        std::vector<Point2D<uint32_t>> points = {{1, 1}, {2, 2}};

        mask_data.addAtTime(TimeFrameIndex(5), points);
        mask_data.clearAtTime(TimeFrameIndex(5));
        mask_data.addAtTime(TimeFrameIndex(5), points);

        auto masks = mask_data.getAtTime(TimeFrameIndex(5));
        REQUIRE(masks.size() == 1);
        REQUIRE(masks[0].size() == 2);
    }
}

TEST_CASE("MaskData - Copy and Move operations", "[mask][data][copy][move]") {
    MaskData source_data;
    MaskData target_data;

    // Setup test data
    std::vector<uint32_t> x1 = {1, 2, 3, 1};
    std::vector<uint32_t> y1 = {1, 1, 2, 2};
    
    std::vector<uint32_t> x2 = {4, 5, 6, 4};
    std::vector<uint32_t> y2 = {3, 3, 4, 4};
    
    std::vector<Point2D<uint32_t>> points1 = {{10, 10}, {11, 10}, {11, 11}};
    std::vector<Point2D<uint32_t>> points2 = {{20, 20}, {21, 20}};

    source_data.addAtTime(TimeFrameIndex(10), x1, y1);
    source_data.addAtTime(TimeFrameIndex(10), x2, y2);  // Second mask at same time
    source_data.addAtTime(TimeFrameIndex(15), points1);
    source_data.addAtTime(TimeFrameIndex(20), points2);

    SECTION("copyTo - time range operations") {
        SECTION("Copy entire range") {
            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(20)};
            std::size_t copied = source_data.copyTo(target_data, interval);
            
            REQUIRE(copied == 4); // 2 masks at time 10, 1 at time 15, 1 at time 20
            
            // Verify all masks were copied
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 1);
            
            // Verify source data is unchanged
            REQUIRE(source_data.getAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(source_data.getAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(source_data.getAtTime(TimeFrameIndex(20)).size() == 1);
        }

        SECTION("Copy partial range") {
            TimeFrameInterval interval{TimeFrameIndex(15), TimeFrameIndex(15)};
            std::size_t copied = source_data.copyTo(target_data, interval);
            
            REQUIRE(copied == 1); // Only 1 mask at time 15
            
            // Verify only masks in range were copied
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).empty());
            REQUIRE(target_data.getAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).empty());
        }

        SECTION("Copy non-existent range") {
            TimeFrameInterval interval{TimeFrameIndex(100), TimeFrameIndex(200)};
            std::size_t copied = source_data.copyTo(target_data, interval);
            
            REQUIRE(copied == 0);
            REQUIRE(target_data.getTimesWithData().empty());
        }

        SECTION("Copy with invalid range") {
            TimeFrameInterval interval{TimeFrameIndex(20), TimeFrameIndex(10)}; // start > end
            std::size_t copied = source_data.copyTo(target_data, interval);
            
            REQUIRE(copied == 0);
            REQUIRE(target_data.getTimesWithData().empty());
        }
    }

    SECTION("copyTo - specific times operations") {
        SECTION("Copy specific existing times") {
            std::vector<TimeFrameIndex> times = {TimeFrameIndex(10), TimeFrameIndex(20)};
            std::size_t copied = source_data.copyTo(target_data, times);
            
            REQUIRE(copied == 3); // 2 masks at time 10, 1 mask at time 20
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(15)).empty());
            REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 1);
        }

        SECTION("Copy mix of existing and non-existing times") {
            std::vector<TimeFrameIndex> times = {TimeFrameIndex(10), TimeFrameIndex(100), TimeFrameIndex(20), TimeFrameIndex(200)};
            std::size_t copied = source_data.copyTo(target_data, times);

            REQUIRE(copied == 3); // Only times 10 and 20 exist
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 1);
        }
    }

    SECTION("moveTo - time range operations") {
        SECTION("Move entire range") {
            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(20)};
            std::size_t moved = source_data.moveTo(target_data, interval);
            
            REQUIRE(moved == 4); // 2 + 1 + 1 = 4 masks total
            
            // Verify all masks were moved to target
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 1);
            
            // Verify source data is now empty
            REQUIRE(source_data.getTimesWithData().empty());
        }

        SECTION("Move partial range") {
            TimeFrameInterval interval{TimeFrameIndex(15), TimeFrameIndex(20)};
            std::size_t moved = source_data.moveTo(target_data, interval);
            
            REQUIRE(moved == 2); // 1 + 1 = 2 masks
            
            // Verify only masks in range were moved
            REQUIRE(target_data.getAtTime(TimeFrameIndex(15)).size() == 1);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 1);
            
            // Verify source still has data outside the range
            REQUIRE(source_data.getAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(source_data.getAtTime(TimeFrameIndex(15)).empty());
            REQUIRE(source_data.getAtTime(TimeFrameIndex(20)).empty());
        }

        SECTION("Move non-existent range") {
            TimeFrameInterval interval{TimeFrameIndex(100), TimeFrameIndex(200)};
            std::size_t moved = source_data.moveTo(target_data, interval);
            
            REQUIRE(moved == 0);
            REQUIRE(target_data.getTimesWithData().empty());
            
            // Source should be unchanged
            REQUIRE(source_data.getTimesWithData().size() == 3);
        }
    }

    SECTION("moveTo - specific times operations") {
        SECTION("Move specific existing times") {
            std::vector<TimeFrameIndex> times = {TimeFrameIndex(15), TimeFrameIndex(10)}; // Non-sequential order
            std::size_t moved = source_data.moveTo(target_data, times);
            
            REQUIRE(moved == 3); // 1 + 2 = 3 masks
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(15)).size() == 1);
            
            // Only time 20 should remain in source
            REQUIRE(source_data.getTimesWithData().size() == 1);
            REQUIRE(source_data.getAtTime(TimeFrameIndex(20)).size() == 1);
        }

        SECTION("Move mix of existing and non-existing times") {
            std::vector<TimeFrameIndex> times = {TimeFrameIndex(20), TimeFrameIndex(100), TimeFrameIndex(10), TimeFrameIndex(200)};
            std::size_t moved = source_data.moveTo(target_data, times);
            
            REQUIRE(moved == 3); // Only times 10 and 20 exist
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 2);
            REQUIRE(target_data.getAtTime(TimeFrameIndex(20)).size() == 1);
            
            // Only time 15 should remain in source
            REQUIRE(source_data.getTimesWithData().size() == 1);
            REQUIRE(source_data.getAtTime(TimeFrameIndex(15)).size() == 1);
        }
    }

    SECTION("Copy/Move to target with existing data") {
        // Add some existing data to target
        std::vector<Point2D<uint32_t>> existing_mask = {{100, 200}};
        target_data.addAtTime(TimeFrameIndex(10), existing_mask);

        SECTION("Copy to existing time adds masks") {
            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(10)};
            std::size_t copied = source_data.copyTo(target_data, interval);
            
            REQUIRE(copied == 2);
            // Should have existing mask plus copied masks
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 3); // 1 existing + 2 copied
        }

        SECTION("Move to existing time adds masks") {
            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(10)};
            std::size_t moved = source_data.moveTo(target_data, interval);
            
            REQUIRE(moved == 2);
            // Should have existing mask plus moved masks
            REQUIRE(target_data.getAtTime(TimeFrameIndex(10)).size() == 3); // 1 existing + 2 moved
            // Source should no longer have data at time 10
            REQUIRE(source_data.getAtTime(TimeFrameIndex(10)).empty());
        }
    }

    SECTION("Edge cases") {
        MaskData empty_source;

        SECTION("Copy from empty source") {
            TimeFrameInterval interval{TimeFrameIndex(0), TimeFrameIndex(100)};
            std::size_t copied = empty_source.copyTo(target_data, interval);
            REQUIRE(copied == 0);
            REQUIRE(target_data.getTimesWithData().empty());
        }

        SECTION("Move from empty source") {
            TimeFrameInterval interval{TimeFrameIndex(0), TimeFrameIndex(100)};
            std::size_t moved = empty_source.moveTo(target_data, interval);
            REQUIRE(moved == 0);
            REQUIRE(target_data.getTimesWithData().empty());
        }

        SECTION("Copy to self (same object)") {
            // This is a corner case - copying to self should return 0 and not
            // modify the data
            TimeFrameInterval interval{TimeFrameIndex(10), TimeFrameIndex(10)};
            std::size_t copied = source_data.copyTo(source_data, interval);
            REQUIRE(copied == 0);
            // Should now have doubled the masks at time 10
            REQUIRE(source_data.getAtTime(TimeFrameIndex(10)).size() == 2); // 2 original
        }

        SECTION("Observer notification control") {
            MaskData test_source;
            MaskData test_target;
            
            test_source.addAtTime(TimeFrameIndex(5), points1);
            
            int target_notifications = 0;
            test_target.addObserver([&target_notifications]() {
                target_notifications++;
            });
            
            // Copy without notification
            TimeFrameInterval interval{TimeFrameIndex(5), TimeFrameIndex(5)};
            std::size_t copied = test_source.copyTo(test_target, interval, false);
            REQUIRE(copied == 1);
            REQUIRE(target_notifications == 0);
            
            // Copy with notification
            copied = test_source.copyTo(test_target, interval, true);
            REQUIRE(copied == 1);
            REQUIRE(target_notifications == 1);
        }
    }
}
