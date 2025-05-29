#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "OpenGLWidget.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/TimeFrame.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <QApplication>
#include <QTest>
#include <memory>

// Note: These tests require Qt application context for OpenGLWidget functionality
// Run with: your_test_runner --qt-app

TEST_CASE("OpenGLWidget - Time Frame Conversion for Interval Dragging", "[OpenGLWidget][timeframe][integration]") {
    
    SECTION("Setup multi-timeframe scenario with interval dragging") {
        DataManager dm;
        
        // Create master clock: 30000 samples (1 to 30000) - high frequency electrophysiology
        std::vector<int> master_times(30000);
        std::iota(master_times.begin(), master_times.end(), 1);
        auto master_timeframe = std::make_shared<TimeFrame>(master_times);
        
        // Create camera clock: samples every 300 ticks of master clock - 100Hz video
        // This gives us 100 camera frames (30000 / 300 = 100)
        std::vector<int> camera_times;
        for (int i = 0; i < 100; ++i) {
            camera_times.push_back(1 + i * 300); // 1, 301, 601, 901, ...
        }
        auto camera_timeframe = std::make_shared<TimeFrame>(camera_times);
        
        // Register timeframes with DataManager
        REQUIRE(dm.setTime("master", master_timeframe));
        REQUIRE(dm.setTime("camera", camera_timeframe));
        
        // Create behavior intervals at camera resolution (frame indices)
        std::vector<Interval> behavior_intervals = {
            {10, 20},   // Camera frames 10-20: grooming behavior
            {30, 40},   // Camera frames 30-40: locomotion  
            {60, 70}    // Camera frames 60-70: rearing
        };
        auto interval_series = std::make_shared<DigitalIntervalSeries>(behavior_intervals);
        dm.setData<DigitalIntervalSeries>("behavior", interval_series, "camera");
        
        // Create OpenGL widget and set up time frames
        OpenGLWidget widget;
        widget.setMasterTimeFrame(master_timeframe);
        widget.addDigitalIntervalSeries("behavior", interval_series, camera_timeframe, "#00FF00");
        
        // Test findIntervalAtTime with time frame conversion
        SECTION("findIntervalAtTime handles time frame conversion correctly") {
            // Query in master time coordinates
            float master_time_coord = 3001.0f; // Should correspond to camera frame 10
            auto found_interval = widget.findIntervalAtTime("behavior", master_time_coord);
            
            REQUIRE(found_interval.has_value());
            auto [start_master, end_master] = found_interval.value();
            
            // Verify the returned interval is in master time coordinates
            REQUIRE(start_master == 3001); // Camera frame 10 -> master time 3001
            REQUIRE(end_master == 6001);   // Camera frame 20 -> master time 6001
            
            // Test edge case: query at exact camera frame boundary
            float boundary_time = 6001.0f; // Exact end of first interval
            auto boundary_interval = widget.findIntervalAtTime("behavior", boundary_time);
            REQUIRE(boundary_interval.has_value());
            
            // Test no interval found
            float empty_time = 1500.0f; // Between camera frames, no interval
            auto empty_result = widget.findIntervalAtTime("behavior", empty_time);
            REQUIRE_FALSE(empty_result.has_value());
        }
        
        SECTION("Interval dragging with different time frames") {
            // First, select an interval in master time coordinates
            widget.setSelectedInterval("behavior", 3001, 6001); // Camera frames 10-20
            
            // Simulate starting a drag operation
            QPoint start_pos(100, 100);
            widget.startIntervalDrag("behavior", true, start_pos); // Drag left edge
            
            REQUIRE(widget.isDraggingInterval());
            
            // Simulate dragging to a new position
            // Move to master time 2701 (should snap to camera frame 9)
            QPoint new_pos(80, 100); // This would correspond to earlier time
            
            // We can't easily test the full drag without mocking canvasXToTime,
            // but we can verify the drag state is properly managed
            widget.cancelIntervalDrag();
            REQUIRE_FALSE(widget.isDraggingInterval());
        }
        
        SECTION("Time frame conversion edge cases") {
            // Test conversion at time frame boundaries
            float start_time = 1.0f; // First sample in master
            auto result_start = widget.findIntervalAtTime("behavior", start_time);
            REQUIRE_FALSE(result_start.has_value()); // No interval at start
            
            float end_time = 29701.0f; // Last camera frame in master time
            auto result_end = widget.findIntervalAtTime("behavior", end_time);
            REQUIRE_FALSE(result_end.has_value()); // No interval at end
            
            // Test time beyond available data
            float beyond_time = 35000.0f; // Beyond master time frame
            auto result_beyond = widget.findIntervalAtTime("behavior", beyond_time);
            REQUIRE_FALSE(result_beyond.has_value());
        }
    }
    
    SECTION("Time frame conversion precision and rounding") {
        DataManager dm;
        
        // Create a coarse time frame with irregular spacing
        std::vector<int> master_times = {1, 5, 15, 25, 50, 100, 200};
        auto master_timeframe = std::make_shared<TimeFrame>(master_times);
        
        std::vector<int> coarse_times = {1, 25, 100}; // Very coarse sampling
        auto coarse_timeframe = std::make_shared<TimeFrame>(coarse_times);
        
        REQUIRE(dm.setTime("master", master_timeframe));
        REQUIRE(dm.setTime("coarse", coarse_timeframe));
        
        // Create interval at coarse resolution
        std::vector<Interval> coarse_intervals = {{0, 1}}; // Coarse indices 0-1
        auto interval_series = std::make_shared<DigitalIntervalSeries>(coarse_intervals);
        dm.setData<DigitalIntervalSeries>("test_intervals", interval_series, "coarse");
        
        OpenGLWidget widget;
        widget.setMasterTimeFrame(master_timeframe);
        widget.addDigitalIntervalSeries("test_intervals", interval_series, coarse_timeframe, "#FF0000");
        
        // Test rounding behavior
        float query_time = 10.0f; // Should round to coarse index 0 (time 1)
        auto found = widget.findIntervalAtTime("test_intervals", query_time);
        REQUIRE(found.has_value());
        
        auto [start, end] = found.value();
        REQUIRE(start == 1);   // Coarse index 0 -> time 1
        REQUIRE(end == 25);    // Coarse index 1 -> time 25
    }
    
    SECTION("Error handling during interval operations") {
        DataManager dm;
        OpenGLWidget widget;
        
        // Test operations on non-existent series
        auto result = widget.findIntervalAtTime("nonexistent", 100.0f);
        REQUIRE_FALSE(result.has_value());
        
        // Test drag operations on non-existent series
        QPoint pos(100, 100);
        widget.startIntervalDrag("nonexistent", true, pos);
        REQUIRE_FALSE(widget.isDraggingInterval()); // Should not start dragging
        
        // Test with null time frame (this would be a programming error but should be handled gracefully)
        std::vector<Interval> intervals = {{10, 20}};
        auto interval_series = std::make_shared<DigitalIntervalSeries>(intervals);
        
        // This should not crash even if time frame setup is incomplete
        widget.addDigitalIntervalSeries("test", interval_series, nullptr, "#0000FF");
        auto null_result = widget.findIntervalAtTime("test", 15.0f);
        // Behavior with null time frame is implementation-defined, but should not crash
    }
}

TEST_CASE("OpenGLWidget - Time Frame Conversion Consistency", "[OpenGLWidget][timeframe][unit]") {
    
    SECTION("Round-trip time frame conversions") {
        // Create test time frames
        std::vector<int> master_times(1000);
        std::iota(master_times.begin(), master_times.end(), 1);
        auto master_tf = std::make_shared<TimeFrame>(master_times);
        
        std::vector<int> sparse_times;
        for (int i = 0; i < 100; ++i) {
            sparse_times.push_back(1 + i * 10); // Every 10th sample
        }
        auto sparse_tf = std::make_shared<TimeFrame>(sparse_times);
        
        // Test conversion consistency
        for (int master_time : {1, 50, 500, 999}) {
            // Convert master time to sparse index
            int sparse_index = sparse_tf->getIndexAtTime(static_cast<float>(master_time));
            
            // Convert back to time
            int recovered_time = sparse_tf->getTimeAtIndex(sparse_index);
            
            // Should be close (within sparse sampling resolution)
            REQUIRE(std::abs(recovered_time - master_time) <= 10);
        }
    }
} 