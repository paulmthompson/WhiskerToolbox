#include "StrongTimeTypes.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

TEST_CASE("CameraFrameIndex - Basic operations", "[strongtypes][cameraframe]") {
    SECTION("Construction and value retrieval") {
        CameraFrameIndex frame(42);
        REQUIRE(frame.getValue() == 42);
        
        CameraFrameIndex zero_frame(0);
        REQUIRE(zero_frame.getValue() == 0);
        
        CameraFrameIndex negative_frame(-10);
        REQUIRE(negative_frame.getValue() == -10);
    }
    
    SECTION("Comparison operators") {
        CameraFrameIndex frame1(10);
        CameraFrameIndex frame2(20);
        CameraFrameIndex frame3(10);
        
        REQUIRE(frame1 == frame3);
        REQUIRE(frame1 != frame2);
        REQUIRE(frame1 < frame2);
        REQUIRE(frame2 > frame1);
        REQUIRE(frame1 <= frame2);
        REQUIRE(frame1 <= frame3);
        REQUIRE(frame2 >= frame1);
        REQUIRE(frame3 >= frame1);
    }
    
    SECTION("Arithmetic operations") {
        CameraFrameIndex frame(100);
        
        // Addition with offset
        CameraFrameIndex plus_ten = frame + 10;
        REQUIRE(plus_ten.getValue() == 110);
        
        // Subtraction with offset
        CameraFrameIndex minus_five = frame - 5;
        REQUIRE(minus_five.getValue() == 95);
        
        // Difference between frames
        CameraFrameIndex other_frame(150);
        int64_t diff = other_frame - frame;
        REQUIRE(diff == 50);
    }
}

TEST_CASE("ClockTicks - Basic operations", "[strongtypes][clockticks]") {
    SECTION("Construction and value retrieval") {
        ClockTicks ticks(30000);
        REQUIRE(ticks.getValue() == 30000);
        
        ClockTicks zero_ticks(0);
        REQUIRE(zero_ticks.getValue() == 0);
    }
    
    SECTION("Comparison operators") {
        ClockTicks ticks1(1000);
        ClockTicks ticks2(2000);
        ClockTicks ticks3(1000);
        
        REQUIRE(ticks1 == ticks3);
        REQUIRE(ticks1 != ticks2);
        REQUIRE(ticks1 < ticks2);
        REQUIRE(ticks2 > ticks1);
        REQUIRE(ticks1 <= ticks2);
        REQUIRE(ticks1 <= ticks3);
        REQUIRE(ticks2 >= ticks1);
        REQUIRE(ticks3 >= ticks1);
    }
    
    SECTION("Arithmetic operations") {
        ClockTicks ticks(30000);
        
        // Addition with offset
        ClockTicks plus_thousand = ticks + 1000;
        REQUIRE(plus_thousand.getValue() == 31000);
        
        // Subtraction with offset
        ClockTicks minus_five_hundred = ticks - 500;
        REQUIRE(minus_five_hundred.getValue() == 29500);
        
        // Difference between ticks
        ClockTicks other_ticks(35000);
        int64_t diff = other_ticks - ticks;
        REQUIRE(diff == 5000);
    }
}

