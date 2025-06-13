#include "TimeFrameV2.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("DenseTimeRange - Basic functionality", "[timeframev2][dense]") {
    SECTION("Construction and basic operations") {
        DenseTimeRange range(1000, 100, 10); // Start=1000, Count=100, Step=10
        
        REQUIRE(range.size() == 100);
        REQUIRE(range.getTimeAtIndex(TimeFrameIndex(0)) == 1000);
        REQUIRE(range.getTimeAtIndex(TimeFrameIndex(1)) == 1010);
        REQUIRE(range.getTimeAtIndex(TimeFrameIndex(99)) == 1990);
    }
    
    SECTION("Index-to-time conversion") {
        DenseTimeRange range(0, 1000, 1); // Simple 0-999 range
        
        REQUIRE(range.getTimeAtIndex(TimeFrameIndex(0)) == 0);
        REQUIRE(range.getTimeAtIndex(TimeFrameIndex(500)) == 500);
        REQUIRE(range.getTimeAtIndex(TimeFrameIndex(999)) == 999);
        
        // Out of bounds should return start value
        REQUIRE(range.getTimeAtIndex(TimeFrameIndex(-1)) == 0);
        REQUIRE(range.getTimeAtIndex(TimeFrameIndex(1000)) == 0);
    }
    
    SECTION("Time-to-index conversion") {
        DenseTimeRange range(100, 50, 2); // 100, 102, 104, ..., 198
        
        REQUIRE(range.getIndexAtTime(100.0) == TimeFrameIndex(0));
        REQUIRE(range.getIndexAtTime(110.0) == TimeFrameIndex(5));
        REQUIRE(range.getIndexAtTime(198.0) == TimeFrameIndex(49));
        
        // Test clamping behavior
        REQUIRE(range.getIndexAtTime(50.0) == TimeFrameIndex(0));   // Before start
        REQUIRE(range.getIndexAtTime(300.0) == TimeFrameIndex(49)); // After end
        
        // Test closest value selection
        REQUIRE(range.getIndexAtTime(101.0) == TimeFrameIndex(0)); // Closer to 100
        REQUIRE(range.getIndexAtTime(103.0) == TimeFrameIndex(1)); // Closer to 102
    }
    
    SECTION("Empty range") {
        DenseTimeRange empty_range(0, 0, 1);
        
        REQUIRE(empty_range.size() == 0);
        REQUIRE(empty_range.getIndexAtTime(100.0) == TimeFrameIndex(0));
    }
}

TEST_CASE("TimeFrameV2 - Dense storage with ClockTicks", "[timeframev2][clockticks][dense]") {
    SECTION("Construction and basic operations") {
        // Create a 30kHz sampling clock starting at tick 1000
        ClockTimeFrame clock_frame(1000, 30000, 1, 30000.0);
        
        REQUIRE(clock_frame.getTotalFrameCount() == 30000);
        REQUIRE(clock_frame.isDense());
        REQUIRE_FALSE(clock_frame.isSparse());
        REQUIRE(clock_frame.getSamplingRate().has_value());
        REQUIRE_THAT(clock_frame.getSamplingRate().value(), Catch::Matchers::WithinRel(30000.0, 1e-9));
    }
    
    SECTION("Time coordinate operations") {
        ClockTimeFrame clock_frame(0, 1000, 1, 30000.0);
        
        // Test getTimeAtIndex
        ClockTicks first_tick = clock_frame.getTimeAtIndex(TimeFrameIndex(0));
        REQUIRE(first_tick.getValue() == 0);
        
        ClockTicks middle_tick = clock_frame.getTimeAtIndex(TimeFrameIndex(500));
        REQUIRE(middle_tick.getValue() == 500);
        
        ClockTicks last_tick = clock_frame.getTimeAtIndex(TimeFrameIndex(999));
        REQUIRE(last_tick.getValue() == 999);
        
        // Test getIndexAtTime
        REQUIRE(clock_frame.getIndexAtTime(ClockTicks(0)) == TimeFrameIndex(0));
        REQUIRE(clock_frame.getIndexAtTime(ClockTicks(500)) == TimeFrameIndex(500));
        REQUIRE(clock_frame.getIndexAtTime(ClockTicks(999)) == TimeFrameIndex(999));
    }
    
    SECTION("Conversion to seconds") {
        ClockTimeFrame clock_frame(0, 30000, 1, 30000.0); // 1 second of data
        
        ClockTicks half_second_tick(15000);
        auto seconds_opt = clock_frame.coordinateToSeconds(half_second_tick);
        
        REQUIRE(seconds_opt.has_value());
        REQUIRE_THAT(seconds_opt.value().getValue(), Catch::Matchers::WithinRel(0.5, 1e-9));
        
        // Convert back to ticks
        auto ticks_opt = clock_frame.secondsToCoordinate(Seconds(0.5));
        REQUIRE(ticks_opt.has_value());
        REQUIRE(ticks_opt.value().getValue() == 15000);
    }
    
    SECTION("Bounds checking") {
        ClockTimeFrame clock_frame(100, 50, 1, 30000.0);
        
        REQUIRE(clock_frame.checkIndexInBounds(TimeFrameIndex(-10)) == TimeFrameIndex(0));
        REQUIRE(clock_frame.checkIndexInBounds(TimeFrameIndex(25)) == TimeFrameIndex(25));
        REQUIRE(clock_frame.checkIndexInBounds(TimeFrameIndex(100)) == TimeFrameIndex(49));
    }
}

