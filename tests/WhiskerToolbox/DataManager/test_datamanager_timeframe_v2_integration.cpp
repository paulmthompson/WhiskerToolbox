#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DataManager.hpp"
#include "TimeFrame/TimeFrameV2.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TimeFrame.hpp"

#include <memory>
#include <ranges>
#include <vector>
#include <random>

TEST_CASE("DataManager TimeFrameV2 Integration", "[datamanager][timeframev2][integration]") {
    
    SECTION("Basic TimeFrameV2 registry operations") {
        DataManager dm;
        
        // Create a dense clock timeframe (30kHz sampling rate)
        REQUIRE(dm.createClockTimeFrame("master_clock", 0, 30000, 30000.0));
        
        // Create a sparse camera timeframe
        std::vector<int64_t> camera_frames = {0, 300, 600, 900, 1200}; // Every 300 ticks
        REQUIRE(dm.createCameraTimeFrame("camera", camera_frames));
        
        // Verify registration
        auto clock_tf = dm.getTimeV2("master_clock");
        auto camera_tf = dm.getTimeV2("camera");
        
        REQUIRE(clock_tf.has_value());
        REQUIRE(camera_tf.has_value());
        
        // Verify keys
        auto keys = dm.getTimeFrameV2Keys();
        REQUIRE(keys.size() == 2);
        REQUIRE(std::find(keys.begin(), keys.end(), "master_clock") != keys.end());
        REQUIRE(std::find(keys.begin(), keys.end(), "camera") != keys.end());
        
        // Test removal
        REQUIRE(dm.removeTimeV2("camera"));
        REQUIRE(!dm.getTimeV2("camera").has_value());
        
        keys = dm.getTimeFrameV2Keys();
        REQUIRE(keys.size() == 1);
    }
    
    SECTION("AnalogTimeSeries with direct TimeFrameV2 integration") {
        DataManager dm;
        
        // Create a clock timeframe for neural data
        auto clock_timeframe = TimeFrameUtils::createDenseClockTimeFrame(0, 10000, 30000.0);
        
        // Create synthetic neural data
        std::vector<float> neural_data(10000);
        std::random_device rd;
        std::mt19937 gen(42); // Fixed seed for reproducibility
        std::normal_distribution<float> dist(0.0f, 1.0f);
        
        for (size_t i = 0; i < neural_data.size(); ++i) {
            neural_data[i] = dist(gen) + std::sin(2.0f * M_PI * i / 1000.0f);
        }
        std::vector<TimeFrameIndex> time_vector;
        for (size_t i = 0; i < neural_data.size(); ++i) {
            time_vector.push_back(TimeFrameIndex(i));
        }
        
        // Create AnalogTimeSeries with legacy interface
        auto neural_series = std::make_shared<AnalogTimeSeries>(neural_data, time_vector);
        
        // Use new DataManager API to associate with TimeFrameV2
        dm.setDataV2("neural_signal", neural_series, clock_timeframe, "master_clock");
        
        // Verify the data has the TimeFrameV2 reference
        auto retrieved_series = dm.getData<AnalogTimeSeries>("neural_signal");
        REQUIRE(retrieved_series != nullptr);
        REQUIRE(retrieved_series->hasTimeFrameV2());
        
        auto timeframe_ref = retrieved_series->getTimeFrameV2();
        REQUIRE(timeframe_ref.has_value());
        
    }
    
    SECTION("Memory efficiency comparison - dense vs sparse storage") {
        DataManager dm;
        
        // Create large dense timeframe (simulating 30kHz for 10 seconds = 300,000 samples)
        const int64_t num_samples = 300000;
        auto dense_clock = TimeFrameUtils::createDenseClockTimeFrame(0, num_samples, 30000.0);
        
        // Create equivalent sparse timeframe (would be massive vector)
        std::vector<int64_t> sparse_indices(num_samples);
        std::iota(sparse_indices.begin(), sparse_indices.end(), 0);
        auto sparse_clock = TimeFrameUtils::createSparseClockTimeFrame(std::move(sparse_indices), 30000.0);
        
        // Register both
        dm.setTimeV2("dense_clock", dense_clock);
        dm.setTimeV2("sparse_clock", sparse_clock);
        
        // Verify both work correctly
        auto dense_ref = dm.getTimeV2("dense_clock");
        auto sparse_ref = dm.getTimeV2("sparse_clock");
        
        REQUIRE(dense_ref.has_value());
        REQUIRE(sparse_ref.has_value());
        
        // Test coordinate access on both
        std::visit([](auto const & timeframe_ptr) {
            // Test a few coordinates
            auto coord_100 = timeframe_ptr->getTimeAtIndex(TimeFrameIndex(100));
            auto coord_1000 = timeframe_ptr->getTimeAtIndex(TimeFrameIndex(1000));
            
            if constexpr (std::is_same_v<typename std::decay_t<decltype(*timeframe_ptr)>::coordinate_type, ClockTicks>) {
                REQUIRE(coord_100.getValue() == 100);
                REQUIRE(coord_1000.getValue() == 1000);
            }
        }, dense_ref.value());
        
        std::visit([](auto const & timeframe_ptr) {
            auto coord_100 = timeframe_ptr->getTimeAtIndex(TimeFrameIndex(100));
            auto coord_1000 = timeframe_ptr->getTimeAtIndex(TimeFrameIndex(1000));
            
            if constexpr (std::is_same_v<typename std::decay_t<decltype(*timeframe_ptr)>::coordinate_type, ClockTicks>) {
                REQUIRE(coord_100.getValue() == 100);
                REQUIRE(coord_1000.getValue() == 1000);
            }
        }, sparse_ref.value());
    }
    
    SECTION("Backward compatibility - old and new systems coexist") {
        DataManager dm;
        
        // Use legacy TimeFrame API
        std::vector<int> legacy_times = {0, 100, 200, 300, 400};
        auto legacy_timeframe = std::make_shared<TimeFrame>(legacy_times);
        dm.setTime("legacy_clock", legacy_timeframe);
        
        // Use new TimeFrameV2 API
        dm.createCameraTimeFrame("new_camera", {0, 100, 200, 300, 400});
        
        // Create data with legacy system
        std::vector<float> legacy_data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<TimeFrameIndex> time_vector;
        for (size_t i = 0; i < legacy_data.size(); ++i) {
            time_vector.push_back(TimeFrameIndex(i));
        }
        auto legacy_series = std::make_shared<AnalogTimeSeries>(legacy_data, time_vector);
        dm.setData("legacy_data", legacy_series, "legacy_clock");
        
        // Create data with new system
        std::vector<float> new_data = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
        time_vector.clear();
        for (size_t i = 0; i < new_data.size(); ++i) {
            time_vector.push_back(TimeFrameIndex(i));
        }
        auto new_series = std::make_shared<AnalogTimeSeries>(new_data, time_vector);
        auto camera_tf = dm.getTimeV2("new_camera").value();
        dm.setDataV2("new_data", new_series, camera_tf);
        
        // Both should be accessible
        auto legacy_retrieved = dm.getData<AnalogTimeSeries>("legacy_data");
        auto new_retrieved = dm.getData<AnalogTimeSeries>("new_data");
        
        REQUIRE(legacy_retrieved != nullptr);
        REQUIRE(new_retrieved != nullptr);
        
        // Legacy doesn't have TimeFrameV2
        REQUIRE(!legacy_retrieved->hasTimeFrameV2());
        
        // New has TimeFrameV2
        REQUIRE(new_retrieved->hasTimeFrameV2());
        
        // Both timeframe registries should work
        REQUIRE(dm.getTime("legacy_clock") != nullptr);
        REQUIRE(dm.getTimeV2("new_camera").has_value());
        
        // Keys should be separate
        auto legacy_keys = dm.getTimeFrameKeys();
        auto new_keys = dm.getTimeFrameV2Keys();
        
        REQUIRE(std::find(legacy_keys.begin(), legacy_keys.end(), "legacy_clock") != legacy_keys.end());
        REQUIRE(std::find(new_keys.begin(), new_keys.end(), "new_camera") != new_keys.end());
        
        // Cross-contamination check
        REQUIRE(std::find(legacy_keys.begin(), legacy_keys.end(), "new_camera") == legacy_keys.end());
        REQUIRE(std::find(new_keys.begin(), new_keys.end(), "legacy_clock") == new_keys.end());
    }
    
    SECTION("Error handling in new API") {
        DataManager dm;
        
        // Test accessing non-existent TimeFrameV2
        REQUIRE(!dm.getTimeV2("nonexistent").has_value());
        
        // Test removing non-existent TimeFrameV2
        REQUIRE(!dm.removeTimeV2("nonexistent"));
        
        // Test setting data with non-existent TimeFrameV2 key
        std::vector<float> data = {1.0f, 2.0f, 3.0f};
        std::vector<TimeFrameIndex> time_vector;
        for (size_t i = 0; i < data.size(); ++i) {
            time_vector.push_back(TimeFrameIndex(i));
        }
        auto series = std::make_shared<AnalogTimeSeries>(data, time_vector);
        
        // This should fail gracefully (error message to stderr)
        dm.setDataV2("test", series, "nonexistent_key");
        
        // Data should not be set
        REQUIRE(dm.getData<AnalogTimeSeries>("test") == nullptr);
        
    }
}

