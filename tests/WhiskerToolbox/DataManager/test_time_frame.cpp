#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "DataManager.hpp"
#include "TimeFrame.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <memory>
#include <vector>
#include <random>

TEST_CASE("Multi-TimeFrame Integration Tests", "[integration][timeframe]") {
    
    SECTION("Setup master and camera clocks with data conversion") {
        DataManager dm;
        
        // Create master clock: 30000 samples (1 to 30000)
        std::vector<int> master_times(30000);
        std::iota(master_times.begin(), master_times.end(), 1);
        auto master_timeframe = std::make_shared<TimeFrame>(master_times);
        
        // Create camera clock: samples every 300 ticks of master clock
        // This gives us 100 camera frames (30000 / 300 = 100)
        std::vector<int> camera_times;
        for (int i = 0; i < 100; ++i) {
            camera_times.push_back(1 + i * 300); // 1, 301, 601, 901, ...
        }
        auto camera_timeframe = std::make_shared<TimeFrame>(camera_times);
        
        // Register timeframes with DataManager
        REQUIRE(dm.setTime("master", master_timeframe));
        REQUIRE(dm.setTime("camera", camera_timeframe));
        
        // Verify timeframes are registered
        auto retrieved_master = dm.getTime("master");
        auto retrieved_camera = dm.getTime("camera");
        REQUIRE(retrieved_master != nullptr);
        REQUIRE(retrieved_camera != nullptr);
        REQUIRE(retrieved_master->getTotalFrameCount() == 30000);
        REQUIRE(retrieved_camera->getTotalFrameCount() == 100);
        
        // Test time coordinate conversion
        REQUIRE(retrieved_camera->getTimeAtIndex(TimeFrameIndex(0)) == 1);
        REQUIRE(retrieved_camera->getTimeAtIndex(TimeFrameIndex(1)) == 301);
        REQUIRE(retrieved_camera->getTimeAtIndex(TimeFrameIndex(99)) == 29701);
        
        // Test reverse conversion (master time to camera index)
        REQUIRE(retrieved_camera->getIndexAtTime(1) == 0);
        REQUIRE(retrieved_camera->getIndexAtTime(301) == 1);
        REQUIRE(retrieved_camera->getIndexAtTime(150) == 0); // Should round to nearest
        REQUIRE(retrieved_camera->getIndexAtTime(450) == 1); // Should round to nearest
    }
    
    SECTION("Create analog data on master timeframe") {
        DataManager dm;
        
        // Setup timeframes
        std::vector<int> master_times(30000);
        std::iota(master_times.begin(), master_times.end(), 1);
        auto master_timeframe = std::make_shared<TimeFrame>(master_times);
        REQUIRE(dm.setTime("master", master_timeframe));
        
        // Create analog data sampled at master rate
        std::vector<float> analog_values(30000);
        std::random_device rd;
        std::mt19937 gen(42); // Fixed seed for reproducible tests
        std::normal_distribution<float> dist(0.0f, 1.0f);
        
        for (size_t i = 0; i < analog_values.size(); ++i) {
            analog_values[i] = dist(gen) + std::sin(2.0f * M_PI * i / 1000.0f); // Sine wave + noise
        }
        
        std::vector<TimeFrameIndex> analog_indices;
        for (size_t i = 0; i < analog_values.size(); ++i) {
            analog_indices.push_back(TimeFrameIndex(i));
        }
        
        auto analog_series = std::make_shared<AnalogTimeSeries>(analog_values, analog_indices);
        dm.setData<AnalogTimeSeries>("neural_signal", analog_series, "master");
        
        // Verify data is correctly associated
        auto retrieved_analog = dm.getData<AnalogTimeSeries>("neural_signal");
        REQUIRE(retrieved_analog != nullptr);
        REQUIRE(retrieved_analog->getAnalogTimeSeries().size() == 30000);
        REQUIRE(dm.getTimeFrame("neural_signal") == "master");
        
        // Test specific values
        REQUIRE_THAT(retrieved_analog->getAnalogTimeSeries()[0], 
                    Catch::Matchers::WithinRel(analog_values[0], 1e-6f));
        REQUIRE_THAT(retrieved_analog->getAnalogTimeSeries()[1000], 
                    Catch::Matchers::WithinRel(analog_values[1000], 1e-6f));
    }
    
    SECTION("Create digital event series on master timeframe") {
        DataManager dm;
        
        // Setup timeframes
        std::vector<int> master_times(30000);
        std::iota(master_times.begin(), master_times.end(), 1);
        auto master_timeframe = std::make_shared<TimeFrame>(master_times);
        REQUIRE(dm.setTime("master", master_timeframe));
        
        // Create spike events at master resolution
        std::vector<float> spike_times = {
            150.0f, 1250.0f, 2890.0f, 5400.0f, 8750.0f, 12300.0f, 
            15600.0f, 18900.0f, 22100.0f, 25800.0f, 28500.0f
        };
        
        auto spike_series = std::make_shared<DigitalEventSeries>(spike_times);
        dm.setData<DigitalEventSeries>("spike_events", spike_series, "master");
        
        // Verify data
        auto retrieved_spikes = dm.getData<DigitalEventSeries>("spike_events");
        REQUIRE(retrieved_spikes != nullptr);
        REQUIRE(retrieved_spikes->size() == 11);
        REQUIRE(dm.getTimeFrame("spike_events") == "master");
        
        // Test event retrieval in range
        auto events_in_range = retrieved_spikes->getEventsAsVector(1000.0f, 20000.0f);
        REQUIRE(events_in_range.size() == 7); // Events between 1000 and 20000
        
        // Test with time transformation (example: master to camera coordinate conversion)
        auto camera_transform = [&master_timeframe](float master_time) -> float {
            // Convert master time to camera frame index
            return std::floor((master_time - 1) / 300.0f);
        };
        
        auto events_camera_coords = retrieved_spikes->getEventsAsVector(
            0.0f, 99.0f, camera_transform);
        REQUIRE(events_camera_coords.size() == spike_times.size());
    }
    
    SECTION("Create digital interval series on camera timeframe") {
        DataManager dm;
        
        // Setup timeframes
        std::vector<int> master_times(30000);
        std::iota(master_times.begin(), master_times.end(), 1);
        auto master_timeframe = std::make_shared<TimeFrame>(master_times);
        
        std::vector<int> camera_times;
        for (int i = 0; i < 100; ++i) {
            camera_times.push_back(1 + i * 300);
        }
        auto camera_timeframe = std::make_shared<TimeFrame>(camera_times);
        
        REQUIRE(dm.setTime("master", master_timeframe));
        REQUIRE(dm.setTime("camera", camera_timeframe));
        
        // Create behavior intervals at camera resolution (frame indices)
        std::vector<Interval> behavior_intervals = {
            {5, 15},    // Frames 5-15: grooming behavior
            {25, 35},   // Frames 25-35: locomotion
            {50, 65},   // Frames 50-65: rearing
            {80, 90}    // Frames 80-90: exploration
        };
        
        auto interval_series = std::make_shared<DigitalIntervalSeries>(behavior_intervals);
        dm.setData<DigitalIntervalSeries>("behavior", interval_series, "camera");
        
        // Verify data
        auto retrieved_intervals = dm.getData<DigitalIntervalSeries>("behavior");
        REQUIRE(retrieved_intervals != nullptr);
        REQUIRE(retrieved_intervals->size() == 4);
        REQUIRE(dm.getTimeFrame("behavior") == "camera");
        
        // Test interval queries
        auto intervals_in_range = retrieved_intervals->getIntervalsAsVector<
            DigitalIntervalSeries::RangeMode::OVERLAPPING>(20, 70);
        REQUIRE(intervals_in_range.size() == 2); // Should find intervals [25,35] and [50,65]
        
        // Test with time transformation (camera frame to master time)
        auto master_transform = [&camera_timeframe](int64_t camera_frame) -> int64_t {
            // Convert camera frame index to master time
            return camera_timeframe->getTimeAtIndex(TimeFrameIndex(static_cast<int>(camera_frame)));
        };
        
        // Query intervals in master time coordinates
        auto intervals_master_coords = retrieved_intervals->getIntervalsInRange<
            DigitalIntervalSeries::RangeMode::OVERLAPPING>(
            1501, 20000, master_transform); // Master times 1501-20000
        
        // Convert to vector for testing
        std::vector<Interval> master_intervals{
            std::ranges::begin(intervals_master_coords), 
            std::ranges::end(intervals_master_coords)
        };
        
        // Should find intervals that overlap with this master time range
        REQUIRE(master_intervals.size() >= 1);
    }
    
    SECTION("Test interval data range queries with clipping") {
        DataManager dm;
        
        // Setup camera timeframe
        std::vector<int> camera_times;
        for (int i = 0; i < 100; ++i) {
            camera_times.push_back(1 + i * 300);
        }
        auto camera_timeframe = std::make_shared<TimeFrame>(camera_times);
        REQUIRE(dm.setTime("camera", camera_timeframe));
        
        // Create intervals that extend beyond query range
        std::vector<Interval> test_intervals = {
            {5, 25},    // Partially overlaps start of range
            {30, 40},   // Fully contained
            {45, 65},   // Fully contained
            {70, 85}    // Partially overlaps end of range
        };
        
        auto interval_series = std::make_shared<DigitalIntervalSeries>(test_intervals);
        dm.setData<DigitalIntervalSeries>("test_behavior", interval_series, "camera");
        
        auto retrieved_intervals = dm.getData<DigitalIntervalSeries>("test_behavior");
        
        // Test CONTAINED mode
        auto contained = retrieved_intervals->getIntervalsAsVector<
            DigitalIntervalSeries::RangeMode::CONTAINED>(28, 68);
        REQUIRE(contained.size() == 2); // Only intervals [30,40] and [45,65]
        
        // Test OVERLAPPING mode ERROR THIS IS ONLY 2 (??)
        auto overlapping = retrieved_intervals->getIntervalsAsVector<
            DigitalIntervalSeries::RangeMode::OVERLAPPING>(28, 68);
        REQUIRE(overlapping.size() == 2); // Only intervals [30,40] and [45,65]
        
        // Test CLIP mode
        auto clipped = retrieved_intervals->getIntervalsAsVector<
            DigitalIntervalSeries::RangeMode::CLIP>(10, 80);
        REQUIRE(clipped.size() == 4); // All intervals, but clipped to range boundaries
        
        // Verify clipping behavior
        bool found_clipped_start = false;
        bool found_clipped_end = false;
        for (auto const & interval : clipped) {
            if (interval.start >= 28) found_clipped_start = true;
            if (interval.end <= 68) found_clipped_end = true;
        }
        REQUIRE(found_clipped_start);
        REQUIRE(found_clipped_end);
    }
    
    SECTION("Test time coordinate conversion edge cases") {
        DataManager dm;
        
        // Create sparse camera timeframe (not every 300th sample)
        std::vector<int> sparse_camera_times = {1, 500, 1200, 2000, 3500, 5000};
        auto sparse_timeframe = std::make_shared<TimeFrame>(sparse_camera_times);
        REQUIRE(dm.setTime("sparse_camera", sparse_timeframe));
        
        auto retrieved_sparse = dm.getTime("sparse_camera");
        
        // Test boundary conditions
        REQUIRE(retrieved_sparse->getIndexAtTime(0) == 0);     // Before first time
        REQUIRE(retrieved_sparse->getIndexAtTime(1) == 0);     // Exact match
        REQUIRE(retrieved_sparse->getIndexAtTime(250) == 0);   // Closer to first
        REQUIRE(retrieved_sparse->getIndexAtTime(350) == 1);   // Closer to second
        REQUIRE(retrieved_sparse->getIndexAtTime(6000) == 5);  // After last time
        
        // Test exact matches
        REQUIRE(retrieved_sparse->getIndexAtTime(500) == 1);
        REQUIRE(retrieved_sparse->getIndexAtTime(1200) == 2);
        REQUIRE(retrieved_sparse->getIndexAtTime(5000) == 5);
        
        // Test time retrieval
        REQUIRE(retrieved_sparse->getTimeAtIndex(TimeFrameIndex(0)) == 1);
        REQUIRE(retrieved_sparse->getTimeAtIndex(TimeFrameIndex(3)) == 2000);
        REQUIRE(retrieved_sparse->getTimeAtIndex(TimeFrameIndex(5)) == 5000);
    }
    
    SECTION("Integration test: Full pipeline with coordinate transformations") {
        DataManager dm;
        
        // Setup complete multi-timeframe scenario
        std::vector<int> master_times(30000);
        std::iota(master_times.begin(), master_times.end(), 1);
        auto master_timeframe = std::make_shared<TimeFrame>(master_times);
        
        std::vector<int> camera_times;
        for (int i = 0; i < 100; ++i) {
            camera_times.push_back(1 + i * 300);
        }
        auto camera_timeframe = std::make_shared<TimeFrame>(camera_times);
        
        REQUIRE(dm.setTime("master", master_timeframe));
        REQUIRE(dm.setTime("camera", camera_timeframe));
        
        // Create synchronized data across timeframes
        
        // 1. Neural signal at master rate
        std::vector<float> neural_signal(30000);
        for (size_t i = 0; i < neural_signal.size(); ++i) {
            neural_signal[i] = std::sin(2.0f * M_PI * i / 3000.0f); // 10 Hz sine wave
        }
        std::vector<TimeFrameIndex> neural_indices;
        for (size_t i = 0; i < neural_signal.size(); ++i) {
            neural_indices.push_back(TimeFrameIndex(i));
        }
        auto neural_series = std::make_shared<AnalogTimeSeries>(neural_signal, neural_indices);
        dm.setData<AnalogTimeSeries>("neural", neural_series, "master");
        
        // 2. Spike events at master rate
        std::vector<float> spike_times;
        for (int i = 1000; i < 29000; i += 2000) {
            spike_times.push_back(static_cast<float>(i));
        }
        auto spike_series = std::make_shared<DigitalEventSeries>(spike_times);
        dm.setData<DigitalEventSeries>("spikes", spike_series, "master");
        
        // 3. Behavior intervals at camera rate
        std::vector<Interval> behavior_intervals = {{10, 20}, {40, 60}, {80, 95}};
        auto behavior_series = std::make_shared<DigitalIntervalSeries>(behavior_intervals);
        dm.setData<DigitalIntervalSeries>("behavior", behavior_series, "camera");
        
        // Test cross-timeframe queries
        
        // Query neural data during behavior intervals (need coordinate conversion)
        auto behavior_data = dm.getData<DigitalIntervalSeries>("behavior");
        auto neural_data = dm.getData<AnalogTimeSeries>("neural");
        
        // Convert first behavior interval (camera frames 10-20) to master times
        int64_t behavior_start_master = camera_timeframe->getTimeAtIndex(TimeFrameIndex(10)); // Should be 3001
        int64_t behavior_end_master = camera_timeframe->getTimeAtIndex(TimeFrameIndex(20));   // Should be 6001

        REQUIRE(behavior_start_master == 3001);
        REQUIRE(behavior_end_master == 6001);
        
        // Extract neural signal during this behavior
        auto neural_times = neural_data->getTimeSeries();
        auto neural_values = neural_data->getAnalogTimeSeries();
        
        std::vector<float> neural_during_behavior;
        for (size_t i = 0; i < neural_times.size(); ++i) {
            int neural_time = master_timeframe->getTimeAtIndex(neural_times[i]);
            if (neural_time >= behavior_start_master && neural_time <= behavior_end_master) {
                neural_during_behavior.push_back(neural_values[i]);
            }
        }
        
        REQUIRE(neural_during_behavior.size() > 0);
        REQUIRE(neural_during_behavior.size() <= 3001); // Should be around 3000 samples
        
        // Verify we captured the expected portion of the sine wave
        REQUIRE_THAT(neural_during_behavior.front(), 
                    Catch::Matchers::WithinAbs(neural_signal[3000], 0.1f));
    }
}