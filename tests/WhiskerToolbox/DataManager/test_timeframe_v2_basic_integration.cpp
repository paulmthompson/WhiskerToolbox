#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DataManager.hpp"
#include "TimeFrame/TimeFrameV2.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

#include <memory>
#include <vector>

TEST_CASE("TimeFrameV2 Basic Integration", "[timeframev2][integration][basic]") {
    
    SECTION("DataManager TimeFrameV2 registry basics") {
        DataManager dm;
        
        // Create a simple clock timeframe
        REQUIRE(dm.createClockTimeFrame("test_clock", 0, 1000, 1000.0));
        
        // Verify it was registered
        auto timeframe_opt = dm.getTimeV2("test_clock");
        REQUIRE(timeframe_opt.has_value());
        
        // Check the registry keys
        auto keys = dm.getTimeFrameV2Keys();
        REQUIRE(keys.size() == 1);
        REQUIRE(keys[0] == "test_clock");
        
        // Remove and verify
        REQUIRE(dm.removeTimeV2("test_clock"));
        REQUIRE(!dm.getTimeV2("test_clock").has_value());
        REQUIRE(dm.getTimeFrameV2Keys().empty());
    }
    
    SECTION("AnalogTimeSeries TimeFrameV2 integration") {
        DataManager dm;
        
        // Create some test data
        std::vector<float> test_data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
        auto analog_series = std::make_shared<AnalogTimeSeries>(test_data);
        
        // Create a clock timeframe (10 samples at 10Hz)
        auto clock_timeframe = TimeFrameUtils::createDenseClockTimeFrame(0, 10, 10.0);
        
        // Associate data with timeframe using new API
        dm.setDataV2("test_signal", analog_series, clock_timeframe, "test_clock");
        
        // Retrieve and verify
        auto retrieved = dm.getData<AnalogTimeSeries>("test_signal");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->hasTimeFrameV2());
        
        // Test type-safe coordinate queries
        ClockTicks start_tick(2);
        ClockTicks end_tick(5);
        
        auto values_in_range = retrieved->getDataInCoordinateRange(start_tick, end_tick);
        REQUIRE(values_in_range.size() == 4); // ticks 2,3,4,5 = indices 2,3,4,5
        
        // Verify correct values
        REQUIRE_THAT(values_in_range[0], Catch::Matchers::WithinRel(3.0f, 1e-6f)); // index 2
        REQUIRE_THAT(values_in_range[1], Catch::Matchers::WithinRel(4.0f, 1e-6f)); // index 3
        REQUIRE_THAT(values_in_range[2], Catch::Matchers::WithinRel(5.0f, 1e-6f)); // index 4
        REQUIRE_THAT(values_in_range[3], Catch::Matchers::WithinRel(6.0f, 1e-6f)); // index 5
    }
    
    SECTION("Camera vs Clock coordinate type safety") {
        DataManager dm;
        
        // Create camera timeframe
        auto camera_timeframe = TimeFrameUtils::createDenseCameraTimeFrame(0, 5);
        
        std::vector<float> data = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
        auto series = std::make_shared<AnalogTimeSeries>(data);
        
        dm.setDataV2("camera_data", series, camera_timeframe);
        auto retrieved = dm.getData<AnalogTimeSeries>("camera_data");
        
        // This should work - matching coordinate type
        CameraFrameIndex start_frame(1);
        CameraFrameIndex end_frame(3);
        auto camera_values = retrieved->getDataInCoordinateRange(start_frame, end_frame);
        REQUIRE(camera_values.size() == 3);
        REQUIRE_THAT(camera_values[0], Catch::Matchers::WithinRel(20.0f, 1e-6f));
        REQUIRE_THAT(camera_values[1], Catch::Matchers::WithinRel(30.0f, 1e-6f));
        REQUIRE_THAT(camera_values[2], Catch::Matchers::WithinRel(40.0f, 1e-6f));
        
        // This should throw - mismatched coordinate type
        ClockTicks wrong_start(1);
        ClockTicks wrong_end(3);
        REQUIRE_THROWS_AS(retrieved->getDataInCoordinateRange(wrong_start, wrong_end), 
                         std::runtime_error);
    }
    
    SECTION("TimeFrameV2 coordinate and value retrieval") {
        DataManager dm;
        
        // Create clock timeframe starting at tick 100
        auto clock_timeframe = TimeFrameUtils::createDenseClockTimeFrame(100, 5, 1000.0);
        
        std::vector<float> data = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
        auto series = std::make_shared<AnalogTimeSeries>(data);
        
        dm.setDataV2("offset_data", series, clock_timeframe);
        auto retrieved = dm.getData<AnalogTimeSeries>("offset_data");
        
        // Query with coordinates and values
        ClockTicks start_tick(101);
        ClockTicks end_tick(103);
        
        auto [coords, values] = retrieved->getDataAndCoordsInRange(start_tick, end_tick);
        
        REQUIRE(coords.size() == 3);
        REQUIRE(values.size() == 3);
        
        // Check coordinates (101, 102, 103)
        REQUIRE(coords[0].getValue() == 101);
        REQUIRE(coords[1].getValue() == 102);
        REQUIRE(coords[2].getValue() == 103);
        
        // Check values (indices 1, 2, 3)
        REQUIRE_THAT(values[0], Catch::Matchers::WithinRel(2.2f, 1e-6f));
        REQUIRE_THAT(values[1], Catch::Matchers::WithinRel(3.3f, 1e-6f));
        REQUIRE_THAT(values[2], Catch::Matchers::WithinRel(4.4f, 1e-6f));
    }
    
    SECTION("Backward compatibility - both APIs coexist") {
        DataManager dm;
        
        // Use legacy API
        std::vector<int> legacy_times = {0, 10, 20, 30, 40};
        auto legacy_timeframe = std::make_shared<TimeFrame>(legacy_times);
        dm.setTime("legacy", legacy_timeframe);
        
        std::vector<float> legacy_data = {100.0f, 200.0f, 300.0f, 400.0f, 500.0f};
        auto legacy_series = std::make_shared<AnalogTimeSeries>(legacy_data);
        dm.setData("legacy_data", legacy_series, "legacy");
        
        // Use new API
        dm.createCameraTimeFrame("new_camera", {0, 10, 20, 30, 40});
        
        std::vector<float> new_data = {1000.0f, 2000.0f, 3000.0f, 4000.0f, 5000.0f};
        auto new_series = std::make_shared<AnalogTimeSeries>(new_data);
        auto camera_tf = dm.getTimeV2("new_camera").value();
        dm.setDataV2("new_data", new_series, camera_tf);
        
        // Both should work independently
        auto legacy_retrieved = dm.getData<AnalogTimeSeries>("legacy_data");
        auto new_retrieved = dm.getData<AnalogTimeSeries>("new_data");
        
        REQUIRE(legacy_retrieved != nullptr);
        REQUIRE(new_retrieved != nullptr);
        
        // Legacy doesn't have TimeFrameV2
        REQUIRE(!legacy_retrieved->hasTimeFrameV2());
        
        // New has TimeFrameV2
        REQUIRE(new_retrieved->hasTimeFrameV2());
        
        // New API should work
        CameraFrameIndex start(1);
        CameraFrameIndex end(2);
        auto new_values = new_retrieved->getDataInCoordinateRange(start, end);
        REQUIRE(new_values.size() == 2);
        REQUIRE_THAT(new_values[0], Catch::Matchers::WithinRel(2000.0f, 1e-6f));
        REQUIRE_THAT(new_values[1], Catch::Matchers::WithinRel(3000.0f, 1e-6f));
        
        // Legacy should throw if TimeFrameV2 methods are called
        ClockTicks legacy_start(0);
        ClockTicks legacy_end(1);
        REQUIRE_THROWS_AS(legacy_retrieved->getDataInCoordinateRange(legacy_start, legacy_end), 
                         std::runtime_error);
    }
    
    SECTION("Error handling") {
        DataManager dm;
        
        // Non-existent TimeFrameV2
        REQUIRE(!dm.getTimeV2("nonexistent").has_value());
        REQUIRE(!dm.removeTimeV2("nonexistent"));
        
        // Empty data
        std::vector<float> empty_data;
        auto empty_series = std::make_shared<AnalogTimeSeries>(empty_data);
        auto timeframe = TimeFrameUtils::createDenseClockTimeFrame(0, 0, 1000.0);
        
        dm.setDataV2("empty", empty_series, timeframe);
        auto retrieved = dm.getData<AnalogTimeSeries>("empty");
        
        ClockTicks start(0);
        ClockTicks end(10);
        auto values = retrieved->getDataInCoordinateRange(start, end);
        REQUIRE(values.empty());
    }
} 