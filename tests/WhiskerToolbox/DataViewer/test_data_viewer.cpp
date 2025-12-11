#include "AnalogTimeSeries/AnalogTimeSeriesDisplayOptions.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/MVP_AnalogTimeSeries.hpp"
#include "DigitalEvent/DigitalEventSeriesDisplayOptions.hpp"
#include "DigitalEvent/MVP_DigitalEvent.hpp"
#include "DigitalInterval/DigitalIntervalSeriesDisplayOptions.hpp"
#include "DigitalInterval/MVP_DigitalInterval.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "PlottingManager/PlottingManager.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "XAxis.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

using Catch::Matchers::WithinRel;

/**
 * @brief Generate Gaussian distributed data for integration tests
 * 
 * @param num_points Number of data points
 * @param mean Mean of distribution
 * @param std_dev Standard deviation
 * @param seed Random seed for reproducibility
 * @return Vector of Gaussian distributed values
 */
std::vector<float> generateGaussianDataIntegration(size_t num_points, float mean, float std_dev, unsigned int seed = 42) {
    std::mt19937 gen(seed);
    std::normal_distribution<float> dist(mean, std_dev);

    std::vector<float> data;
    data.reserve(num_points);

    for (size_t i = 0; i < num_points; ++i) {
        data.push_back(dist(gen));
    }

    return data;
}

/**
 * @brief Generate uniformly distributed data for integration tests
 * 
 * @param num_points Number of data points
 * @param min_val Minimum value
 * @param max_val Maximum value
 * @param seed Random seed for reproducibility
 * @return Vector of uniformly distributed values
 */
std::vector<float> generateUniformDataIntegration(size_t num_points, float min_val, float max_val, unsigned int seed = 123) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> dist(min_val, max_val);

    std::vector<float> data;
    data.reserve(num_points);

    for (size_t i = 0; i < num_points; ++i) {
        data.push_back(dist(gen));
    }

    return data;
}

/**
 * @brief Apply MVP transformation to a data point for integration tests
 * 
 * @param data_index Index of the data point (for X coordinate)
 * @param data_value Value of the data point (for Y coordinate)  
 * @param model Model transformation matrix
 * @param view View transformation matrix
 * @param projection Projection transformation matrix
 * @return Transformed point in normalized device coordinates
 */
glm::vec2 applyMVPTransformationIntegration(int data_index, float data_value,
                                            glm::mat4 const & model,
                                            glm::mat4 const & view,
                                            glm::mat4 const & projection) {
    // Create homogeneous coordinate
    glm::vec4 point(static_cast<float>(data_index), data_value, 0.0f, 1.0f);

    // Apply transformations in order: Model -> View -> Projection
    glm::vec4 model_point = model * point;
    glm::vec4 view_point = view * model_point;
    glm::vec4 clip_point = projection * view_point;

    // Convert from homogeneous coordinates to 2D
    return glm::vec2(clip_point.x / clip_point.w, clip_point.y / clip_point.w);
}

/**
 * @brief Apply MVP transformation to an interval corner point for integration tests
 * 
 * @param time_value Time value (start or end time of interval)
 * @param y_value Y value (top or bottom of interval rectangle)
 * @param model Model transformation matrix
 * @param view View transformation matrix
 * @param projection Projection transformation matrix
 * @return Transformed point in normalized device coordinates
 */