TEST_CASE("TimeFrameV2 - Sparse storage with CameraFrameIndex", "[timeframev2][cameraframe][sparse]") {
    SECTION("Construction and basic operations") {
        std::vector<int64_t> frame_times = {100, 350, 600, 1200, 1500};
        CameraTimeFrame camera_frame(frame_times);
        
        REQUIRE(camera_frame.getTotalFrameCount() == 5);
        REQUIRE(camera_frame.isSparse());
        REQUIRE_FALSE(camera_frame.isDense());
        REQUIRE_FALSE(camera_frame.getSamplingRate().has_value());
    }
    
    SECTION("Time coordinate operations") {
        std::vector<int64_t> frame_times = {10, 25, 40, 80, 120};
        CameraTimeFrame camera_frame(frame_times);
        
        // Test getTimeAtIndex
        CameraFrameIndex first_frame = camera_frame.getTimeAtIndex(TimeFrameIndex(0));
        REQUIRE(first_frame.getValue() == 10);
        
        CameraFrameIndex middle_frame = camera_frame.getTimeAtIndex(TimeFrameIndex(2));
        REQUIRE(middle_frame.getValue() == 40);
        
        CameraFrameIndex last_frame = camera_frame.getTimeAtIndex(TimeFrameIndex(4));
        REQUIRE(last_frame.getValue() == 120);
        
        // Test getIndexAtTime
        REQUIRE(camera_frame.getIndexAtTime(CameraFrameIndex(10)) == TimeFrameIndex(0));
        REQUIRE(camera_frame.getIndexAtTime(CameraFrameIndex(40)) == TimeFrameIndex(2));
        REQUIRE(camera_frame.getIndexAtTime(CameraFrameIndex(120)) == TimeFrameIndex(4));
        
        // Test closest value selection
        REQUIRE(camera_frame.getIndexAtTime(CameraFrameIndex(15)) == TimeFrameIndex(0)); // Closer to 10
        REQUIRE(camera_frame.getIndexAtTime(CameraFrameIndex(20)) == TimeFrameIndex(1)); // Closer to 25
    }
    
    SECTION("Out of bounds behavior") {
        std::vector<int64_t> frame_times = {100, 200, 300};
        CameraTimeFrame camera_frame(frame_times);
        
        // Out of bounds index should return zero coordinate
        CameraFrameIndex out_of_bounds = camera_frame.getTimeAtIndex(TimeFrameIndex(-1));
        REQUIRE(out_of_bounds.getValue() == 0);
        
        out_of_bounds = camera_frame.getTimeAtIndex(TimeFrameIndex(10));
        REQUIRE(out_of_bounds.getValue() == 0);
        
        // Out of bounds time should clamp to valid range
        REQUIRE(camera_frame.getIndexAtTime(CameraFrameIndex(50)) == TimeFrameIndex(0));   // Before first
        REQUIRE(camera_frame.getIndexAtTime(CameraFrameIndex(500)) == TimeFrameIndex(2));  // After last
    }
}

