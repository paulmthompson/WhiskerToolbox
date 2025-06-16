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
#include "TimeFrame.hpp"

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
        int gaussian_series = manager.addAnalogSeries();
        int uniform_series = manager.addAnalogSeries();
        int interval_series = manager.addDigitalIntervalSeries();

        // Verify series indices and counts
        REQUIRE(gaussian_series == 0);
        REQUIRE(uniform_series == 1);
        REQUIRE(interval_series == 0);// Digital interval indices are separate
        REQUIRE(manager.total_analog_series == 2);
        REQUIRE(manager.total_digital_series == 1);

        // Set visible data range
        manager.setVisibleDataRange(1, 10000);

        // Generate test data
        constexpr size_t num_points = 10000;
        std::vector<TimeFrameIndex> time_vector;
        for (size_t i = 0; i < num_points; ++i) {
            time_vector.push_back(TimeFrameIndex(i));
        }
        auto gaussian_data = generateGaussianDataIntegration(num_points, 0.0f, 10.0f, 42);
        auto uniform_data = generateUniformDataIntegration(num_points, 0.0f, 1.0f, 123);
        auto intervals = generateTestIntervalData(10, 10000.0f, 200.0f, 800.0f, 456);

        auto gaussian_time_series = std::make_shared<AnalogTimeSeries>(gaussian_data, time_vector);
        auto uniform_time_series = std::make_shared<AnalogTimeSeries>(uniform_data, time_vector);
        auto intervals_time_series = std::make_shared<DigitalIntervalSeries>(intervals);

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
        int analog1 = manager.addAnalogSeries();
        int analog2 = manager.addAnalogSeries();
        int event1 = manager.addDigitalEventSeries();
        int event2 = manager.addDigitalEventSeries();

        // Verify series indices and counts
        REQUIRE(analog1 == 0);
        REQUIRE(analog2 == 1);
        REQUIRE(event1 == 0);
        REQUIRE(event2 == 1);
        REQUIRE(manager.total_analog_series == 2);
        REQUIRE(manager.total_event_series == 2);

        // Set visible data range
        manager.setVisibleDataRange(1, 10000);

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