TEST_CASE("Seconds - Basic operations", "[strongtypes][seconds]") {
    SECTION("Construction and value retrieval") {
        Seconds time(42.5);
        REQUIRE_THAT(time.getValue(), Catch::Matchers::WithinRel(42.5, 1e-9));
        
        Seconds zero_time(0.0);
        REQUIRE_THAT(zero_time.getValue(), Catch::Matchers::WithinRel(0.0, 1e-9));
        
        Seconds negative_time(-1.5);
        REQUIRE_THAT(negative_time.getValue(), Catch::Matchers::WithinRel(-1.5, 1e-9));
    }
    
    SECTION("Comparison operators") {
        Seconds time1(1.5);
        Seconds time2(2.5);
        Seconds time3(1.5);
        
        REQUIRE(time1 == time3);
        REQUIRE(time1 != time2);
        REQUIRE(time1 < time2);
        REQUIRE(time2 > time1);
        REQUIRE(time1 <= time2);
        REQUIRE(time1 <= time3);
        REQUIRE(time2 >= time1);
        REQUIRE(time3 >= time1);
    }
    
    SECTION("Arithmetic operations") {
        Seconds time(10.5);
        
        // Addition with offset
        Seconds plus_half = time + 0.5;
        REQUIRE_THAT(plus_half.getValue(), Catch::Matchers::WithinRel(11.0, 1e-9));
        
        // Subtraction with offset
        Seconds minus_two = time - 2.0;
        REQUIRE_THAT(minus_two.getValue(), Catch::Matchers::WithinRel(8.5, 1e-9));
        
        // Difference between times
        Seconds other_time(15.25);
        double diff = other_time - time;
        REQUIRE_THAT(diff, Catch::Matchers::WithinRel(4.75, 1e-9));
    }
}

TEST_CASE("UncalibratedIndex - Basic operations and unsafe conversions", "[strongtypes][uncalibrated]") {
    SECTION("Construction and value retrieval") {
        UncalibratedIndex index(1000);
        REQUIRE(index.getValue() == 1000);
        
        UncalibratedIndex zero_index(0);
        REQUIRE(zero_index.getValue() == 0);
    }
    
    SECTION("Comparison operators") {
        UncalibratedIndex index1(100);
        UncalibratedIndex index2(200);
        UncalibratedIndex index3(100);
        
        REQUIRE(index1 == index3);
        REQUIRE(index1 != index2);
        REQUIRE(index1 < index2);
        REQUIRE(index2 > index1);
        REQUIRE(index1 <= index2);
        REQUIRE(index1 <= index3);
        REQUIRE(index2 >= index1);
        REQUIRE(index3 >= index1);
    }
    
    SECTION("Arithmetic operations") {
        UncalibratedIndex index(500);
        
        // Addition with offset
        UncalibratedIndex plus_fifty = index + 50;
        REQUIRE(plus_fifty.getValue() == 550);
        
        // Subtraction with offset
        UncalibratedIndex minus_twenty = index - 20;
        REQUIRE(minus_twenty.getValue() == 480);
        
        // Difference between indices
        UncalibratedIndex other_index(750);
        int64_t diff = other_index - index;
        REQUIRE(diff == 250);
    }
    
    SECTION("Unsafe conversions") {
        UncalibratedIndex index(42);
        
        // Convert to CameraFrameIndex
        CameraFrameIndex camera_frame = index.unsafeToCameraFrameIndex();
        REQUIRE(camera_frame.getValue() == 42);
        
        // Convert to ClockTicks
        ClockTicks clock_ticks = index.unsafeToClockTicks();
        REQUIRE(clock_ticks.getValue() == 42);
        
        // Verify that the conversions produce independent objects
        UncalibratedIndex modified = index + 10;
        REQUIRE(camera_frame.getValue() == 42); // Should remain unchanged
        REQUIRE(clock_ticks.getValue() == 42);   // Should remain unchanged
    }
}

TEST_CASE("TimeCoordinate variant and utility functions", "[strongtypes][variant]") {
    SECTION("TimeCoordinate variant can hold all types") {
        TimeCoordinate camera_coord = CameraFrameIndex(100);
        TimeCoordinate clock_coord = ClockTicks(30000);
        TimeCoordinate seconds_coord = Seconds(42.5);
        TimeCoordinate uncalib_coord = UncalibratedIndex(999);
        
        // Verify that we can extract values
        REQUIRE(getTimeValue<int64_t>(camera_coord) == 100);
        REQUIRE(getTimeValue<int64_t>(clock_coord) == 30000);
        REQUIRE(getTimeValue<int64_t>(seconds_coord) == 42); // Truncated to int
        REQUIRE(getTimeValue<int64_t>(uncalib_coord) == 999);
        
        // Verify that we can extract double values
        REQUIRE_THAT(getTimeValue<double>(camera_coord), Catch::Matchers::WithinRel(100.0, 1e-9));
        REQUIRE_THAT(getTimeValue<double>(clock_coord), Catch::Matchers::WithinRel(30000.0, 1e-9));
        REQUIRE_THAT(getTimeValue<double>(seconds_coord), Catch::Matchers::WithinRel(42.5, 1e-9));
        REQUIRE_THAT(getTimeValue<double>(uncalib_coord), Catch::Matchers::WithinRel(999.0, 1e-9));
    }
    
    SECTION("getTimeValue template specialization for Seconds") {
        TimeCoordinate seconds_coord = Seconds(3.14159);
        
        // Should preserve precision for double extraction
        REQUIRE_THAT(getTimeValue<double>(seconds_coord), Catch::Matchers::WithinRel(3.14159, 1e-9));
        
        // Should truncate for int extraction
        REQUIRE(getTimeValue<int64_t>(seconds_coord) == 3);
    }
}