TEST_CASE("TimeFrameV2 - Seconds coordinate type", "[timeframev2][seconds]") {
    SECTION("Dense storage with seconds") {
        // 10 seconds of data at 0.1 second intervals
        SecondsTimeFrame seconds_frame(0, 100, 1);
        
        REQUIRE(seconds_frame.getTotalFrameCount() == 100);
        REQUIRE(seconds_frame.isDense());
        
        Seconds first_time = seconds_frame.getTimeAtIndex(TimeFrameIndex(0));
        REQUIRE_THAT(first_time.getValue(), Catch::Matchers::WithinRel(0.0, 1e-9));
        
        Seconds last_time = seconds_frame.getTimeAtIndex(TimeFrameIndex(99));
        REQUIRE_THAT(last_time.getValue(), Catch::Matchers::WithinRel(99.0, 1e-9));
    }
    
    SECTION("Sparse storage with seconds") {
        std::vector<int64_t> time_values = {0, 5, 12, 25, 50}; // Note: stored as int64_t internally
        SecondsTimeFrame seconds_frame(time_values);
        
        REQUIRE(seconds_frame.getTotalFrameCount() == 5);
        REQUIRE(seconds_frame.isSparse());
        
        Seconds middle_time = seconds_frame.getTimeAtIndex(TimeFrameIndex(2));
        REQUIRE_THAT(middle_time.getValue(), Catch::Matchers::WithinRel(12.0, 1e-9));
    }
}

TEST_CASE("TimeFrameV2 - UncalibratedIndex operations", "[timeframev2][uncalibrated]") {
    SECTION("Dense uncalibrated storage") {
        UncalibratedTimeFrame uncalib_frame(1000, 500, 2);
        
        REQUIRE(uncalib_frame.getTotalFrameCount() == 500);
        REQUIRE(uncalib_frame.isDense());
        
        UncalibratedIndex first_index = uncalib_frame.getTimeAtIndex(TimeFrameIndex(0));
        REQUIRE(first_index.getValue() == 1000);
        
        UncalibratedIndex last_index = uncalib_frame.getTimeAtIndex(TimeFrameIndex(499));
        REQUIRE(last_index.getValue() == 1000 + 499 * 2);
    }
    
    SECTION("Sparse uncalibrated storage") {
        std::vector<int64_t> indices = {10, 50, 75, 120, 200};
        UncalibratedTimeFrame uncalib_frame(indices);
        
        REQUIRE(uncalib_frame.getTotalFrameCount() == 5);
        REQUIRE(uncalib_frame.isSparse());
        
        UncalibratedIndex middle_index = uncalib_frame.getTimeAtIndex(TimeFrameIndex(2));
        REQUIRE(middle_index.getValue() == 75);
    }
}

TEST_CASE("TimeFrameUtils - Factory functions", "[timeframev2][utils]") {
    SECTION("createDenseClockTimeFrame") {
        auto clock_frame = TimeFrameUtils::createDenseClockTimeFrame(0, 30000, 30000.0);
        
        REQUIRE(clock_frame != nullptr);
        REQUIRE(clock_frame->getTotalFrameCount() == 30000);
        REQUIRE(clock_frame->isDense());
        REQUIRE(clock_frame->getSamplingRate().has_value());
        REQUIRE_THAT(clock_frame->getSamplingRate().value(), Catch::Matchers::WithinRel(30000.0, 1e-9));
    }
    
    SECTION("createSparseCameraTimeFrame") {
        std::vector<int64_t> frame_indices = {0, 10, 25, 50, 100};
        auto camera_frame = TimeFrameUtils::createSparseCameraTimeFrame(std::move(frame_indices));
        
        REQUIRE(camera_frame != nullptr);
        REQUIRE(camera_frame->getTotalFrameCount() == 5);
        REQUIRE(camera_frame->isSparse());
        
        CameraFrameIndex frame = camera_frame->getTimeAtIndex(TimeFrameIndex(2));
        REQUIRE(frame.getValue() == 25);
    }
    
    SECTION("createDenseCameraTimeFrame") {
        auto camera_frame = TimeFrameUtils::createDenseCameraTimeFrame(0, 1000);
        
        REQUIRE(camera_frame != nullptr);
        REQUIRE(camera_frame->getTotalFrameCount() == 1000);
        REQUIRE(camera_frame->isDense());
        
        CameraFrameIndex first_frame = camera_frame->getTimeAtIndex(TimeFrameIndex(0));
        CameraFrameIndex last_frame = camera_frame->getTimeAtIndex(TimeFrameIndex(999));
        
        REQUIRE(first_frame.getValue() == 0);
        REQUIRE(last_frame.getValue() == 999);
    }
}

