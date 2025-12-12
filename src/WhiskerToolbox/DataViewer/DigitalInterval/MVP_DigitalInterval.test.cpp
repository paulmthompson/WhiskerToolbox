#include "MVP_DigitalInterval.hpp"

#include "DigitalIntervalSeriesDisplayOptions.hpp"
#include "PlottingManager/PlottingManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include "../../tests/WhiskerToolbox/DataViewer_Widget/fixtures/builders/DigitalIntervalBuilder.hpp"
#include "../../tests/WhiskerToolbox/DataViewer_Widget/fixtures/scenarios/digital_interval_scenarios.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <iostream>
#include <vector>

using Catch::Matchers::WithinRel;

/**
 * @brief Apply MVP transformation to an interval corner point
 * 
 * Applies Model, View, and Projection matrices to transform an interval corner
 * from data coordinates to normalized device coordinates. For intervals, we
 * typically check corners of the rectangle.
 * 
 * @param time_value Time value (start or end time of interval)
 * @param y_value Y value (top or bottom of interval rectangle)
 * @param model Model transformation matrix
 * @param view View transformation matrix
 * @param projection Projection transformation matrix
 * @return Transformed point in normalized device coordinates
 */
glm::vec2 applyIntervalMVPTransformation(float time_value, float y_value,
                                         glm::mat4 const & model,
                                         glm::mat4 const & view,
                                         glm::mat4 const & projection) {
    // Create homogeneous coordinate
    glm::vec4 point(time_value, y_value, 0.0f, 1.0f);

    // Apply transformations in order: Model -> View -> Projection
    glm::vec4 model_point = model * point;
    glm::vec4 view_point = view * model_point;
    glm::vec4 clip_point = projection * view_point;

    // Convert from homogeneous coordinates to 2D
    return glm::vec2(clip_point.x / clip_point.w, clip_point.y / clip_point.w);
}

