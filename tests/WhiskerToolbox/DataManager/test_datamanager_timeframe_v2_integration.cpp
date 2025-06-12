#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DataManager.hpp"
#include "TimeFrame/TimeFrameV2.hpp"
#include "TimeFrame/StrongTimeTypes.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"

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
        
        // Create AnalogTimeSeries with legacy interface
        auto neural_series = std::make_shared<AnalogTimeSeries>(neural_data);
        
        // Use new DataManager API to associate with TimeFrameV2
        dm.setDataV2("neural_signal", neural_series, clock_timeframe, "master_clock");
        
        // Verify the data has the TimeFrameV2 reference
        auto retrieved_series = dm.getData<AnalogTimeSeries>("neural_signal");
        REQUIRE(retrieved_series != nullptr);
        REQUIRE(retrieved_series->hasTimeFrameV2());
        
        auto timeframe_ref = retrieved_series->getTimeFrameV2();
        REQUIRE(timeframe_ref.has_value());
        
        // Test type-safe coordinate queries
        ClockTicks start_tick(1000);
        ClockTicks end_tick(2000);
        
        auto values_in_range = retrieved_series->getDataInCoordinateRange(start_tick, end_tick);
        REQUIRE(values_in_range.size() == 1001); // 1000 to 2000 inclusive
        
        // Verify we got the expected data
        for (size_t i = 0; i < values_in_range.size(); ++i) {
            REQUIRE_THAT(values_in_range[i], 
                        Catch::Matchers::WithinRel(neural_data[1000 + i], 1e-6f));
        }
    }
    
    SECTION("Type safety verification - coordinate mismatch should throw") {
        DataManager dm;
        
        // Create a camera timeframe
        auto camera_timeframe = TimeFrameUtils::createDenseCameraTimeFrame(0, 100);
        
        std::vector<float> data(100, 1.0f);
        auto series = std::make_shared<AnalogTimeSeries>(data);
        
        dm.setDataV2("test_data", series, camera_timeframe);
        auto retrieved = dm.getData<AnalogTimeSeries>("test_data");
        
        // This should work - correct coordinate type
        CameraFrameIndex start_frame(10);
        CameraFrameIndex end_frame(20);
        auto values = retrieved->getDataInCoordinateRange(start_frame, end_frame);
        REQUIRE(values.size() == 11);
        
        // This should throw - incorrect coordinate type
        ClockTicks wrong_start(100);
        ClockTicks wrong_end(200);
        REQUIRE_THROWS_AS(retrieved->getDataInCoordinateRange(wrong_start, wrong_end), 
                         std::runtime_error);
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
            auto coord_100 = timeframe_ptr->getTimeAtIndex(100);
            auto coord_1000 = timeframe_ptr->getTimeAtIndex(1000);
            
            if constexpr (std::is_same_v<typename std::decay_t<decltype(*timeframe_ptr)>::coordinate_type, ClockTicks>) {
                REQUIRE(coord_100.getValue() == 100);
                REQUIRE(coord_1000.getValue() == 1000);
            }
        }, dense_ref.value());
        
        std::visit([](auto const & timeframe_ptr) {
            auto coord_100 = timeframe_ptr->getTimeAtIndex(100);
            auto coord_1000 = timeframe_ptr->getTimeAtIndex(1000);
            
            if constexpr (std::is_same_v<typename std::decay_t<decltype(*timeframe_ptr)>::coordinate_type, ClockTicks>) {
                REQUIRE(coord_100.getValue() == 100);
                REQUIRE(coord_1000.getValue() == 1000);
            }
        }, sparse_ref.value());
    }
    
    SECTION("Coordinate conversion between timeframes") {
        DataManager dm;
        
        // Create master clock (30kHz)
        auto master_clock = TimeFrameUtils::createDenseClockTimeFrame(0, 30000, 30000.0);
        
        // Create camera timeframe (every 300 ticks = 100 Hz)
        std::vector<int64_t> camera_sync_ticks;
        for (int i = 0; i < 100; ++i) {
            camera_sync_ticks.push_back(i * 300);
        }
        auto camera_frames = TimeFrameUtils::createSparseCameraTimeFrame(camera_sync_ticks);
        
        dm.setTimeV2("master", master_clock);
        dm.setTimeV2("camera", camera_frames);
        
        // Create neural data on master clock
        std::vector<float> neural_data(30000, 1.0f);
        // Add some spikes at known positions
        neural_data[1500] = 10.0f;  // Spike at tick 1500 (should be in camera frame 5)
        neural_data[9000] = 15.0f;  // Spike at tick 9000 (should be in camera frame 30)
        
        auto neural_series = std::make_shared<AnalogTimeSeries>(neural_data);
        dm.setDataV2("neural", neural_series, master_clock);
        
        // Query neural data using clock coordinates
        auto retrieved_neural = dm.getData<AnalogTimeSeries>("neural");
        
        ClockTicks start_tick(1400);
        ClockTicks end_tick(1600);
        auto values = retrieved_neural->getDataInCoordinateRange(start_tick, end_tick);
        
        // Should contain the spike
        bool found_spike = false;
        for (auto val : values) {
            if (val > 5.0f) {
                found_spike = true;
                break;
            }
        }
        REQUIRE(found_spike);
        
        // Test coordinate and value retrieval together
        auto [coords, vals] = retrieved_neural->getDataAndCoordsInRange(start_tick, end_tick);
        REQUIRE(coords.size() == vals.size());
        REQUIRE(coords.size() == 201); // 1400 to 1600 inclusive
        
        // Find the spike coordinate
        for (size_t i = 0; i < coords.size(); ++i) {
            if (vals[i] > 5.0f) {
                REQUIRE(coords[i].getValue() == 1500);
                break;
            }
        }
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
        auto legacy_series = std::make_shared<AnalogTimeSeries>(legacy_data);
        dm.setData("legacy_data", legacy_series, "legacy_clock");
        
        // Create data with new system
        std::vector<float> new_data = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
        auto new_series = std::make_shared<AnalogTimeSeries>(new_data);
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
        auto series = std::make_shared<AnalogTimeSeries>(data);
        
        // This should fail gracefully (error message to stderr)
        dm.setDataV2("test", series, "nonexistent_key");
        
        // Data should not be set
        REQUIRE(dm.getData<AnalogTimeSeries>("test") == nullptr);
        
        // Test AnalogTimeSeries methods without TimeFrameV2
        auto series_no_tf = std::make_shared<AnalogTimeSeries>(data);
        ClockTicks start(0);
        ClockTicks end(10);
        
        REQUIRE_THROWS_AS(series_no_tf->getDataInCoordinateRange(start, end), 
                         std::runtime_error);
        REQUIRE_THROWS_AS(series_no_tf->getDataAndCoordsInRange(start, end), 
                         std::runtime_error);
    }
} 