TEST_CASE("TimeFrameV2 - Memory efficiency scenarios", "[timeframev2][memory]") {
    SECTION("Dense storage should be memory efficient") {
        // Dense storage for 1 million samples should use minimal memory
        ClockTimeFrame large_dense_frame(0, 1000000, 1, 30000.0);
        
        REQUIRE(large_dense_frame.getTotalFrameCount() == 1000000);
        REQUIRE(large_dense_frame.isDense());
        
        // Should be able to access any element efficiently
        ClockTicks middle_tick = large_dense_frame.getTimeAtIndex(TimeFrameIndex(500000));
        REQUIRE(middle_tick.getValue() == 500000);
        
        ClockTicks last_tick = large_dense_frame.getTimeAtIndex(TimeFrameIndex(999999));
        REQUIRE(last_tick.getValue() == 999999);
    }
    
    SECTION("Sparse storage for irregular sampling") {
        // Create irregular timing typical of sparse events
        std::vector<int64_t> sparse_times;
        sparse_times.reserve(1000);
        
        int64_t current_time = 0;
        for (int i = 0; i < 1000; ++i) {
            current_time += (i % 10 == 0) ? 100 : 10; // Irregular intervals
            sparse_times.push_back(current_time);
        }
        
        CameraTimeFrame sparse_frame(sparse_times);
        
        REQUIRE(sparse_frame.getTotalFrameCount() == 1000);
        REQUIRE(sparse_frame.isSparse());
        
        // Should handle irregular timing correctly
        CameraFrameIndex first_frame = sparse_frame.getTimeAtIndex(TimeFrameIndex(0));
        CameraFrameIndex some_frame = sparse_frame.getTimeAtIndex(TimeFrameIndex(500));
        
        REQUIRE(first_frame.getValue() == sparse_times[0]);
        REQUIRE(some_frame.getValue() == sparse_times[500]);
    }
}

TEST_CASE("TimeFrameV2 - Type safety verification", "[timeframev2][safety]") {
    SECTION("Different coordinate types cannot be mixed") {
        ClockTimeFrame clock_frame(0, 1000, 1, 30000.0);
        CameraTimeFrame camera_frame(0, 1000, 1);
        
        ClockTicks clock_tick = clock_frame.getTimeAtIndex(TimeFrameIndex(100));
        CameraFrameIndex camera_frame_idx = camera_frame.getTimeAtIndex(TimeFrameIndex(100));
        
        // These should not compile if uncommented:
        // int64_t result = clock_frame.getIndexAtTime(camera_frame_idx); // Type error
        // int64_t result = camera_frame.getIndexAtTime(clock_tick);      // Type error
        
        // But we can access underlying values if needed
        REQUIRE(clock_tick.getValue() == camera_frame_idx.getValue());
    }
    
    SECTION("Conversion operations respect type constraints") {
        ClockTimeFrame clock_frame(0, 1000, 1, 30000.0);
        CameraTimeFrame camera_frame(0, 1000, 1); // No sampling rate
        
        ClockTicks clock_tick(15000);
        CameraFrameIndex camera_idx(100);
        
        // Clock frame should support seconds conversion
        auto seconds_opt = clock_frame.coordinateToSeconds(clock_tick);
        REQUIRE(seconds_opt.has_value());
        
        // Camera frame should not support seconds conversion
        // This would not compile:
        // auto camera_seconds = camera_frame.coordinateToSeconds(camera_idx);
    }
}

TEST_CASE("AnyTimeFrame variant", "[timeframev2][variant]") {
    SECTION("Can hold different TimeFrame types") {
        auto clock_frame = TimeFrameUtils::createDenseClockTimeFrame(0, 1000, 30000.0);
        auto camera_frame = TimeFrameUtils::createSparseCameraTimeFrame({0, 10, 20, 30});
        
        AnyTimeFrame any_clock = clock_frame;
        AnyTimeFrame any_camera = camera_frame;
        
        // Test that we can use std::visit or std::holds_alternative
        bool is_clock = std::holds_alternative<std::shared_ptr<ClockTimeFrame>>(any_clock);
        bool is_camera = std::holds_alternative<std::shared_ptr<CameraTimeFrame>>(any_camera);
        
        REQUIRE(is_clock);
        REQUIRE(is_camera);
        
        // Test accessing through variant
        auto retrieved_clock = std::get<std::shared_ptr<ClockTimeFrame>>(any_clock);
        REQUIRE(retrieved_clock->getTotalFrameCount() == 1000);
        
        auto retrieved_camera = std::get<std::shared_ptr<CameraTimeFrame>>(any_camera);
        REQUIRE(retrieved_camera->getTotalFrameCount() == 4);
    }
} 