TEST_CASE("New Digital Interval MVP System - Happy Path Tests", "[mvp][digital_interval][new]") {

    SECTION("Interval basic functionality") {
        // Test basic interval construction and validation
        Interval interval1{100, 200};
        REQUIRE(interval1.start == 100);
        REQUIRE(interval1.end == 200);
        REQUIRE(interval1.end > interval1.start);

        // Test invalid interval (end <= start)
        Interval interval2{200, 100};
        REQUIRE(interval2.start == 200);
        REQUIRE(interval2.end == 100);
        REQUIRE(interval2.end <= interval2.start);

        Interval interval3{100, 100};
        REQUIRE(interval3.start == 100);
        REQUIRE(interval3.end == 100);

        Interval interval4{};
        REQUIRE(interval4.start == 0);
        REQUIRE(interval4.end == 0);
    }

    SECTION("Generate test interval data") {
        // Generate test intervals and validate properties
        auto intervals = DigitalIntervalBuilder()
            .withRandomIntervals(50, 10000.0f, 50.0f, 500.0f, 42)
            .build();

        REQUIRE(intervals.size() == 50);

        // Verify all intervals are valid
        for (auto const & interval: intervals) {
            REQUIRE(interval.end > interval.start);
            REQUIRE(interval.start >= 0);
            REQUIRE(interval.end <= 10000);
            int64_t duration = interval.end - interval.start;
            REQUIRE(duration >= 50);
            REQUIRE(duration <= 500);
        }

        // Verify intervals are ordered by start time
        for (size_t i = 1; i < intervals.size(); ++i) {
            REQUIRE(intervals[i].start >= intervals[i - 1].start);
        }
    }

    SECTION("PlottingManager with digital intervals") {
        PlottingManager manager;

        // Test adding digital interval series
        std::vector<Interval> intervals;
        auto series = std::make_shared<DigitalIntervalSeries>(intervals);
        std::vector<int> times(100);
        for(int i=0; i<100; ++i) times[i] = i;
        auto time_frame = std::make_shared<TimeFrame>(times);

        series->setTimeFrame(time_frame);

        manager.total_digital_series = 2;
        int series1_idx = 0;
        int series2_idx = 1;

        REQUIRE(series1_idx == 0);
        REQUIRE(series2_idx == 1);
        REQUIRE(manager.total_digital_series == 2);

        // Test digital interval allocation (should use full canvas)
        float center1, height1, center2, height2;
        manager.calculateDigitalIntervalSeriesAllocation(0, center1, height1);
        manager.calculateDigitalIntervalSeriesAllocation(1, center2, height2);

        // Both should get full canvas height
        REQUIRE_THAT(height1, WithinRel(2.0f, 0.01f));// Full viewport: 1.0 - (-1.0) = 2.0
        REQUIRE_THAT(height2, WithinRel(2.0f, 0.01f));

        // Both should be centered at viewport center
        REQUIRE_THAT(center1, Catch::Matchers::WithinAbs(0.0f, 0.01f));// (-1.0 + 1.0) / 2 = 0.0
        REQUIRE_THAT(center2, Catch::Matchers::WithinAbs(0.0f, 0.01f));
    }

    SECTION("Digital interval display options configuration") {
        // Test default options
        NewDigitalIntervalSeriesDisplayOptions options;
        REQUIRE(options.style.hex_color == "#ff6b6b");
        REQUIRE(options.style.alpha == 0.3f);
        REQUIRE(options.style.is_visible);
        REQUIRE(options.extend_full_canvas);
        REQUIRE(options.layout.allocated_height == 2.0f);
        REQUIRE(options.margin_factor == 0.95f);

        // Test intrinsic properties setting
        auto intervals = DigitalIntervalBuilder()
            .withRandomIntervals(100, 10000.0f, 50.0f, 500.0f, 42)
            .build();
        auto updated_options = DigitalIntervalDisplayOptionsBuilder()
            .withIntrinsicProperties(intervals)
            .build();

        // Should adjust alpha for many intervals
        REQUIRE(updated_options.style.alpha < 0.3f);// Should be reduced due to large number of intervals
    }

    SECTION("Single interval series MVP transformation - Gold Standard Test") {
        // Generate test intervals between 0 and 10000
        auto intervals = DigitalIntervalBuilder()
            .withRandomIntervals(20, 10000.0f, 100.0f, 300.0f, 42)
            .build();
        REQUIRE(!intervals.empty());

        // Set up plotting manager for single digital interval series
        PlottingManager manager;
        auto series = std::make_shared<DigitalIntervalSeries>(intervals);
        std::vector<int> times(100);
        for(int i=0; i<100; ++i) times[i] = i;
        auto time_frame = std::make_shared<TimeFrame>(times);
        series->setTimeFrame(time_frame);
        manager.total_digital_series = 1;
        int series_idx = 0;

        // Set up display options
        NewDigitalIntervalSeriesDisplayOptions display_options;

        // Calculate series allocation (should be full canvas)
        float allocated_center, allocated_height;
        manager.calculateDigitalIntervalSeriesAllocation(series_idx, allocated_center, allocated_height);
        display_options.layout.allocated_y_center = allocated_center;
        display_options.layout.allocated_height = allocated_height;

        // Set intrinsic properties
        display_options = DigitalIntervalDisplayOptionsBuilder()
            .withIntrinsicProperties(intervals)
            .build();
        display_options.layout.allocated_y_center = allocated_center;
        display_options.layout.allocated_height = allocated_height;

        // Generate MVP matrices
        glm::mat4 model = new_getIntervalModelMat(display_options, manager);
        glm::mat4 view = new_getIntervalViewMat(manager);
        glm::mat4 projection = new_getIntervalProjectionMat(1, 10000, -1.0f, 1.0f, manager);

        // Test interval rectangle corners
        // For an interval from time 1000 to 1500, test the four corners of the rectangle

        // Bottom-left corner: (start_time, -1) should map to appropriate NDC
        glm::vec2 bottom_left = applyIntervalMVPTransformation(1000.0f, -1.0f, model, view, projection);

        // Expected X: (1000 - 1) / (10000 - 1) * 2 - 1 ≈ (999/9999) * 2 - 1 ≈ -0.8
        float expected_x_start = (1000.0f - 1.0f) / (10000.0f - 1.0f) * 2.0f - 1.0f;
        REQUIRE_THAT(bottom_left.x, WithinRel(expected_x_start, 0.01f));

        // Expected Y: Should be at bottom of visible range after scaling
        // With full canvas and margin factor 0.95, and allocated_center = 0:
        // y_out = (-1.0) * (2.0 * 0.95 * 0.5) + 0.0 = -0.95
        float expected_y_bottom = -1.0f * (allocated_height * display_options.margin_factor * 0.5f) + allocated_center;
        REQUIRE_THAT(bottom_left.y, WithinRel(expected_y_bottom, 0.01f));

        // Top-right corner: (end_time, +1) should map to appropriate NDC
        glm::vec2 top_right = applyIntervalMVPTransformation(1500.0f, 1.0f, model, view, projection);

        // Expected X for end time
        float expected_x_end = (1500.0f - 1.0f) / (10000.0f - 1.0f) * 2.0f - 1.0f;
        REQUIRE_THAT(top_right.x, WithinRel(expected_x_end, 0.01f));

        // Expected Y: Should be at top of visible range
        float expected_y_top = 1.0f * (allocated_height * display_options.margin_factor * 0.5f) + allocated_center;
        REQUIRE_THAT(top_right.y, WithinRel(expected_y_top, 0.01f));

        // Test interval at left edge (time 1)
        glm::vec2 left_edge = applyIntervalMVPTransformation(1.0f, 0.0f, model, view, projection);
        REQUIRE_THAT(left_edge.x, WithinRel(-1.0f, 0.01f));                            // Left edge of NDC
        REQUIRE_THAT(left_edge.y, Catch::Matchers::WithinAbs(allocated_center, 0.02f));// Center Y

        // Test interval at right edge (time 10000)
        glm::vec2 right_edge = applyIntervalMVPTransformation(10000.0f, 0.0f, model, view, projection);
        REQUIRE_THAT(right_edge.x, WithinRel(1.0f, 0.01f));                             // Right edge of NDC
        REQUIRE_THAT(right_edge.y, Catch::Matchers::WithinAbs(allocated_center, 0.02f));// Center Y
    }

    SECTION("Multiple interval series with analog series integration") {
        PlottingManager manager;

        // Add mixed series types
        std::vector<int> times(100);
        for(int i=0; i<100; ++i) times[i] = i;
        auto time_frame = std::make_shared<TimeFrame>(times);
        
        std::vector<float> data(100, 0.0f);
        std::vector<TimeFrameIndex> time_indices;
        for(int i=0; i<100; ++i) time_indices.emplace_back(i);
        auto series = std::make_shared<AnalogTimeSeries>(data, time_indices);

        series->setTimeFrame(time_frame);

        manager.total_analog_series = 1;
        int analog_series = 0;
        
        std::vector<Interval> intervals;
        auto interval_series = std::make_shared<DigitalIntervalSeries>(intervals);

        interval_series->setTimeFrame(time_frame);
        manager.total_digital_series = 2;
        int interval_series1 = 0;
        int interval_series2 = 1;

        // Verify series counts
        REQUIRE(manager.total_analog_series == 1);
        REQUIRE(manager.total_digital_series == 2);

        // Check that analog and digital allocations are independent
        float analog_center, analog_height;
        float interval1_center, interval1_height;
        float interval2_center, interval2_height;

        manager.calculateAnalogSeriesAllocation(analog_series, analog_center, analog_height);
        manager.calculateDigitalIntervalSeriesAllocation(interval_series1, interval1_center, interval1_height);
        manager.calculateDigitalIntervalSeriesAllocation(interval_series2, interval2_center, interval2_height);

        // Analog series should use full canvas (single analog series)
        REQUIRE_THAT(analog_height, WithinRel(2.0f, 0.01f));
        REQUIRE_THAT(analog_center, Catch::Matchers::WithinAbs(0.0f, 0.01f));

        // Digital intervals should both use full canvas (overlapping is expected)
        REQUIRE_THAT(interval1_height, WithinRel(2.0f, 0.01f));
        REQUIRE_THAT(interval2_height, WithinRel(2.0f, 0.01f));
        REQUIRE_THAT(interval1_center, Catch::Matchers::WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(interval2_center, Catch::Matchers::WithinAbs(0.0f, 0.01f));
    }

    SECTION("Panning integration with digital intervals") {
        PlottingManager manager;
        std::vector<Interval> intervals;
        auto series = std::make_shared<DigitalIntervalSeries>(intervals);
        std::vector<int> times(100);
        for(int i=0; i<100; ++i) times[i] = i;
        auto time_frame = std::make_shared<TimeFrame>(times);
        series->setTimeFrame(time_frame);
        manager.total_digital_series = 1;
        int series_idx = 0;

        // Test initial state (no panning)
        REQUIRE(manager.getPanOffset() == 0.0f);

        // Apply vertical pan
        float pan_offset = 0.5f;
        manager.setPanOffset(pan_offset);
        REQUIRE(manager.getPanOffset() == pan_offset);

        // Generate MVP matrices with panning
        NewDigitalIntervalSeriesDisplayOptions display_options;
        float allocated_center, allocated_height;
        manager.calculateDigitalIntervalSeriesAllocation(series_idx, allocated_center, allocated_height);
        display_options.layout.allocated_y_center = allocated_center;
        display_options.layout.allocated_height = allocated_height;

        glm::mat4 model = new_getIntervalModelMat(display_options, manager);
        glm::mat4 view = new_getIntervalViewMat(manager);
        glm::mat4 projection = new_getIntervalProjectionMat(1, 10000, -1.0f, 1.0f, manager);

        // Test point transformation with panning
        glm::vec2 center_point = applyIntervalMVPTransformation(5000.0f, 0.0f, model, view, projection);

        // Y coordinate should NOT be shifted by pan offset for digital intervals
        // Digital intervals should remain pinned to the current viewport
        float expected_y = allocated_center;// Should remain at original position
        REQUIRE_THAT(center_point.y, WithinRel(expected_y, 0.01f));

        // X coordinate should be unaffected by vertical panning
        float expected_x = (5000.0f - 1.0f) / (10000.0f - 1.0f) * 2.0f - 1.0f;// Should be ~0
        REQUIRE_THAT(center_point.x, WithinRel(expected_x, 0.01f));

        // Test pan delta functionality
        manager.applyPanDelta(0.2f);
        REQUIRE_THAT(manager.getPanOffset(), WithinRel(0.7f, 0.01f));

        // Reset panning
        manager.resetPan();
        REQUIRE(manager.getPanOffset() == 0.0f);
    }

    SECTION("Digital intervals always extend full view height during panning - CORRECTED BEHAVIOR") {
        PlottingManager manager;
        std::vector<Interval> intervals;
        auto series = std::make_shared<DigitalIntervalSeries>(intervals);
        std::vector<int> times(100);
        for(int i=0; i<100; ++i) times[i] = i;
        auto time_frame = std::make_shared<TimeFrame>(times);
        series->setTimeFrame(time_frame);
        manager.total_digital_series = 1;
        int series_idx = 0;

        // Set up display options
        NewDigitalIntervalSeriesDisplayOptions display_options;
        float allocated_center, allocated_height;
        manager.calculateDigitalIntervalSeriesAllocation(series_idx, allocated_center, allocated_height);
        display_options.layout.allocated_y_center = allocated_center;
        display_options.layout.allocated_height = allocated_height;

        // Test with no panning first (baseline)
        {
            glm::mat4 model = new_getIntervalModelMat(display_options, manager);
            glm::mat4 view = new_getIntervalViewMat(manager);
            glm::mat4 projection = new_getIntervalProjectionMat(1, 10000, -1.0f, 1.0f, manager);

            // Test top and bottom corners of an interval
            glm::vec2 bottom_corner = applyIntervalMVPTransformation(5000.0f, -1.0f, model, view, projection);
            glm::vec2 top_corner = applyIntervalMVPTransformation(5000.0f, 1.0f, model, view, projection);

            // Should extend from -0.95 to +0.95 (with margin factor)
            float expected_bottom = -1.0f * (allocated_height * display_options.margin_factor * 0.5f);
            float expected_top = 1.0f * (allocated_height * display_options.margin_factor * 0.5f);

            REQUIRE_THAT(bottom_corner.y, WithinRel(expected_bottom, 0.01f));
            REQUIRE_THAT(top_corner.y, WithinRel(expected_top, 0.01f));
        }

        // Apply significant vertical pan (1.0 units upward)
        float pan_offset = 1.0f;
        manager.setPanOffset(pan_offset);

        // Generate MVP matrices with panning
        {
            glm::mat4 model = new_getIntervalModelMat(display_options, manager);
            glm::mat4 view = new_getIntervalViewMat(manager);
            glm::mat4 projection = new_getIntervalProjectionMat(1, 10000, -1.0f, 1.0f, manager);

            // CRITICAL TEST: Digital intervals should STILL extend from top to bottom of current view
            // Unlike analog series, they should NOT move with the pan offset
            glm::vec2 bottom_corner_panned = applyIntervalMVPTransformation(5000.0f, -1.0f, model, view, projection);
            glm::vec2 top_corner_panned = applyIntervalMVPTransformation(5000.0f, 1.0f, model, view, projection);

            // Intervals should still extend the same relative amount in the current view
            // They should NOT be shifted by the pan offset like analog series would be
            float expected_bottom_panned = -1.0f * (allocated_height * display_options.margin_factor * 0.5f);
            float expected_top_panned = 1.0f * (allocated_height * display_options.margin_factor * 0.5f);

            REQUIRE_THAT(bottom_corner_panned.y, WithinRel(expected_bottom_panned, 0.01f));
            REQUIRE_THAT(top_corner_panned.y, WithinRel(expected_top_panned, 0.01f));

            // X coordinates should be unaffected by vertical panning
            float expected_x = (5000.0f - 1.0f) / (10000.0f - 1.0f) * 2.0f - 1.0f;// Should be ~0
            REQUIRE_THAT(bottom_corner_panned.x, WithinRel(expected_x, 0.01f));
            REQUIRE_THAT(top_corner_panned.x, WithinRel(expected_x, 0.01f));
        }

        // Test with negative pan as well
        manager.setPanOffset(-1.0f);
        {
            glm::mat4 model = new_getIntervalModelMat(display_options, manager);
            glm::mat4 view = new_getIntervalViewMat(manager);
            glm::mat4 projection = new_getIntervalProjectionMat(1, 10000, -1.0f, 1.0f, manager);

            glm::vec2 bottom_corner_neg_pan = applyIntervalMVPTransformation(5000.0f, -1.0f, model, view, projection);
            glm::vec2 top_corner_neg_pan = applyIntervalMVPTransformation(5000.0f, 1.0f, model, view, projection);

            // Should still extend the same amount regardless of pan direction
            float expected_bottom_neg_pan = -1.0f * (allocated_height * display_options.margin_factor * 0.5f);
            float expected_top_neg_pan = 1.0f * (allocated_height * display_options.margin_factor * 0.5f);

            REQUIRE_THAT(bottom_corner_neg_pan.y, WithinRel(expected_bottom_neg_pan, 0.01f));
            REQUIRE_THAT(top_corner_neg_pan.y, WithinRel(expected_top_neg_pan, 0.01f));
        }
    }
}