TEST_CASE("DataManager - Enhanced AnalogTimeSeries Variant Coordinate Support", "[datamanager][analog][variant][coordinates]") {
    
    SECTION("Create AnalogTimeSeries with ClockTicks using convenience methods") {
        DataManager dm;
        
        // Create neural data with convenient API
        std::vector<float> neural_data(1000, 1.0f);
        // Add some spikes
        neural_data[100] = 10.0f;
        neural_data[500] = 15.0f;
        neural_data[800] = 12.0f;
        
        // Use convenience method to create with ClockTicks
        bool success = dm.createAnalogTimeSeriesWithClock(
            "neural_signal", "neural_clock", 
            std::move(neural_data), 0, 30000.0, false);
        
        REQUIRE(success);

 
        
        // Verify creation
        auto series = dm.getData<AnalogTimeSeries>("neural_signal");
        REQUIRE(series != nullptr);
        REQUIRE(series->hasTimeFrameV2());
        REQUIRE(series->getCoordinateType() == "ClockTicks");
        REQUIRE(dm.getAnalogCoordinateType("neural_signal") == "ClockTicks");
        REQUIRE(dm.analogUsesCoordinateType<ClockTicks>("neural_signal"));
        REQUIRE_FALSE(dm.analogUsesCoordinateType<CameraFrameIndex>("neural_signal"));
    

    }
    
    SECTION("Create AnalogTimeSeries with CameraFrameIndex using convenience methods") {
        DataManager dm;
        
        // Create behavioral data synchronized to camera frames
        std::vector<float> position_data{10.5f, 12.3f, 15.7f, 18.2f, 20.1f};
        std::vector<int64_t> frame_indices{100, 120, 140, 160, 180};
        
        bool success = dm.createAnalogTimeSeriesWithCamera(
            "position_x", "camera_sync",
            std::move(position_data), std::move(frame_indices), false);
        
        REQUIRE(success);
        
        // Verify setup
        auto series = dm.getData<AnalogTimeSeries>("position_x");
        REQUIRE(series != nullptr);
        REQUIRE(series->getCoordinateType() == "CameraFrameIndex");
        REQUIRE(dm.analogUsesCoordinateType<CameraFrameIndex>("position_x"));
        
    }
    
    SECTION("Create with dense camera frames") {
        DataManager dm;
        
        std::vector<float> velocity_data{0.5f, 1.2f, 2.1f, 1.8f, 0.9f, 0.3f};
        
        bool success = dm.createAnalogTimeSeriesWithDenseCamera(
            "velocity", "dense_camera",
            std::move(velocity_data), 50, false);
        
        REQUIRE(success);
        
        auto series = dm.getData<AnalogTimeSeries>("velocity");
        REQUIRE(series->getCoordinateType() == "CameraFrameIndex");
        
    }
    
    SECTION("Multi-timeframe experiment with variant coordinates") {
        DataManager dm;
        
        // Create master clock for neural data (30 kHz)
        std::vector<float> neural_data(30000, 0.0f);
        for (int i = 0; i < 30000; i += 1000) {
            neural_data[i] = 5.0f; // Regular spikes every 1000 ticks
        }
        
        dm.createAnalogTimeSeriesWithClock(
            "neural_lfp", "master_clock",
            std::move(neural_data), 0, 30000.0, false);
        
        // Create camera-synchronized behavioral data (100 Hz, so every 300 master ticks)
        std::vector<float> behavior_data;
        std::vector<int64_t> camera_ticks;
        for (int i = 0; i < 100; ++i) {
            behavior_data.push_back(static_cast<float>(i % 10)); // Cyclic pattern
            camera_ticks.push_back(i * 300); // Every 300 master clock ticks
        }
        
        dm.createAnalogTimeSeriesWithCamera(
            "behavior_score", "camera_clock",
            std::move(behavior_data), std::move(camera_ticks), false);
        
        // Verify both were created correctly
        REQUIRE(dm.getAnalogCoordinateType("neural_lfp") == "ClockTicks");
        REQUIRE(dm.getAnalogCoordinateType("behavior_score") == "CameraFrameIndex");
    }
    
    SECTION("Backward compatibility with legacy TimeFrame") {
        DataManager dm;
        
        // Create using legacy API
        std::vector<int> legacy_times = {0, 10, 20, 30};
        auto legacy_timeframe = std::make_shared<TimeFrame>(legacy_times);
        dm.setTime("legacy", legacy_timeframe);
        
        std::vector<float> legacy_data{100.0f, 200.0f, 300.0f, 400.0f};
        std::vector<TimeFrameIndex> time_vector;
        for (size_t i = 0; i < legacy_data.size(); ++i) {
            time_vector.push_back(TimeFrameIndex(i));
        }
        auto legacy_series = std::make_shared<AnalogTimeSeries>(legacy_data, time_vector);
        dm.setData("legacy_data", legacy_series, "legacy");
        
        // Create using new API
        std::vector<float> new_data{1000.0f, 2000.0f, 3000.0f, 4000.0f};
        dm.createAnalogTimeSeriesWithClock(
            "new_data", "new_clock",
            std::move(new_data), 0, 1000.0, false);
        
        // Both should coexist
        auto legacy_retrieved = dm.getData<AnalogTimeSeries>("legacy_data");
        auto new_retrieved = dm.getData<AnalogTimeSeries>("new_data");
        
        REQUIRE(legacy_retrieved != nullptr);
        REQUIRE(new_retrieved != nullptr);
        
        // Legacy should not have TimeFrameV2
        REQUIRE_FALSE(legacy_retrieved->hasTimeFrameV2());
        REQUIRE(dm.getAnalogCoordinateType("legacy_data") == "none");
        
        // New should have TimeFrameV2
        REQUIRE(new_retrieved->hasTimeFrameV2());
        REQUIRE(dm.getAnalogCoordinateType("new_data") == "ClockTicks");
        
    }

} 