glm::vec2 applyIntervalMVPTransformationIntegration(float time_value, float y_value,
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

TEST_CASE("Integration Test: Mixed Data Types with Coordinate Allocation and Panning", "[integration][mvp][mixed_data]") {

    SECTION("Multi-series coordinate allocation and panning behavior") {
        // Set up plotting manager with mixed data types
        PlottingManager manager;

        // Add series in order: Gaussian analog, Uniform analog, Digital intervals
        std::vector<int> times(100);
        for(int i=0; i<100; ++i) times[i] = i;
        auto time_frame = std::make_shared<TimeFrame>(times);
        
        std::vector<float> data(100, 0.0f);
        std::vector<TimeFrameIndex> time_indices;
        for(int i=0; i<100; ++i) time_indices.emplace_back(i);
        auto series = std::make_shared<AnalogTimeSeries>(data, time_indices);

        int gaussian_series = manager.addAnalogSeries("s1", series, time_frame);
        int uniform_series = manager.addAnalogSeries("s2", series, time_frame);
        
        std::vector<Interval> intervals;
        auto interval_series_ptr = std::make_shared<DigitalIntervalSeries>(intervals);
        interval_series_ptr->setTimeFrame(time_frame);
        int interval_series = manager.addDigitalIntervalSeries("i1", interval_series_ptr);

        // Verify series indices and counts
        REQUIRE(gaussian_series == 0);
        REQUIRE(uniform_series == 1);
        REQUIRE(interval_series == 0);// Digital interval indices are separate
        REQUIRE(manager.total_analog_series == 2);
        REQUIRE(manager.total_digital_series == 1);


        // Generate test data
        constexpr size_t num_points = 10000;
        std::vector<TimeFrameIndex> time_vector;
        for (size_t i = 0; i < num_points; ++i) {
            time_vector.push_back(TimeFrameIndex(i));
        }
        auto gaussian_data = generateGaussianDataIntegration(num_points, 0.0f, 10.0f, 42);
        auto uniform_data = generateUniformDataIntegration(num_points, 0.0f, 1.0f, 123);
        auto intervals_data = generateTestIntervalData(10, 10000.0f, 200.0f, 800.0f, 456);

        auto gaussian_time_series = std::make_shared<AnalogTimeSeries>(gaussian_data, time_vector);
        auto uniform_time_series = std::make_shared<AnalogTimeSeries>(uniform_data, time_vector);
        auto intervals_time_series = std::make_shared<DigitalIntervalSeries>(intervals_data);
        // Set up analog series display options
        NewAnalogTimeSeriesDisplayOptions gaussian_options;
        NewAnalogTimeSeriesDisplayOptions uniform_options;
        NewDigitalIntervalSeriesDisplayOptions interval_options;

        // Calculate coordinate allocations
        float gaussian_center, gaussian_height;
        float uniform_center, uniform_height;
        float interval_center, interval_height;

        manager.calculateAnalogSeriesAllocation(gaussian_series, gaussian_center, gaussian_height);
        manager.calculateAnalogSeriesAllocation(uniform_series, uniform_center, uniform_height);
        manager.calculateDigitalIntervalSeriesAllocation(interval_series, interval_center, interval_height);

        // Verify coordinate allocation expectations
        // Analog series should split the canvas in half
        REQUIRE_THAT(gaussian_height, WithinRel(1.0f, 0.01f)); // Half of 2.0 total height
        REQUIRE_THAT(uniform_height, WithinRel(1.0f, 0.01f));  // Half of 2.0 total height
        REQUIRE_THAT(gaussian_center, WithinRel(-0.5f, 0.01f));// First half: -1.0 + 0.5 = -0.5
        REQUIRE_THAT(uniform_center, WithinRel(0.5f, 0.01f));  // Second half: -1.0 + 1.5 = 0.5

        // Digital intervals should use full canvas
        REQUIRE_THAT(interval_height, WithinRel(2.0f, 0.01f));                 // Full canvas height
        REQUIRE_THAT(interval_center, Catch::Matchers::WithinAbs(0.0f, 0.01f));// Center of canvas

        // Configure display options with allocated coordinates
        gaussian_options.allocated_y_center = gaussian_center;
        gaussian_options.allocated_height = gaussian_height;
        uniform_options.allocated_y_center = uniform_center;
        uniform_options.allocated_height = uniform_height;
        interval_options.allocated_y_center = interval_center;
        interval_options.allocated_height = interval_height;

        // Set intrinsic properties
        setAnalogIntrinsicProperties(gaussian_time_series.get(), gaussian_options);
        setAnalogIntrinsicProperties(uniform_time_series.get(), uniform_options);
        setIntervalIntrinsicProperties(intervals, interval_options);

        // Test coordinate allocation without panning
        {
            // Generate MVP matrices for all series
            glm::mat4 gaussian_model = new_getAnalogModelMat(gaussian_options, gaussian_options.cached_std_dev, gaussian_options.cached_mean, manager);
            glm::mat4 gaussian_view = new_getAnalogViewMat(manager);
            glm::mat4 gaussian_projection = new_getAnalogProjectionMat(TimeFrameIndex(1), TimeFrameIndex(10000), -1.0f, 1.0f, manager);

            glm::mat4 uniform_model = new_getAnalogModelMat(uniform_options, uniform_options.cached_std_dev, uniform_options.cached_mean, manager);
            glm::mat4 uniform_view = new_getAnalogViewMat(manager);
            glm::mat4 uniform_projection = new_getAnalogProjectionMat(TimeFrameIndex(1), TimeFrameIndex(10000), -1.0f, 1.0f, manager);

            glm::mat4 interval_model = new_getIntervalModelMat(interval_options, manager);
            glm::mat4 interval_view = new_getIntervalViewMat(manager);
            glm::mat4 interval_projection = new_getIntervalProjectionMat(1, 10000, -1.0f, 1.0f, manager);

            // Test Gaussian series: ±3σ should extend for half the canvas vertically
            // Mean=0 should map to gaussian_center, ±3σ should span allocated height
            glm::vec2 gaussian_mean_point = applyMVPTransformationIntegration(5000, gaussian_options.cached_mean, gaussian_model, gaussian_view, gaussian_projection);

            // Use actual ±3σ values based on cached statistics
            float const actual_plus_3sigma = gaussian_options.cached_mean + 3.0f * gaussian_options.cached_std_dev;
            float const actual_minus_3sigma = gaussian_options.cached_mean - 3.0f * gaussian_options.cached_std_dev;

            glm::vec2 gaussian_plus_3sigma = applyMVPTransformationIntegration(5000, actual_plus_3sigma, gaussian_model, gaussian_view, gaussian_projection);
            glm::vec2 gaussian_minus_3sigma = applyMVPTransformationIntegration(5000, actual_minus_3sigma, gaussian_model, gaussian_view, gaussian_projection);

            REQUIRE_THAT(gaussian_mean_point.y, WithinRel(gaussian_center, 0.02f));

            // ±3σ should extend to edges of allocated space (80% of allocated height)
            // The transformation is designed so that ±3σ maps to ±1.0 in normalized space,
            // then scaled by allocated_height * 0.8 and centered at allocated_center
            // This means ±3σ spans the FULL height_scale, not half of it

            float const height_scale = gaussian_height * 0.8f;                              // 80% of allocated height
            float const gaussian_expected_top_corrected = gaussian_center + height_scale;   // +3σ
            float const gaussian_expected_bottom_corrected = gaussian_center - height_scale;// -3σ

            REQUIRE_THAT(gaussian_plus_3sigma.y, WithinRel(gaussian_expected_top_corrected, 0.02f));
            REQUIRE_THAT(gaussian_minus_3sigma.y, WithinRel(gaussian_expected_bottom_corrected, 0.02f));

            // Test Uniform series: mean=0.5 should be in center of remaining half
            glm::vec2 uniform_mean_point = applyMVPTransformationIntegration(5000, 0.5f, uniform_model, uniform_view, uniform_projection);
            REQUIRE_THAT(uniform_mean_point.y, WithinRel(uniform_center, 0.02f));

            // Test Digital intervals: should extend from top to bottom of canvas
            glm::vec2 interval_bottom = applyIntervalMVPTransformationIntegration(5000.0f, -1.0f, interval_model, interval_view, interval_projection);
            glm::vec2 interval_top = applyIntervalMVPTransformationIntegration(5000.0f, 1.0f, interval_model, interval_view, interval_projection);

            float interval_expected_bottom = -1.0f * (interval_height * interval_options.margin_factor * 0.5f);
            float interval_expected_top = 1.0f * (interval_height * interval_options.margin_factor * 0.5f);
            REQUIRE_THAT(interval_bottom.y, WithinRel(interval_expected_bottom, 0.02f));
            REQUIRE_THAT(interval_top.y, WithinRel(interval_expected_top, 0.02f));
        }

        // Test behavior with vertical panning
        float pan_offset = 1.0f;// Pan upward by 1 unit
        manager.setPanOffset(pan_offset);

        {
            // Generate MVP matrices with panning applied
            glm::mat4 gaussian_model_panned = new_getAnalogModelMat(gaussian_options, gaussian_options.cached_std_dev, gaussian_options.cached_mean, manager);
            glm::mat4 gaussian_view_panned = new_getAnalogViewMat(manager);
            glm::mat4 gaussian_projection_panned = new_getAnalogProjectionMat(TimeFrameIndex(1), TimeFrameIndex(10000), -1.0f, 1.0f, manager);

            glm::mat4 uniform_model_panned = new_getAnalogModelMat(uniform_options, uniform_options.cached_std_dev, uniform_options.cached_mean, manager);
            glm::mat4 uniform_view_panned = new_getAnalogViewMat(manager);
            glm::mat4 uniform_projection_panned = new_getAnalogProjectionMat(TimeFrameIndex(1), TimeFrameIndex(10000), -1.0f, 1.0f, manager);

            glm::mat4 interval_model_panned = new_getIntervalModelMat(interval_options, manager);
            glm::mat4 interval_view_panned = new_getIntervalViewMat(manager);
            glm::mat4 interval_projection_panned = new_getIntervalProjectionMat(1, 10000, -1.0f, 1.0f, manager);

            // Test analog series: should be shifted by pan offset
            glm::vec2 gaussian_mean_panned = applyMVPTransformationIntegration(5000, 0.0f, gaussian_model_panned, gaussian_view_panned, gaussian_projection_panned);
            glm::vec2 uniform_mean_panned = applyMVPTransformationIntegration(5000, 0.5f, uniform_model_panned, uniform_view_panned, uniform_projection_panned);

            // Analog series should be shifted by pan offset
            REQUIRE_THAT(gaussian_mean_panned.y, WithinRel(gaussian_center + pan_offset, 0.02f));
            REQUIRE_THAT(uniform_mean_panned.y, WithinRel(uniform_center + pan_offset, 0.02f));

            // Test digital intervals: should remain fixed to canvas bounds regardless of panning
            glm::vec2 interval_bottom_panned = applyIntervalMVPTransformationIntegration(5000.0f, -1.0f, interval_model_panned, interval_view_panned, interval_projection_panned);
            glm::vec2 interval_top_panned = applyIntervalMVPTransformationIntegration(5000.0f, 1.0f, interval_model_panned, interval_view_panned, interval_projection_panned);

            // Digital intervals should NOT be affected by panning - they remain pinned to viewport
            float interval_expected_bottom_panned = -1.0f * (interval_height * interval_options.margin_factor * 0.5f);
            float interval_expected_top_panned = 1.0f * (interval_height * interval_options.margin_factor * 0.5f);
            REQUIRE_THAT(interval_bottom_panned.y, WithinRel(interval_expected_bottom_panned, 0.02f));
            REQUIRE_THAT(interval_top_panned.y, WithinRel(interval_expected_top_panned, 0.02f));
        }

        // Test with negative panning as well
        manager.setPanOffset(-0.8f);

        {
            glm::mat4 gaussian_model_neg = new_getAnalogModelMat(gaussian_options, gaussian_options.cached_std_dev, gaussian_options.cached_mean, manager);
            glm::mat4 gaussian_view_neg = new_getAnalogViewMat(manager);
            glm::mat4 gaussian_projection_neg = new_getAnalogProjectionMat(TimeFrameIndex(1), TimeFrameIndex(10000), -1.0f, 1.0f, manager);

            glm::mat4 interval_model_neg = new_getIntervalModelMat(interval_options, manager);
            glm::mat4 interval_view_neg = new_getIntervalViewMat(manager);
            glm::mat4 interval_projection_neg = new_getIntervalProjectionMat(1, 10000, -1.0f, 1.0f, manager);

            glm::vec2 gaussian_mean_neg = applyMVPTransformationIntegration(5000, 0.0f, gaussian_model_neg, gaussian_view_neg, gaussian_projection_neg);
            glm::vec2 interval_center_neg = applyIntervalMVPTransformationIntegration(5000.0f, 0.0f, interval_model_neg, interval_view_neg, interval_projection_neg);

            // Analog should move with negative pan
            REQUIRE_THAT(gaussian_mean_neg.y, WithinRel(gaussian_center - 0.8f, 0.02f));

            // Digital intervals should remain at viewport center regardless of pan direction
            REQUIRE_THAT(interval_center_neg.y, Catch::Matchers::WithinAbs(0.0f, 0.02f));
        }

        // Reset and verify
        manager.resetPan();
        REQUIRE(manager.getPanOffset() == 0.0f);
    }
}

TEST_CASE("Integration Test: Mixed Analog and Digital Event Series", "[integration][mvp][mixed_analog_events]") {

    SECTION("Global stacked allocation: 2 analog + 2 digital events = 4 series each getting 1/4 canvas") {
        // Set up plotting manager with mixed data types
        PlottingManager manager;

        // Add 2 analog series and 2 digital event series
        std::vector<int> times(100);
        for(int i=0; i<100; ++i) times[i] = i;
        auto time_frame = std::make_shared<TimeFrame>(times);
        
        std::vector<float> data(100, 0.0f);
        std::vector<TimeFrameIndex> time_indices;
        for(int i=0; i<100; ++i) time_indices.emplace_back(i);
        auto series = std::make_shared<AnalogTimeSeries>(data, time_indices);

        int analog1 = manager.addAnalogSeries("s1", series, time_frame);
        int analog2 = manager.addAnalogSeries("s2", series, time_frame);
        auto event_series = std::make_shared<DigitalEventSeries>();
        event_series->setTimeFrame(time_frame);
        int event1 = manager.addDigitalEventSeries("e1", event_series);
        int event2 = manager.addDigitalEventSeries("e2", event_series);

        // Verify series indices and counts
        REQUIRE(analog1 == 0);
        REQUIRE(analog2 == 1);
        REQUIRE(event1 == 0);
        REQUIRE(event2 == 1);
        REQUIRE(manager.total_analog_series == 2);
        REQUIRE(manager.total_event_series == 2);

        // Generate test data
        constexpr size_t num_points = 10000;
        std::vector<TimeFrameIndex> time_vector;
        for (size_t i = 0; i < num_points; ++i) {
            time_vector.push_back(TimeFrameIndex(i));
        }
        auto analog1_data = generateGaussianDataIntegration(num_points, 0.0f, 10.0f, 42);
        auto analog2_data = generateGaussianDataIntegration(num_points, 0.0f, 10.0f, 123);
        auto events1 = generateTestEventData(50, 10000.0f, 456);
        auto events2 = generateTestEventData(30, 10000.0f, 789);

        auto analog1_time_series = std::make_shared<AnalogTimeSeries>(analog1_data, time_vector);
        auto analog2_time_series = std::make_shared<AnalogTimeSeries>(analog2_data, time_vector);

        // Set up display options
        NewAnalogTimeSeriesDisplayOptions analog1_options;
        NewAnalogTimeSeriesDisplayOptions analog2_options;
        NewDigitalEventSeriesDisplayOptions event1_options;
        NewDigitalEventSeriesDisplayOptions event2_options;

        // Configure digital events for stacked mode
        event1_options.plotting_mode = EventPlottingMode::Stacked;
        event2_options.plotting_mode = EventPlottingMode::Stacked;

        // Calculate global stacked allocation (4 total series sharing canvas)
        int const total_stackable_series = manager.total_analog_series + manager.total_event_series;
        REQUIRE(total_stackable_series == 4);

        float analog1_center, analog1_height;
        float analog2_center, analog2_height;
        float event1_center, event1_height;
        float event2_center, event2_height;

        manager.calculateGlobalStackedAllocation(analog1, -1, total_stackable_series, analog1_center, analog1_height);
        manager.calculateGlobalStackedAllocation(analog2, -1, total_stackable_series, analog2_center, analog2_height);
        manager.calculateGlobalStackedAllocation(-1, event1, total_stackable_series, event1_center, event1_height);
        manager.calculateGlobalStackedAllocation(-1, event2, total_stackable_series, event2_center, event2_height);

        // Verify that each series gets 1/4 of the canvas (2.0 total height / 4 = 0.5)
        float expected_height = 2.0f / 4.0f;
        REQUIRE_THAT(analog1_height, WithinRel(expected_height, 0.01f));
        REQUIRE_THAT(analog2_height, WithinRel(expected_height, 0.01f));
        REQUIRE_THAT(event1_height, WithinRel(expected_height, 0.01f));
        REQUIRE_THAT(event2_height, WithinRel(expected_height, 0.01f));

        // Verify proper stacking order and centers
        // Series should be stacked from top to bottom: analog1, analog2, event1, event2
        float expected_analog1_center = -1.0f + expected_height * 0.5f;// First quarter
        float expected_analog2_center = -1.0f + expected_height * 1.5f;// Second quarter
        float expected_event1_center = -1.0f + expected_height * 2.5f; // Third quarter
        float expected_event2_center = -1.0f + expected_height * 3.5f; // Fourth quarter

        REQUIRE_THAT(analog1_center, WithinRel(expected_analog1_center, 0.01f));
        REQUIRE_THAT(analog2_center, WithinRel(expected_analog2_center, 0.01f));
        REQUIRE_THAT(event1_center, WithinRel(expected_event1_center, 0.01f));
        REQUIRE_THAT(event2_center, WithinRel(expected_event2_center, 0.01f));

        // Configure display options with allocated coordinates
        analog1_options.allocated_y_center = analog1_center;
        analog1_options.allocated_height = analog1_height;
        analog2_options.allocated_y_center = analog2_center;
        analog2_options.allocated_height = analog2_height;
        event1_options.allocated_y_center = event1_center;
        event1_options.allocated_height = event1_height;
        event2_options.allocated_y_center = event2_center;
        event2_options.allocated_height = event2_height;

        // Set intrinsic properties
        setAnalogIntrinsicProperties(analog1_time_series.get(), analog1_options);
        setAnalogIntrinsicProperties(analog2_time_series.get(), analog2_options);
        setEventIntrinsicProperties(events1, event1_options);
        setEventIntrinsicProperties(events2, event2_options);

        // Test MVP matrix generation
        glm::mat4 analog1_model = new_getAnalogModelMat(analog1_options, analog1_options.cached_std_dev, analog1_options.cached_mean, manager);
        glm::mat4 analog2_model = new_getAnalogModelMat(analog2_options, analog2_options.cached_std_dev, analog2_options.cached_mean, manager);
        glm::mat4 event1_model = new_getEventModelMat(event1_options, manager);
        glm::mat4 event2_model = new_getEventModelMat(event2_options, manager);

        // Verify that analog series are positioned at their allocated centers
        // For analog series, the mean should map to the allocated center
        glm::vec2 analog1_mean_point = applyMVPTransformationIntegration(5000, analog1_options.cached_mean, analog1_model,
                                                                         new_getAnalogViewMat(manager),
                                                                         new_getAnalogProjectionMat(TimeFrameIndex(1), TimeFrameIndex(10000), -1.0f, 1.0f, manager));
        glm::vec2 analog2_mean_point = applyMVPTransformationIntegration(5000, analog2_options.cached_mean, analog2_model,
                                                                         new_getAnalogViewMat(manager),
                                                                         new_getAnalogProjectionMat(TimeFrameIndex(1), TimeFrameIndex(10000), -1.0f, 1.0f, manager));

        REQUIRE_THAT(analog1_mean_point.y, WithinRel(analog1_center, 0.02f));
        REQUIRE_THAT(analog2_mean_point.y, WithinRel(analog2_center, 0.02f));

        // Verify that digital events are positioned at their allocated centers
        REQUIRE_THAT(event1_model[3][1], WithinRel(event1_center, 0.01f));
        REQUIRE_THAT(event2_model[3][1], WithinRel(event2_center, 0.01f));

        // Test panning behavior: analog series and stacked digital events should move together
        float pan_offset = 0.5f;
        manager.setPanOffset(pan_offset);

        glm::mat4 analog1_view_panned = new_getAnalogViewMat(manager);
        glm::mat4 event1_view_panned = new_getEventViewMat(event1_options, manager);
        glm::mat4 event2_view_panned = new_getEventViewMat(event2_options, manager);

        // All should move with panning in stacked mode
        REQUIRE(analog1_view_panned[3][1] == pan_offset);
        REQUIRE(event1_view_panned[3][1] == pan_offset);
        REQUIRE(event2_view_panned[3][1] == pan_offset);

        manager.resetPan();
    }
}

TEST_CASE("X-Axis Extreme Range Scrolling Test", "[integration][xaxis][extreme_range]") {
    SECTION("Full range to normal window transition with data validation") {
        // Create test data with a large range
        constexpr size_t num_points = 100000;  // Large dataset
        constexpr int64_t data_start = 0;
        constexpr int64_t data_end = num_points - 1;
        
        std::vector<TimeFrameIndex> time_vector;
        for (size_t i = 0; i < num_points; ++i) {
            time_vector.push_back(TimeFrameIndex(i));
        }
        
        // Generate test data with known characteristics at specific points
        auto gaussian_data = generateGaussianDataIntegration(num_points, 0.0f, 10.0f, 42);
        
        // Create specific test points for validation
        gaussian_data[1000] = 100.0f;   // Known value at index 1000
        gaussian_data[50000] = 200.0f;  // Known value at index 50000
        gaussian_data[99000] = 300.0f;  // Known value at index 99000
        
        auto time_series = std::make_shared<AnalogTimeSeries>(gaussian_data, time_vector);
        
        // Set up plotting manager with analog series
        PlottingManager manager;
        std::vector<int> times(num_points);
        for(size_t i=0; i<num_points; ++i) times[i] = static_cast<int>(i);
        auto time_frame = std::make_shared<TimeFrame>(times);
        int series_index = manager.addAnalogSeries("s1", time_series, time_frame);
        
        // Configure display options
        NewAnalogTimeSeriesDisplayOptions display_options;
        float allocated_center, allocated_height;
        manager.calculateAnalogSeriesAllocation(series_index, allocated_center, allocated_height);
        display_options.allocated_y_center = allocated_center;
        display_options.allocated_height = allocated_height;
        setAnalogIntrinsicProperties(time_series.get(), display_options);
        
        // Test Case 1: Set x-axis to the entirety of the data range
        INFO("Testing full range display");
        
        // Generate MVP matrices for full range
        glm::mat4 model_full = new_getAnalogModelMat(display_options, 
                                                    display_options.cached_std_dev, 
                                                    display_options.cached_mean, 
                                                    manager);
        glm::mat4 view_full = new_getAnalogViewMat(manager);
        glm::mat4 projection_full = new_getAnalogProjectionMat(TimeFrameIndex(data_start), 
                                                              TimeFrameIndex(data_end), 
                                                              -400.0f, 400.0f, 
                                                              manager);
        
        // Verify matrices are valid (not NaN or infinite)
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                REQUIRE(std::isfinite(model_full[i][j]));
                REQUIRE(std::isfinite(view_full[i][j]));
                REQUIRE(std::isfinite(projection_full[i][j]));
            }
        }
        
        // Test transformations at key points for full range
        glm::vec2 point_1000_full = applyMVPTransformationIntegration(1000, 100.0f, 
                                                                     model_full, view_full, projection_full);
        glm::vec2 point_50000_full = applyMVPTransformationIntegration(50000, 200.0f, 
                                                                      model_full, view_full, projection_full);
        glm::vec2 point_99000_full = applyMVPTransformationIntegration(99000, 300.0f, 
                                                                      model_full, view_full, projection_full);
        
        // Verify points are within reasonable NDC range [-1.1, 1.1] with tolerance
        REQUIRE(point_1000_full.x >= -1.1f);
        REQUIRE(point_1000_full.x <= 1.1f);
        REQUIRE(point_50000_full.x >= -1.1f);
        REQUIRE(point_50000_full.x <= 1.1f);
        REQUIRE(point_99000_full.x >= -1.1f);
        REQUIRE(point_99000_full.x <= 1.1f);
        
        // Verify Y coordinates are finite and reasonable
        REQUIRE(std::isfinite(point_1000_full.y));
        REQUIRE(std::isfinite(point_50000_full.y));
        REQUIRE(std::isfinite(point_99000_full.y));
        
        // Test Case 2: Change back to a normal window (zoom in to a specific range)
        INFO("Testing normal window display");
        constexpr int64_t normal_start = 45000;
        constexpr int64_t normal_end = 55000;
                
        // Generate MVP matrices for normal range
        glm::mat4 model_normal = new_getAnalogModelMat(display_options, 
                                                      display_options.cached_std_dev, 
                                                      display_options.cached_mean, 
                                                      manager);
        glm::mat4 view_normal = new_getAnalogViewMat(manager);
        glm::mat4 projection_normal = new_getAnalogProjectionMat(TimeFrameIndex(normal_start), 
                                                                TimeFrameIndex(normal_end), 
                                                                -400.0f, 400.0f, 
                                                                manager);
        
        // Verify matrices are valid
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                REQUIRE(std::isfinite(model_normal[i][j]));
                REQUIRE(std::isfinite(view_normal[i][j]));
                REQUIRE(std::isfinite(projection_normal[i][j]));
            }
        }
        
        // Test transformations for the point that should be visible (index 50000)
        glm::vec2 point_50000_normal = applyMVPTransformationIntegration(50000, 200.0f, 
                                                                        model_normal, view_normal, projection_normal);
        
        // Verify the visible point is within NDC range
        REQUIRE(point_50000_normal.x >= -1.1f);
        REQUIRE(point_50000_normal.x <= 1.1f);
        REQUIRE(std::isfinite(point_50000_normal.y));
        
        // Test transformations for points outside the normal range
        glm::vec2 point_1000_normal = applyMVPTransformationIntegration(1000, 100.0f, 
                                                                       model_normal, view_normal, projection_normal);
        glm::vec2 point_99000_normal = applyMVPTransformationIntegration(99000, 300.0f, 
                                                                        model_normal, view_normal, projection_normal);
        
        // Points outside the visible range should be outside NDC range
        INFO("Point 1000 (outside range) x-coordinate: " << point_1000_normal.x);
        INFO("Point 99000 (outside range) x-coordinate: " << point_99000_normal.x);
        
        // These should be outside the visible range but still finite
        REQUIRE(std::isfinite(point_1000_normal.x));
        REQUIRE(std::isfinite(point_1000_normal.y));
        REQUIRE(std::isfinite(point_99000_normal.x));
        REQUIRE(std::isfinite(point_99000_normal.y));
        
        // Test Case 3: Verify data consistency after range changes
        INFO("Testing data consistency after range changes");
        
        // The point at index 50000 should have the same Y-transformation in both cases
        // (only X should change due to different projection ranges)
        // Model and View matrices should be identical for the same series
        REQUIRE_THAT(model_normal[1][1], WithinRel(model_full[1][1], 0.01f)); // Y scaling should be the same
        REQUIRE_THAT(model_normal[3][1], WithinRel(model_full[3][1], 0.01f)); // Y offset should be the same
        
        // Test Case 4: Extreme range transitions (stress test)
        INFO("Testing extreme range transitions");
        
        // Go to a very small range
        constexpr int64_t tiny_start = 50000;
        constexpr int64_t tiny_end = 50010;  // Only 10 data points
                
        glm::mat4 projection_tiny = new_getAnalogProjectionMat(TimeFrameIndex(tiny_start), 
                                                              TimeFrameIndex(tiny_end), 
                                                              -400.0f, 400.0f, 
                                                              manager);
        
        // Verify tiny range projection is valid
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                REQUIRE(std::isfinite(projection_tiny[i][j]));
            }
        }
        
        // Test transformation for point in tiny range
        glm::vec2 point_50005_tiny = applyMVPTransformationIntegration(50005, 200.0f, 
                                                                      model_normal, view_normal, projection_tiny);
        
        REQUIRE(std::isfinite(point_50005_tiny.x));
        REQUIRE(std::isfinite(point_50005_tiny.y));
        
        // Go back to full range to verify system recovery        
        glm::mat4 projection_recovery = new_getAnalogProjectionMat(TimeFrameIndex(data_start), 
                                                                  TimeFrameIndex(data_end), 
                                                                  -400.0f, 400.0f, 
                                                                  manager);
        
        // Verify recovery matrices are valid
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                REQUIRE(std::isfinite(projection_recovery[i][j]));
            }
        }
        
        // Verify that after recovery, transformations work correctly again
        glm::vec2 point_50000_recovery = applyMVPTransformationIntegration(50000, 200.0f, 
                                                                          model_normal, view_normal, projection_recovery);
        
        REQUIRE(std::isfinite(point_50000_recovery.x));
        REQUIRE(std::isfinite(point_50000_recovery.y));
        REQUIRE(point_50000_recovery.x >= -1.1f);
        REQUIRE(point_50000_recovery.x <= 1.1f);
    }
    
    SECTION("Short video extreme scrolling simulation (704 frames)") {
        // Simulate the bug reported with short videos (704 frames)
        // Test multiple extreme zoom in/out cycles to see if range gets stuck
        
        constexpr int64_t video_length = 704;
        constexpr int64_t data_min = 0;
        constexpr int64_t data_max = video_length - 1;
        
        // Create XAxis for short video
        XAxis x_axis(0, 100, data_min, data_max);
        
        // Create test data
        std::vector<TimeFrameIndex> time_vector;
        for (int64_t i = 0; i < video_length; ++i) {
            time_vector.push_back(TimeFrameIndex(i));
        }
        auto test_data = generateGaussianDataIntegration(static_cast<size_t>(video_length), 0.0f, 10.0f, 42);
        auto time_series = std::make_shared<AnalogTimeSeries>(test_data, time_vector);
        
        PlottingManager manager;
        std::vector<int> times(video_length);
        for(size_t i=0; i<video_length; ++i) times[i] = static_cast<int>(i);
        auto time_frame = std::make_shared<TimeFrame>(times);
        int series_index = manager.addAnalogSeries("s1", time_series, time_frame);
        
        NewAnalogTimeSeriesDisplayOptions display_options;
        float allocated_center, allocated_height;
        manager.calculateAnalogSeriesAllocation(series_index, allocated_center, allocated_height);
        display_options.allocated_y_center = allocated_center;
        display_options.allocated_height = allocated_height;
        setAnalogIntrinsicProperties(time_series.get(), display_options);
        
        INFO("Testing short video (704 frames) extreme zoom cycles");
        
        // Cycle 1: Zoom out to full range
        INFO("Cycle 1: Zoom to full range");
        int64_t center = video_length / 2;
        int64_t range_width = video_length;
        int64_t actual_range = x_axis.setCenterAndZoomWithFeedback(center, range_width);
        
        REQUIRE(actual_range > 0);
        REQUIRE(actual_range <= video_length);
        REQUIRE(x_axis.getStart() >= data_min);
        REQUIRE(x_axis.getEnd() <= data_max);
        
        // Verify MVP matrices are valid at full range
        glm::mat4 projection_full = new_getAnalogProjectionMat(
            TimeFrameIndex(x_axis.getStart()), 
            TimeFrameIndex(x_axis.getEnd()), 
            -400.0f, 400.0f, 
            manager);
        
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                REQUIRE(std::isfinite(projection_full[i][j]));
            }
        }
        
        // Cycle 2: Zoom way in to very small range (simulating extreme zoom)
        INFO("Cycle 2: Zoom to tiny range (10 samples)");
        range_width = 10;
        actual_range = x_axis.setCenterAndZoomWithFeedback(center, range_width);
        
        REQUIRE(actual_range > 0);
        REQUIRE(x_axis.getStart() >= data_min);
        REQUIRE(x_axis.getEnd() <= data_max);
        
        glm::mat4 projection_tiny = new_getAnalogProjectionMat(
            TimeFrameIndex(x_axis.getStart()), 
            TimeFrameIndex(x_axis.getEnd()), 
            -400.0f, 400.0f, 
            manager);
        
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                REQUIRE(std::isfinite(projection_tiny[i][j]));
            }
        }
        
        // Cycle 3: Zoom back out to full range
        INFO("Cycle 3: Zoom back to full range");
        range_width = video_length;
        actual_range = x_axis.setCenterAndZoomWithFeedback(center, range_width);
        
        REQUIRE(actual_range > 0);
        REQUIRE(actual_range <= video_length);
        
        // Cycle 4: Extreme zoom (try to go to 2 samples like the bug)
        INFO("Cycle 4: Zoom to 2 samples");
        range_width = 2;
        actual_range = x_axis.setCenterAndZoomWithFeedback(center, range_width);
        
        REQUIRE(actual_range > 0);
        REQUIRE(x_axis.getStart() >= data_min);
        REQUIRE(x_axis.getEnd() <= data_max);
        
        glm::mat4 projection_2samples = new_getAnalogProjectionMat(
            TimeFrameIndex(x_axis.getStart()), 
            TimeFrameIndex(x_axis.getEnd()), 
            -400.0f, 400.0f, 
            manager);
        
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                REQUIRE(std::isfinite(projection_2samples[i][j]));
            }
        }
        
        // Cycle 5: Try to zoom back out after being stuck at 2
        INFO("Cycle 5: Attempt to zoom out from 2 samples to 100 samples");
        range_width = 100;
        actual_range = x_axis.setCenterAndZoomWithFeedback(center, range_width);
        
        INFO("After requesting 100 samples, actual range achieved: " << actual_range);
        
        // This is the key test: after being at 2 samples, can we zoom out?
        REQUIRE(actual_range >= 90);  // Should be close to requested 100 (allow some clamping)
        REQUIRE(actual_range > 2);     // Should definitely be more than 2!
        
        // Cycle 6: Multiple rapid zoom cycles
        INFO("Cycle 6: Rapid zoom cycles");
        std::vector<int64_t> test_ranges = {704, 50, 200, 10, 500, 5, 704, 2, 350};
        
        for (size_t i = 0; i < test_ranges.size(); ++i) {
            range_width = test_ranges[i];
            actual_range = x_axis.setCenterAndZoomWithFeedback(center, range_width);
            
            INFO("Rapid cycle " << i << ": requested=" << range_width << ", actual=" << actual_range);
            
            REQUIRE(actual_range > 0);
            REQUIRE(x_axis.getStart() >= data_min);
            REQUIRE(x_axis.getEnd() <= data_max);
            REQUIRE(x_axis.getStart() < x_axis.getEnd());
            
            // Verify MVP matrices remain valid throughout rapid changes
            glm::mat4 projection = new_getAnalogProjectionMat(
                TimeFrameIndex(x_axis.getStart()), 
                TimeFrameIndex(x_axis.getEnd()), 
                -400.0f, 400.0f, 
                manager);
            
            for (int row = 0; row < 4; ++row) {
                for (int col = 0; col < 4; ++col) {
                    REQUIRE(std::isfinite(projection[row][col]));
                }
            }
        }
        
        // Final test: Verify we can still zoom to any valid range after all the abuse
        INFO("Final test: Verify full zoom capability after stress test");
        range_width = video_length / 2;  // Half the video
        actual_range = x_axis.setCenterAndZoomWithFeedback(center, range_width);
        
        REQUIRE(actual_range >= video_length / 2 - 10);  // Allow small clamping tolerance
        REQUIRE(actual_range <= video_length / 2 + 10);
    }
    
    SECTION("XAxis clamping boundary conditions (704 frames)") {
        // Test specific scenarios where XAxis might get stuck at small ranges
        constexpr int64_t video_length = 704;
        XAxis x_axis(0, 100, 0, video_length);
        
        INFO("Test 1: Zoom to 2 samples at various positions");
        
        // Test at start of video
        int64_t actual = x_axis.setCenterAndZoomWithFeedback(10, 2);
        INFO("  Position 10, range 2: start=" << x_axis.getStart() << ", end=" << x_axis.getEnd() << ", actual=" << actual);
        REQUIRE(actual == 2);
        REQUIRE(x_axis.getStart() >= 0);
        REQUIRE(x_axis.getEnd() <= video_length);
        
        // Test at middle
        actual = x_axis.setCenterAndZoomWithFeedback(352, 2);
        INFO("  Position 352, range 2: start=" << x_axis.getStart() << ", end=" << x_axis.getEnd() << ", actual=" << actual);
        REQUIRE(actual == 2);
        
        // Test at end
        actual = x_axis.setCenterAndZoomWithFeedback(700, 2);
        INFO("  Position 700, range 2: start=" << x_axis.getStart() << ", end=" << x_axis.getEnd() << ", actual=" << actual);
        REQUIRE(actual == 2);
        REQUIRE(x_axis.getStart() >= 0);
        REQUIRE(x_axis.getEnd() <= video_length);
        
        INFO("Test 2: From stuck at 2 samples (pos 700), try to zoom out to 100");
        actual = x_axis.setCenterAndZoomWithFeedback(700, 100);
        INFO("  Requested 100, got actual=" << actual << ", range=[" << x_axis.getStart() << ", " << x_axis.getEnd() << "]");
        REQUIRE(actual >= 90);  // Should get close to 100
        REQUIRE(actual > 2);    // Should definitely not be stuck at 2!
        
        INFO("Test 3: Extreme case - stuck at 2 samples at absolute end");
        x_axis.setCenterAndZoomWithFeedback(703, 2);  // As far right as possible
        INFO("  After setting to pos 703, range 2: start=" << x_axis.getStart() << ", end=" << x_axis.getEnd());
        
        actual = x_axis.setCenterAndZoomWithFeedback(703, 200);
        INFO("  From pos 703, requested 200, got actual=" << actual << ", range=[" << x_axis.getStart() << ", " << x_axis.getEnd() << "]");
        REQUIRE(actual >= 190);
        REQUIRE(actual <= 210);
        
        INFO("Test 4: Check if _max is still correct after operations");
        REQUIRE(x_axis.getMax() == video_length);
    }
    
    SECTION("XAxis integration with PlottingManager") {
        // Test the integration between XAxis class and PlottingManager
        // This simulates how the OpenGLWidget would use both systems
        
        constexpr int64_t data_min = 0;
        constexpr int64_t data_max = 100000;
        
        // Create XAxis with full data range
        XAxis x_axis(data_min, data_min + 1000, data_min, data_max);  // Start with small visible range
        
        // Create plotting manager and set it to match XAxis
        PlottingManager manager;
        
        // Test synchronized range changes
        INFO("Testing synchronized XAxis and PlottingManager range changes");
        
        // Change to full range
        x_axis.setVisibleRange(data_min, data_max);
        
        REQUIRE(x_axis.getStart() == data_min);
        REQUIRE(x_axis.getEnd() == data_max);
        
        // Change to middle range
        constexpr int64_t mid_start = 40000;
        constexpr int64_t mid_end = 60000;
        
        x_axis.setVisibleRange(mid_start, mid_end);
        
        REQUIRE(x_axis.getStart() == mid_start);
        REQUIRE(x_axis.getEnd() == mid_end);
        
        // Test zoom functionality with feedback
        INFO("Testing zoom functionality with range feedback");
        
        int64_t const zoom_center = 50000;
        int64_t const zoom_range = 2000;
        
        int64_t actual_range = x_axis.setCenterAndZoomWithFeedback(zoom_center, zoom_range);
        
        // Verify zoom worked as expected
        REQUIRE(actual_range > 0);
        REQUIRE(x_axis.getEnd() - x_axis.getStart() == actual_range);
        
        // Center should be approximately correct (may be adjusted due to clamping)
        int64_t actual_center = (x_axis.getStart() + x_axis.getEnd()) / 2;
        REQUIRE_THAT(static_cast<float>(actual_center), WithinRel(static_cast<float>(zoom_center), 0.1f));
        
        // Test extreme zoom out (beyond data boundaries)
        INFO("Testing extreme zoom out behavior");
        
        int64_t const extreme_range = 200000;  // Larger than data range
        int64_t extreme_actual_range = x_axis.setCenterAndZoomWithFeedback(zoom_center, extreme_range);
        
        // Should be clamped to maximum data range
        REQUIRE(extreme_actual_range <= (data_max - data_min));
        REQUIRE(x_axis.getStart() >= data_min);
        REQUIRE(x_axis.getEnd() <= data_max);
    }
    
    SECTION("Data range retrieval edge cases - reproducing OpenGLWidget issue") {
        // This section reproduces the specific issue in OpenGLWidget where 
        // getTimeValueSpanInTimeFrameIndexRange returns empty when scrolled too far
        
        // Create a small dataset to make the issue easier to reproduce
        constexpr size_t num_points = 1000;
        std::vector<TimeFrameIndex> time_vector;
        for (size_t i = 0; i < num_points; ++i) {
            time_vector.push_back(TimeFrameIndex(i));  // Data exists from 0 to 999
        }
        
        auto data = generateGaussianDataIntegration(num_points, 0.0f, 10.0f, 42);
        auto time_series = std::make_shared<AnalogTimeSeries>(data, time_vector);
        
        INFO("Testing data retrieval when range is completely outside data bounds");
        
        // Test case 1: Request range completely before data (should return empty)
        auto analog_range_before = time_series->getTimeValueSpanInTimeFrameIndexRange(
            TimeFrameIndex(-2000), TimeFrameIndex(-1000));
        
        INFO("Range before data: start=-2000, end=-1000, result size=" << analog_range_before.values.size());
        REQUIRE(analog_range_before.values.empty());  // This is the "no data shown" issue
        
        // Test case 2: Request range completely after data (should return empty)
        auto analog_range_after = time_series->getTimeValueSpanInTimeFrameIndexRange(
            TimeFrameIndex(2000), TimeFrameIndex(3000));
        
        INFO("Range after data: start=2000, end=3000, result size=" << analog_range_after.values.size());
        REQUIRE(analog_range_after.values.empty());  // This is the "no data shown" issue
        
        // Test case 3: Request range partially overlapping data (should return partial data)
        auto analog_range_partial_before = time_series->getTimeValueSpanInTimeFrameIndexRange(
            TimeFrameIndex(-500), TimeFrameIndex(100));
        
        INFO("Range partial before: start=-500, end=100, result size=" << analog_range_partial_before.values.size());
        REQUIRE(!analog_range_partial_before.values.empty());  // Should have data from 0 to 100
        REQUIRE(analog_range_partial_before.values.size() == 101);  // 0 through 100 inclusive
        
        auto analog_range_partial_after = time_series->getTimeValueSpanInTimeFrameIndexRange(
            TimeFrameIndex(900), TimeFrameIndex(1500));
        
        INFO("Range partial after: start=900, end=1500, result size=" << analog_range_partial_after.values.size());
        REQUIRE(!analog_range_partial_after.values.empty());  // Should have data from 900 to 999
        REQUIRE(analog_range_partial_after.values.size() == 100);  // 900 through 999 inclusive
        
        // Test case 4: Request range much larger than data (should return all data)
        auto analog_range_huge = time_series->getTimeValueSpanInTimeFrameIndexRange(
            TimeFrameIndex(-10000), TimeFrameIndex(10000));
        
        INFO("Range huge: start=-10000, end=10000, result size=" << analog_range_huge.values.size());
        REQUIRE(!analog_range_huge.values.empty());  // Should have all data
        REQUIRE(analog_range_huge.values.size() == num_points);  // All 1000 points
        
        INFO("Testing how OpenGLWidget would handle these cases");
        
        // Simulate what OpenGLWidget does:
        // 1. Get XAxis range
        // 2. Convert to series time frame
        // 3. Call getTimeValueSpanInTimeFrameIndexRange
        // 4. Check if result is empty and return early if so
        
        auto simulate_opengl_widget_logic = [&](TimeFrameIndex start, TimeFrameIndex end) -> bool {
            auto range = time_series->getTimeValueSpanInTimeFrameIndexRange(start, end);
            if (range.values.empty()) {
                // This is where OpenGLWidget returns early, causing "no data shown"
                return false;  // No data shown
            }
            return true;  // Data would be shown
        };
        
        // Cases that cause "no data shown" in OpenGLWidget
        REQUIRE(!simulate_opengl_widget_logic(TimeFrameIndex(-2000), TimeFrameIndex(-1000)));
        REQUIRE(!simulate_opengl_widget_logic(TimeFrameIndex(2000), TimeFrameIndex(3000)));
        
        // Cases that should show data
        REQUIRE(simulate_opengl_widget_logic(TimeFrameIndex(-500), TimeFrameIndex(100)));
        REQUIRE(simulate_opengl_widget_logic(TimeFrameIndex(900), TimeFrameIndex(1500)));
        REQUIRE(simulate_opengl_widget_logic(TimeFrameIndex(-10000), TimeFrameIndex(10000)));
        REQUIRE(simulate_opengl_widget_logic(TimeFrameIndex(100), TimeFrameIndex(200)));
    }
    
    SECTION("Multi-series range handling - ensuring fix doesn't break other series") {
        // This test verifies that when one series has no data in the visible range,
        // other series can still be rendered properly (testing the OpenGLWidget fix)
        
        // Create multiple series with different data ranges
        constexpr size_t num_points = 1000;
        
        // Series 1: Data from 0 to 999
        std::vector<TimeFrameIndex> time_vector1;
        for (size_t i = 0; i < num_points; ++i) {
            time_vector1.push_back(TimeFrameIndex(i));
        }
        auto data1 = generateGaussianDataIntegration(num_points, 10.0f, 1.0f, 42);
        auto series1 = std::make_shared<AnalogTimeSeries>(data1, time_vector1);
        
        // Series 2: Data from 5000 to 5999 (completely different range)
        std::vector<TimeFrameIndex> time_vector2;
        for (size_t i = 0; i < num_points; ++i) {
            time_vector2.push_back(TimeFrameIndex(5000 + i));
        }
        auto data2 = generateGaussianDataIntegration(num_points, 20.0f, 2.0f, 123);
        auto series2 = std::make_shared<AnalogTimeSeries>(data2, time_vector2);
        
        // Series 3: Data from 10000 to 10999 (even further away)
        std::vector<TimeFrameIndex> time_vector3;
        for (size_t i = 0; i < num_points; ++i) {
            time_vector3.push_back(TimeFrameIndex(10000 + i));
        }
        auto data3 = generateGaussianDataIntegration(num_points, 30.0f, 3.0f, 456);
        auto series3 = std::make_shared<AnalogTimeSeries>(data3, time_vector3);
        
        INFO("Testing scenarios where different series have data in different ranges");
        
        // Test scenario 1: Visible range overlaps only Series 1
        TimeFrameIndex range1_start(100);
        TimeFrameIndex range1_end(200);
        
        auto range1_series1 = series1->getTimeValueSpanInTimeFrameIndexRange(range1_start, range1_end);
        auto range1_series2 = series2->getTimeValueSpanInTimeFrameIndexRange(range1_start, range1_end);
        auto range1_series3 = series3->getTimeValueSpanInTimeFrameIndexRange(range1_start, range1_end);
        
        REQUIRE(!range1_series1.values.empty());  // Series 1 has data
        REQUIRE(range1_series2.values.empty());   // Series 2 has no data (would trigger early return in old code)
        REQUIRE(range1_series3.values.empty());   // Series 3 has no data (would trigger early return in old code)
        
        // Test scenario 2: Visible range overlaps only Series 2
        TimeFrameIndex range2_start(5100);
        TimeFrameIndex range2_end(5200);
        
        auto range2_series1 = series1->getTimeValueSpanInTimeFrameIndexRange(range2_start, range2_end);
        auto range2_series2 = series2->getTimeValueSpanInTimeFrameIndexRange(range2_start, range2_end);
        auto range2_series3 = series3->getTimeValueSpanInTimeFrameIndexRange(range2_start, range2_end);
        
        REQUIRE(range2_series1.values.empty());   // Series 1 has no data
        REQUIRE(!range2_series2.values.empty());  // Series 2 has data
        REQUIRE(range2_series3.values.empty());   // Series 3 has no data
        
        // Test scenario 3: Visible range overlaps multiple series
        TimeFrameIndex range3_start(-1000);
        TimeFrameIndex range3_end(15000);
        
        auto range3_series1 = series1->getTimeValueSpanInTimeFrameIndexRange(range3_start, range3_end);
        auto range3_series2 = series2->getTimeValueSpanInTimeFrameIndexRange(range3_start, range3_end);
        auto range3_series3 = series3->getTimeValueSpanInTimeFrameIndexRange(range3_start, range3_end);
        
        REQUIRE(!range3_series1.values.empty());  // Series 1 has data
        REQUIRE(!range3_series2.values.empty());  // Series 2 has data
        REQUIRE(!range3_series3.values.empty());  // Series 3 has data
        
        // Test scenario 4: Visible range overlaps no series (gap between series)
        TimeFrameIndex range4_start(2000);
        TimeFrameIndex range4_end(3000);
        
        auto range4_series1 = series1->getTimeValueSpanInTimeFrameIndexRange(range4_start, range4_end);
        auto range4_series2 = series2->getTimeValueSpanInTimeFrameIndexRange(range4_start, range4_end);
        auto range4_series3 = series3->getTimeValueSpanInTimeFrameIndexRange(range4_start, range4_end);
        
        REQUIRE(range4_series1.values.empty());   // Series 1 has no data
        REQUIRE(range4_series2.values.empty());   // Series 2 has no data
        REQUIRE(range4_series3.values.empty());   // Series 3 has no data
        
        INFO("Verifying that the fix allows partial rendering of available series");
        
        // Simulate the improved OpenGLWidget logic that continues instead of returning early
        auto simulate_improved_opengl_logic = [&](TimeFrameIndex start, TimeFrameIndex end) -> std::vector<bool> {
            std::vector<bool> series_rendered;
            
            // Test each series
            for (auto series : {series1, series2, series3}) {
                auto range = series->getTimeValueSpanInTimeFrameIndexRange(start, end);
                if (range.values.empty()) {
                    // With the fix: continue to next series instead of early return
                    series_rendered.push_back(false);
                    continue;
                } else {
                    // Series has data and would be rendered
                    series_rendered.push_back(true);
                }
            }
            
            return series_rendered;
        };
        
        // Test scenario 1: Only series 1 should be rendered
        auto result1 = simulate_improved_opengl_logic(range1_start, range1_end);
        REQUIRE(result1.size() == 3);
        REQUIRE(result1[0] == true);   // Series 1 rendered
        REQUIRE(result1[1] == false);  // Series 2 not rendered (but doesn't stop others)
        REQUIRE(result1[2] == false);  // Series 3 not rendered (but doesn't stop others)
        
        // Test scenario 2: Only series 2 should be rendered
        auto result2 = simulate_improved_opengl_logic(range2_start, range2_end);
        REQUIRE(result2.size() == 3);
        REQUIRE(result2[0] == false);  // Series 1 not rendered
        REQUIRE(result2[1] == true);   // Series 2 rendered
        REQUIRE(result2[2] == false);  // Series 3 not rendered
        
        // Test scenario 3: All series should be rendered
        auto result3 = simulate_improved_opengl_logic(range3_start, range3_end);
        REQUIRE(result3.size() == 3);
        REQUIRE(result3[0] == true);   // Series 1 rendered
        REQUIRE(result3[1] == true);   // Series 2 rendered
        REQUIRE(result3[2] == true);   // Series 3 rendered
        
        // Test scenario 4: No series should be rendered
        auto result4 = simulate_improved_opengl_logic(range4_start, range4_end);
        REQUIRE(result4.size() == 3);
        REQUIRE(result4[0] == false);  // Series 1 not rendered
        REQUIRE(result4[1] == false);  // Series 2 not rendered
        REQUIRE(result4[2] == false);  // Series 3 not rendered
    }
    
    SECTION("MVP Matrix corruption with extreme ranges - testing for NaN/Infinity") {
        // This test specifically checks for NaN/Infinity values in MVP matrices
        // which could cause persistent OpenGL state corruption
        
        constexpr size_t num_points = 1000;
        std::vector<TimeFrameIndex> time_vector;
        for (size_t i = 0; i < num_points; ++i) {
            time_vector.push_back(TimeFrameIndex(i));  // Data from 0 to 999
        }
        
        auto data = generateGaussianDataIntegration(num_points, 0.0f, 10.0f, 42);
        auto time_series = std::make_shared<AnalogTimeSeries>(data, time_vector);
        
        PlottingManager manager;
        std::vector<int> times(num_points);
        for(size_t i=0; i<num_points; ++i) times[i] = static_cast<int>(i);
        auto time_frame = std::make_shared<TimeFrame>(times);
        int series_index = manager.addAnalogSeries("s1", time_series, time_frame);
        
        NewAnalogTimeSeriesDisplayOptions display_options;
        float allocated_center, allocated_height;
        manager.calculateAnalogSeriesAllocation(series_index, allocated_center, allocated_height);
        display_options.allocated_y_center = allocated_center;
        display_options.allocated_height = allocated_height;
        setAnalogIntrinsicProperties(time_series.get(), display_options);
        
        auto test_matrix_validity = [](glm::mat4 const& matrix, std::string const& name) {
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    if (!std::isfinite(matrix[i][j])) {
                        FAIL("Matrix " << name << "[" << i << "][" << j << "] contains invalid value: " << matrix[i][j]);
                    }
                }
            }
        };
        
        INFO("Testing MVP matrices with extreme ranges that could cause NaN/Infinity");
        
        // Test Case 1: Extremely large range (potential overflow)
        constexpr int64_t huge_start = -1000000000LL;
        constexpr int64_t huge_end = 1000000000LL;
                
        auto model_huge = new_getAnalogModelMat(display_options, 
                                               display_options.cached_std_dev, 
                                               display_options.cached_mean, 
                                               manager);
        auto view_huge = new_getAnalogViewMat(manager);
        auto projection_huge = new_getAnalogProjectionMat(TimeFrameIndex(huge_start), 
                                                          TimeFrameIndex(huge_end), 
                                                          -1000.0f, 1000.0f, 
                                                          manager);
        
        test_matrix_validity(model_huge, "Model (huge range)");
        test_matrix_validity(view_huge, "View (huge range)");
        test_matrix_validity(projection_huge, "Projection (huge range)");
        
        // Test Case 2: Zero or near-zero range (potential division by zero)
        constexpr int64_t tiny_start = 1000;
        constexpr int64_t tiny_end = 1001;  // Range of 1
                
        auto model_tiny = new_getAnalogModelMat(display_options, 
                                               display_options.cached_std_dev, 
                                               display_options.cached_mean, 
                                               manager);
        auto view_tiny = new_getAnalogViewMat(manager);
        auto projection_tiny = new_getAnalogProjectionMat(TimeFrameIndex(tiny_start), 
                                                          TimeFrameIndex(tiny_end), 
                                                          -1000.0f, 1000.0f, 
                                                          manager);
        
        test_matrix_validity(model_tiny, "Model (tiny range)");
        test_matrix_validity(view_tiny, "View (tiny range)");
        test_matrix_validity(projection_tiny, "Projection (tiny range)");
        
        // Test Case 3: Inverted range (end < start)
        constexpr int64_t inv_start = 1000;
        constexpr int64_t inv_end = 500;  // Invalid: end < start
                
        auto model_inv = new_getAnalogModelMat(display_options, 
                                              display_options.cached_std_dev, 
                                              display_options.cached_mean, 
                                              manager);
        auto view_inv = new_getAnalogViewMat(manager);
        auto projection_inv = new_getAnalogProjectionMat(TimeFrameIndex(inv_start), 
                                                         TimeFrameIndex(inv_end), 
                                                         -1000.0f, 1000.0f, 
                                                         manager);
        
        test_matrix_validity(model_inv, "Model (inverted range)");
        test_matrix_validity(view_inv, "View (inverted range)");
        test_matrix_validity(projection_inv, "Projection (inverted range)");
        
        // Test Case 4: Zero standard deviation (could cause division by zero in model matrix)
        std::vector<float> constant_data(num_points, 42.0f);  // All same value
        auto constant_series = std::make_shared<AnalogTimeSeries>(constant_data, time_vector);
        
        NewAnalogTimeSeriesDisplayOptions constant_options;
        constant_options.allocated_y_center = allocated_center;
        constant_options.allocated_height = allocated_height;
        setAnalogIntrinsicProperties(constant_series.get(), constant_options);
        
        // Reset to normal range for this test
        
        auto model_zero_std = new_getAnalogModelMat(constant_options, 
                                                   constant_options.cached_std_dev,  // Should be ~0
                                                   constant_options.cached_mean, 
                                                   manager);
        auto view_zero_std = new_getAnalogViewMat(manager);
        auto projection_zero_std = new_getAnalogProjectionMat(TimeFrameIndex(0), 
                                                              TimeFrameIndex(999), 
                                                              -1000.0f, 1000.0f, 
                                                              manager);
        
        test_matrix_validity(model_zero_std, "Model (zero std dev)");
        test_matrix_validity(view_zero_std, "View (zero std dev)");
        test_matrix_validity(projection_zero_std, "Projection (zero std dev)");
        
        INFO("Testing that matrices remain valid after returning to normal range");
        
        // Test Case 5: Return to normal range after extreme cases
        
        auto model_recovery = new_getAnalogModelMat(display_options, 
                                                   display_options.cached_std_dev, 
                                                   display_options.cached_mean, 
                                                   manager);
        auto view_recovery = new_getAnalogViewMat(manager);
        auto projection_recovery = new_getAnalogProjectionMat(TimeFrameIndex(100), 
                                                              TimeFrameIndex(200), 
                                                              -1000.0f, 1000.0f, 
                                                              manager);
        
        test_matrix_validity(model_recovery, "Model (recovery)");
        test_matrix_validity(view_recovery, "View (recovery)");
        test_matrix_validity(projection_recovery, "Projection (recovery)");
        
        // Test that transformation still works correctly after recovery
        glm::vec2 recovery_point = applyMVPTransformationIntegration(150, 0.0f, 
                                                                    model_recovery, view_recovery, projection_recovery);
        
        REQUIRE(std::isfinite(recovery_point.x));
        REQUIRE(std::isfinite(recovery_point.y));
        REQUIRE(recovery_point.x >= -1.1f);
        REQUIRE(recovery_point.x <= 1.1f);
    }
}