TEST_CASE("Strong types prevent accidental mixing", "[strongtypes][safety]") {
    SECTION("Different strong types cannot be directly compared") {
        CameraFrameIndex camera_frame(100);
        ClockTicks clock_ticks(100);
        
        // These should not compile if uncommented:
        // bool result = camera_frame == clock_ticks;  // Compilation error
        // bool result = camera_frame < clock_ticks;   // Compilation error
        
        // But we can compare their underlying values if needed
        REQUIRE(camera_frame.getValue() == clock_ticks.getValue());
    }
    
    SECTION("Arithmetic operations preserve type safety") {
        CameraFrameIndex frame1(10);
        CameraFrameIndex frame2(20);
        
        // These operations should return the same type
        CameraFrameIndex sum = frame1 + 5;
        REQUIRE(sum.getValue() == 15);
        
        CameraFrameIndex diff_frame = frame2 - 3;
        REQUIRE(diff_frame.getValue() == 17);
        
        // Difference between same types returns numeric value
        int64_t difference = frame2 - frame1;
        REQUIRE(difference == 10);
    }
}

TEST_CASE("Strong types - Edge cases and boundary conditions", "[strongtypes][edge]") {
    SECTION("Large values") {
        int64_t large_value = 9223372036854775807; // max int64_t
        
        CameraFrameIndex large_frame(large_value);
        REQUIRE(large_frame.getValue() == large_value);
        
        ClockTicks large_ticks(large_value);
        REQUIRE(large_ticks.getValue() == large_value);
        
        UncalibratedIndex large_uncalib(large_value);
        REQUIRE(large_uncalib.getValue() == large_value);
    }
    
    SECTION("Negative values") {
        int64_t negative_value = -1000;
        
        CameraFrameIndex negative_frame(negative_value);
        REQUIRE(negative_frame.getValue() == negative_value);
        
        ClockTicks negative_ticks(negative_value);
        REQUIRE(negative_ticks.getValue() == negative_value);
        
        UncalibratedIndex negative_uncalib(negative_value);
        REQUIRE(negative_uncalib.getValue() == negative_value);
    }
    
    SECTION("Zero values") {
        CameraFrameIndex zero_frame(0);
        ClockTicks zero_ticks(0);
        Seconds zero_seconds(0.0);
        UncalibratedIndex zero_uncalib(0);
        
        REQUIRE(zero_frame.getValue() == 0);
        REQUIRE(zero_ticks.getValue() == 0);
        REQUIRE_THAT(zero_seconds.getValue(), Catch::Matchers::WithinRel(0.0, 1e-9));
        REQUIRE(zero_uncalib.getValue() == 0);
    }
    
    SECTION("Very small and very large double values for Seconds") {
        double very_small = 1e-15;
        double very_large = 1e15;
        
        Seconds small_time(very_small);
        Seconds large_time(very_large);
        
        REQUIRE_THAT(small_time.getValue(), Catch::Matchers::WithinRel(very_small, 1e-3));
        REQUIRE_THAT(large_time.getValue(), Catch::Matchers::WithinRel(very_large, 1e-9));
    }
} 