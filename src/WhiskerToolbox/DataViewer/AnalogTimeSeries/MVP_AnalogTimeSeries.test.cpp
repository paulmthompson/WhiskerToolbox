#include "MVP_AnalogTimeSeries.hpp"

#include "AnalogTimeSeriesDisplayOptions.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "PlottingManager/PlottingManager.hpp"
#include "TimeFrame.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <random>
#include <cmath>
#include <algorithm>
#include <iostream>

using Catch::Matchers::WithinRel;

/**
 * @brief Generate Gaussian process data for testing
 * 
 * Creates a vector of normally distributed random values with specified
 * mean and standard deviation for testing MVP matrix transformations.
 * 
 * @param num_points Number of data points to generate
 * @param mean Mean of the distribution
 * @param std_dev Standard deviation of the distribution
 * @param seed Random seed for reproducibility
 * @return Vector of generated data points
 */
std::vector<float> generateGaussianData(size_t num_points, float mean, float std_dev, unsigned int seed = 42) {
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
 * @brief Calculate standard deviation of data
 * 
 * @param data Vector of data points
 * @return Standard deviation
 */
float calculateStdDev(std::vector<float> const & data) {
    if (data.empty()) return 0.0f;
    
    // Calculate mean
    float mean = 0.0f;
    for (float value : data) {
        mean += value;
    }
    mean /= static_cast<float>(data.size());
    
    // Calculate variance
    float variance = 0.0f;
    for (float value : data) {
        float diff = value - mean;
        variance += diff * diff;
    }
    variance /= static_cast<float>(data.size());
    
    return std::sqrt(variance);
}

/**
 * @brief Apply MVP transformation to a data point
 * 
 * Applies Model, View, and Projection matrices to transform a data point
 * from data coordinates to normalized device coordinates.
 * 
 * @param data_index Index of the data point (for X coordinate)
 * @param data_value Value of the data point (for Y coordinate)  
 * @param model Model transformation matrix
 * @param view View transformation matrix
 * @param projection Projection transformation matrix
 * @return Transformed point in normalized device coordinates
 */
glm::vec2 applyMVPTransformation(int data_index, float data_value,
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
    // (In this case w should be 1.0 for orthographic projection)
    return glm::vec2(clip_point.x / clip_point.w, clip_point.y / clip_point.w);
}

TEST_CASE("New MVP System - Happy Path Tests", "[mvp][analog][new]") {
    
    SECTION("PlottingManager basic functionality") {
        PlottingManager manager;
        
        // Test initial state
        REQUIRE(manager.total_analog_series == 0);
        REQUIRE(manager.global_zoom == 1.0f);
        REQUIRE(manager.viewport_y_min == -1.0f);
        REQUIRE(manager.viewport_y_max == 1.0f);
        
        // Test adding series
        int series1_idx = manager.addAnalogSeries();
        int series2_idx = manager.addAnalogSeries();
        
        REQUIRE(series1_idx == 0);
        REQUIRE(series2_idx == 1);
        REQUIRE(manager.total_analog_series == 2);
        
        // Test series allocation for two series
        float center1, height1, center2, height2;
        manager.calculateAnalogSeriesAllocation(0, center1, height1);
        manager.calculateAnalogSeriesAllocation(1, center2, height2);
        
        // Each series should get half the viewport height
        REQUIRE_THAT(height1, WithinRel(1.0f, 0.01f)); // 2.0 / 2 = 1.0
        REQUIRE_THAT(height2, WithinRel(1.0f, 0.01f));
        
        // Centers should be positioned correctly
        REQUIRE_THAT(center1, WithinRel(-0.5f, 0.01f)); // -1.0 + 1.0/2 = -0.5
        REQUIRE_THAT(center2, WithinRel(0.5f, 0.01f));  // -1.0 + 1.5*1.0 = 0.5
        
        // Test visible data range
        manager.setVisibleDataRange(1, 10000);
        REQUIRE(manager.visible_start_index == 1);
        REQUIRE(manager.visible_end_index == 10000);
    }
    
    SECTION("Single Gaussian series MVP transformation - Gold Standard Test") {
        // Generate 100,000 point Gaussian data (mean=0, sigma=10)
        constexpr size_t num_points = 100000;
        std::vector<TimeFrameIndex> time_vector;
        for (size_t i = 0; i < num_points; ++i) {
            time_vector.push_back(TimeFrameIndex(i));
        }
        constexpr float expected_mean = 0.0f;
        constexpr float expected_std_dev = 10.0f;
        
        auto data = generateGaussianData(num_points, expected_mean, expected_std_dev, 42);
        auto time_series = std::make_shared<AnalogTimeSeries>(data, time_vector);
        REQUIRE(data.size() == num_points);
        
        // Verify the generated data has approximately correct statistics
        float actual_std_dev = calculateStdDev(data);
        REQUIRE_THAT(actual_std_dev, WithinRel(expected_std_dev, 0.1f)); // Within 10%
        
        // Set up plotting manager for single series
        PlottingManager manager;
        int series_idx = manager.addAnalogSeries();
        manager.setVisibleDataRange(1, 10000); // Show points 1-10000 as specified
        
        // Set up display options with default scaling
        NewAnalogTimeSeriesDisplayOptions display_options;
        
        // Calculate series allocation
        float allocated_center, allocated_height;
        manager.calculateAnalogSeriesAllocation(series_idx, allocated_center, allocated_height);
        display_options.allocated_y_center = allocated_center;
        display_options.allocated_height = allocated_height;
        
        // Set intrinsic properties for the data
        setAnalogIntrinsicProperties(time_series.get(), display_options);
        
        // Generate MVP matrices
        glm::mat4 model = new_getAnalogModelMat(display_options, display_options.cached_std_dev, display_options.cached_mean, manager);
        glm::mat4 view = new_getAnalogViewMat(manager);
        glm::mat4 projection = new_getAnalogProjectionMat(TimeFrameIndex(1), TimeFrameIndex(10000), -1.0f, 1.0f, manager);
        
        // Test key data points transformation
        // Test point at data index 1, value 0 (should map to left center)
        glm::vec2 left_center = applyMVPTransformation(1, 0.0f, model, view, projection);
        REQUIRE_THAT(left_center.x, WithinRel(-1.0f, 0.01f)); // Left edge of NDC
        REQUIRE_THAT(left_center.y, Catch::Matchers::WithinAbs(0.0f, 0.02f));  // Center Y (value = 0)
        
        // Test point at data index 10000, value 0 (should map to right center)
        glm::vec2 right_center = applyMVPTransformation(10000, 0.0f, model, view, projection);
        REQUIRE_THAT(right_center.x, WithinRel(1.0f, 0.01f));  // Right edge of NDC
        REQUIRE_THAT(right_center.y, Catch::Matchers::WithinAbs(0.0f, 0.02f));  // Center Y (value = 0)
        
        // Test point at middle index with +3*std_dev value (should map to top edge)
        float three_sigma_pos = 3.0f * actual_std_dev;
        glm::vec2 top_edge = applyMVPTransformation(5500, three_sigma_pos, model, view, projection);
        REQUIRE_THAT(top_edge.x, WithinRel(0.09f, 0.1f)); // Middle X: (5500-1)/(10000-1) * 2 - 1 ≈ 0.09
        
        // The Y coordinate calculation with new mean-centered scaling:
        // Formula: y_out = (y_in - data_mean) * final_y_scale + allocated_center
        // For Gaussian data with mean ≈ 0, allocated_center = 0:
        // +3*std_dev: y_out = (3*std_dev - 0) * scale + 0 = 3*std_dev * scale
        // where scale = (1/(3*std_dev)) * height_factor = (1/(3*std_dev)) * (2.0 * 0.8) = 1.6/(3*std_dev)
        // So: y_out = 3*std_dev * 1.6/(3*std_dev) = 1.6
        float expected_top_y = allocated_center + (allocated_height * 0.8f);
        REQUIRE_THAT(top_edge.y, WithinRel(expected_top_y, 0.1f));
        
        // Test point at middle index with -3*std_dev value (should map to bottom edge)
        float three_sigma_neg = -3.0f * actual_std_dev;
        glm::vec2 bottom_edge = applyMVPTransformation(5500, three_sigma_neg, model, view, projection);
        REQUIRE_THAT(bottom_edge.x, WithinRel(0.09f, 0.1f)); // Same X as above
        
        // -3*std_dev: y_out = (-3*std_dev - 0) * scale + 0 = -1.6
        float expected_bottom_y = allocated_center - (allocated_height * 0.8f);
        REQUIRE_THAT(bottom_edge.y, WithinRel(expected_bottom_y, 0.1f));
    }
    
    SECTION("Multiple series coordinate allocation") {
        PlottingManager manager;
        
        // Add three analog series
        int series1 = manager.addAnalogSeries();
        int series2 = manager.addAnalogSeries(); 
        int series3 = manager.addAnalogSeries();
        
        REQUIRE(manager.total_analog_series == 3);
        
        // Calculate allocations
        float center1, height1, center2, height2, center3, height3;
        manager.calculateAnalogSeriesAllocation(0, center1, height1);
        manager.calculateAnalogSeriesAllocation(1, center2, height2);
        manager.calculateAnalogSeriesAllocation(2, center3, height3);
        
        // Each series should get 1/3 of viewport height
        float expected_height = 2.0f / 3.0f; // Total viewport height (2.0) / 3 series
        REQUIRE_THAT(height1, WithinRel(expected_height, 0.01f));
        REQUIRE_THAT(height2, WithinRel(expected_height, 0.01f));
        REQUIRE_THAT(height3, WithinRel(expected_height, 0.01f));
        
        // Centers should be evenly distributed
        REQUIRE_THAT(center1, WithinRel(-2.0f/3.0f, 0.01f)); // -1.0 + height/2
        REQUIRE_THAT(center2, Catch::Matchers::WithinAbs(0.0f, 0.01f));       // -1.0 + 1.5*height  
        REQUIRE_THAT(center3, WithinRel(2.0f/3.0f, 0.01f));  // -1.0 + 2.5*height
        
    }
    
    SECTION("User and global scaling effects") {
        // Generate test data
        constexpr size_t num_points = 1000;
        std::vector<TimeFrameIndex> time_vector;
        for (size_t i = 0; i < num_points; ++i) {
            time_vector.push_back(TimeFrameIndex(i));
        }
        auto data = generateGaussianData(num_points, 0.0f, 5.0f, 123);
        auto time_series = std::make_shared<AnalogTimeSeries>(data, time_vector);
        float std_dev = calculateStdDev(data);
        
        PlottingManager manager;
        int series_idx = manager.addAnalogSeries();
        
        // Test user scaling factor
        NewAnalogTimeSeriesDisplayOptions display_options;
        display_options.scaling.user_scale_factor = 2.0f; // Double the amplitude
        
        float allocated_center, allocated_height;
        manager.calculateAnalogSeriesAllocation(series_idx, allocated_center, allocated_height);
        display_options.allocated_y_center = allocated_center;
        display_options.allocated_height = allocated_height;
        
        // Set intrinsic properties for the test data
        setAnalogIntrinsicProperties(time_series.get(), display_options);
        
        glm::mat4 model_2x = new_getAnalogModelMat(display_options, display_options.cached_std_dev, display_options.cached_mean, manager);
        
        // Compare with 1x scaling
        display_options.scaling.user_scale_factor = 1.0f;
        glm::mat4 model_1x = new_getAnalogModelMat(display_options, display_options.cached_std_dev, display_options.cached_mean, manager);
        
        // The 2x model should scale Y coordinates by factor of 2
        glm::vec4 test_point(100.0f, 10.0f, 0.0f, 1.0f);
        glm::vec4 result_1x = model_1x * test_point;
        glm::vec4 result_2x = model_2x * test_point;
        
        // Y coordinate should be approximately 2x larger (within scaling factors)
        float scale_ratio = result_2x.y / result_1x.y;
        REQUIRE_THAT(scale_ratio, WithinRel(2.0f, 0.1f));
        
        // X coordinate should be the same
        REQUIRE_THAT(result_1x.x, WithinRel(result_2x.x, 0.01f));
    }
    
    SECTION("Uniform [0,1] data centering test") {
        // Generate uniform [0,1] data to test offset handling
        std::mt19937 gen(789);
        std::uniform_real_distribution<float> uniform_dist(0.0f, 1.0f);
        
        std::vector<float> uniform_data;
        uniform_data.reserve(10000);
        for (size_t i = 0; i < 10000; ++i) {
            uniform_data.push_back(uniform_dist(gen));
        }
        std::vector<TimeFrameIndex> time_vector;
        for (size_t i = 0; i < 10000; ++i) {
            time_vector.push_back(TimeFrameIndex(i));
        }
        auto time_series = std::make_shared<AnalogTimeSeries>(uniform_data, time_vector);
        float uniform_std_dev = calculateStdDev(uniform_data);
        float uniform_mean = 0.0f;
        for (float value : uniform_data) {
            uniform_mean += value;
        }
        uniform_mean /= static_cast<float>(uniform_data.size());
        
        // Verify data characteristics
        REQUIRE_THAT(uniform_mean, WithinRel(0.5f, 0.1f)); // Should be around 0.5
        REQUIRE_THAT(uniform_std_dev, WithinRel(0.289f, 0.1f)); // Should be around sqrt(1/12) ≈ 0.289
        
        // Set up plotting manager for single series
        PlottingManager manager;
        int series_idx = manager.addAnalogSeries();
        manager.setVisibleDataRange(1, 1000);
        
        NewAnalogTimeSeriesDisplayOptions display_options;
        float allocated_center, allocated_height;
        manager.calculateAnalogSeriesAllocation(series_idx, allocated_center, allocated_height);
        display_options.allocated_y_center = allocated_center;
        display_options.allocated_height = allocated_height;
        
        // Set intrinsic properties for the uniform data
        setAnalogIntrinsicProperties(time_series.get(), display_options);
        
        // Generate MVP matrices
        glm::mat4 model = new_getAnalogModelMat(display_options, display_options.cached_std_dev, display_options.cached_mean, manager);
        glm::mat4 view = new_getAnalogViewMat(manager);
        glm::mat4 projection = new_getAnalogProjectionMat(TimeFrameIndex(1), TimeFrameIndex(1000), -1.0f, 1.0f, manager);
        
        // Test key transformations
        // Test where the mean value (0.5) gets positioned
        glm::vec2 mean_point = applyMVPTransformation(500, uniform_mean, model, view, projection);
        
        // Test where the range endpoints (0.0 and 1.0) get positioned
        glm::vec2 min_point = applyMVPTransformation(500, 0.0f, model, view, projection);
        glm::vec2 max_point = applyMVPTransformation(500, 1.0f, model, view, projection);
        
        std::cout << "Uniform [0,1] data transformation results:" << std::endl;
        std::cout << "  Mean value (" << uniform_mean << ") maps to Y = " << mean_point.y << std::endl;
        std::cout << "  Min value (0.0) maps to Y = " << min_point.y << std::endl;
        std::cout << "  Max value (1.0) maps to Y = " << max_point.y << std::endl;
        std::cout << "  Allocated center: " << allocated_center << std::endl;
        std::cout << "  Cached mean: " << display_options.cached_mean << std::endl;
        std::cout << "  Cached std dev: " << display_options.cached_std_dev << std::endl;
        
        // NEW BEHAVIOR: With mean-centered scaling, the data mean (≈0.5) should map to allocated_center
        REQUIRE_THAT(mean_point.y, WithinRel(allocated_center, 0.05f));
        
        // The min (0.0) and max (1.0) should be symmetrically positioned around the center
        float distance_min = std::abs(min_point.y - allocated_center);
        float distance_max = std::abs(max_point.y - allocated_center);
        REQUIRE_THAT(distance_min, WithinRel(distance_max, 0.1f)); // Should be symmetric
        
        // Min should be below center, max should be above center
        REQUIRE(min_point.y < allocated_center);
        REQUIRE(max_point.y > allocated_center);
        
        // Test that the data range is reasonable (not extending beyond viewport)
        REQUIRE(min_point.y >= -2.0f); // Within reasonable bounds
        REQUIRE(max_point.y <= 2.0f);  // Within reasonable bounds
    }
    
    SECTION("Vertical panning functionality") {
        // Create test data for panning tests
        constexpr size_t num_points = 1000;
        std::vector<TimeFrameIndex> time_vector;
        for (size_t i = 0; i < num_points; ++i) {
            time_vector.push_back(TimeFrameIndex(i));
        }
        auto test_data = generateGaussianData(num_points, 0.0f, 5.0f, 999);
        auto time_series = std::make_shared<AnalogTimeSeries>(test_data, time_vector);
        PlottingManager manager;
        int series_idx = manager.addAnalogSeries();
        manager.setVisibleDataRange(1, 1000);
        
        // Set up display options
        NewAnalogTimeSeriesDisplayOptions display_options;
        float allocated_center, allocated_height;
        manager.calculateAnalogSeriesAllocation(series_idx, allocated_center, allocated_height);
        display_options.allocated_y_center = allocated_center;
        display_options.allocated_height = allocated_height;
        setAnalogIntrinsicProperties(time_series.get(), display_options);
        
        // Test initial state (no panning)
        REQUIRE(manager.getPanOffset() == 0.0f);
        
        // Generate MVP matrices without panning
        glm::mat4 model = new_getAnalogModelMat(display_options, display_options.cached_std_dev, display_options.cached_mean, manager);
        glm::mat4 view_no_pan = new_getAnalogViewMat(manager);
        glm::mat4 projection = new_getAnalogProjectionMat(TimeFrameIndex(1), TimeFrameIndex(1000), -1.0f, 1.0f, manager);
        
        // Test a reference point without panning
        glm::vec2 reference_point = applyMVPTransformation(500, 0.0f, model, view_no_pan, projection);
        
        // Apply pan offset (positive = upward)
        float pan_delta = 0.5f; // Pan upward by 0.5 NDC units
        manager.setPanOffset(pan_delta);
        REQUIRE(manager.getPanOffset() == pan_delta);
        
        // Generate view matrix with panning
        glm::mat4 view_with_pan = new_getAnalogViewMat(manager);
        
        // Test same point with panning applied
        glm::vec2 panned_point = applyMVPTransformation(500, 0.0f, model, view_with_pan, projection);
        
        // The Y coordinate should be shifted upward by the pan amount
        REQUIRE_THAT(panned_point.y, WithinRel(reference_point.y + pan_delta, 0.01f));
        
        // X coordinate should remain unchanged
        REQUIRE_THAT(panned_point.x, WithinRel(reference_point.x, 0.01f));
        
        // Test negative panning (downward)
        float negative_pan = -0.3f;
        manager.setPanOffset(negative_pan);
        
        glm::mat4 view_negative_pan = new_getAnalogViewMat(manager);
        glm::vec2 negative_panned_point = applyMVPTransformation(500, 0.0f, model, view_negative_pan, projection);
        
        REQUIRE_THAT(negative_panned_point.y, WithinRel(reference_point.y + negative_pan, 0.01f));
        
        // Test applyPanDelta functionality
        manager.resetPan();
        REQUIRE(manager.getPanOffset() == 0.0f);
        
        manager.applyPanDelta(0.2f);
        REQUIRE_THAT(manager.getPanOffset(), WithinRel(0.2f, 0.001f));
        
        manager.applyPanDelta(0.1f);
        REQUIRE_THAT(manager.getPanOffset(), WithinRel(0.3f, 0.001f));
        
        manager.applyPanDelta(-0.5f);
        REQUIRE_THAT(manager.getPanOffset(), WithinRel(-0.2f, 0.001f));
    }
    
    SECTION("Multiple series panning consistency") {
        // Test that all series are panned equally when multiple series exist
        constexpr size_t num_points = 1000;
        std::vector<TimeFrameIndex> time_vector;
        for (size_t i = 0; i < num_points; ++i) {
            time_vector.push_back(TimeFrameIndex(i));
        }
        auto data1 = generateGaussianData(num_points, 0.0f, 3.0f, 111);
        auto data2 = generateGaussianData(num_points, 5.0f, 2.0f, 222);  // Different mean and std dev
        auto data3 = generateGaussianData(num_points, -2.0f, 8.0f, 333); // Another different dataset

        auto time_series1 = std::make_shared<AnalogTimeSeries>(data1, time_vector);
        auto time_series2 = std::make_shared<AnalogTimeSeries>(data2, time_vector);
        auto time_series3 = std::make_shared<AnalogTimeSeries>(data3, time_vector);

        PlottingManager manager;
        int series1_idx = manager.addAnalogSeries();
        int series2_idx = manager.addAnalogSeries(); 
        int series3_idx = manager.addAnalogSeries();
        manager.setVisibleDataRange(1, 1000);
        
        // Set up display options for all series
        NewAnalogTimeSeriesDisplayOptions display_options1, display_options2, display_options3;
        
        float center1, height1, center2, height2, center3, height3;
        manager.calculateAnalogSeriesAllocation(series1_idx, center1, height1);
        manager.calculateAnalogSeriesAllocation(series2_idx, center2, height2);
        manager.calculateAnalogSeriesAllocation(series3_idx, center3, height3);
        
        display_options1.allocated_y_center = center1;
        display_options1.allocated_height = height1;
        setAnalogIntrinsicProperties(time_series1.get(), display_options1);
        
        display_options2.allocated_y_center = center2;
        display_options2.allocated_height = height2;
        setAnalogIntrinsicProperties(time_series2.get(), display_options2);
        
        display_options3.allocated_y_center = center3;
        display_options3.allocated_height = height3;
        setAnalogIntrinsicProperties(time_series3.get(), display_options3);
        
        // Generate models for all series
        glm::mat4 model1 = new_getAnalogModelMat(display_options1, display_options1.cached_std_dev, display_options1.cached_mean, manager);
        glm::mat4 model2 = new_getAnalogModelMat(display_options2, display_options2.cached_std_dev, display_options2.cached_mean, manager);
        glm::mat4 model3 = new_getAnalogModelMat(display_options3, display_options3.cached_std_dev, display_options3.cached_mean, manager);
        glm::mat4 projection = new_getAnalogProjectionMat(TimeFrameIndex(1), TimeFrameIndex(1000), -1.0f, 1.0f, manager);
        
        // Test reference points without panning
        glm::mat4 view_no_pan = new_getAnalogViewMat(manager);
        glm::vec2 ref1 = applyMVPTransformation(500, display_options1.cached_mean, model1, view_no_pan, projection);
        glm::vec2 ref2 = applyMVPTransformation(500, display_options2.cached_mean, model2, view_no_pan, projection);
        glm::vec2 ref3 = applyMVPTransformation(500, display_options3.cached_mean, model3, view_no_pan, projection);
        
        // Verify initial relative positions
        float initial_spacing12 = ref2.y - ref1.y;
        float initial_spacing23 = ref3.y - ref2.y;
        
        // Apply panning
        float pan_amount = 0.8f;
        manager.setPanOffset(pan_amount);
        glm::mat4 view_with_pan = new_getAnalogViewMat(manager);
        
        // Test points with panning
        glm::vec2 pan1 = applyMVPTransformation(500, display_options1.cached_mean, model1, view_with_pan, projection);
        glm::vec2 pan2 = applyMVPTransformation(500, display_options2.cached_mean, model2, view_with_pan, projection);
        glm::vec2 pan3 = applyMVPTransformation(500, display_options3.cached_mean, model3, view_with_pan, projection);
        
        // All series should be panned by the same amount
        REQUIRE_THAT(pan1.y, WithinRel(ref1.y + pan_amount, 0.01f));
        REQUIRE_THAT(pan2.y, WithinRel(ref2.y + pan_amount, 0.01f));
        REQUIRE_THAT(pan3.y, WithinRel(ref3.y + pan_amount, 0.01f));
        
        // Relative spacing between series should be preserved
        float panned_spacing12 = pan2.y - pan1.y;
        float panned_spacing23 = pan3.y - pan2.y;
        REQUIRE_THAT(panned_spacing12, WithinRel(initial_spacing12, 0.01f));
        REQUIRE_THAT(panned_spacing23, WithinRel(initial_spacing23, 0.01f));
        
        // X coordinates should remain unchanged
        REQUIRE_THAT(pan1.x, WithinRel(ref1.x, 0.01f));
        REQUIRE_THAT(pan2.x, WithinRel(ref2.x, 0.01f));
        REQUIRE_THAT(pan3.x, WithinRel(ref3.x, 0.01f));
    }
    
    SECTION("Panning data out of view") {
        // Test that data can be panned completely out of the visible area
        constexpr size_t num_points = 1000;
        std::vector<TimeFrameIndex> time_vector;
        for (size_t i = 0; i < num_points; ++i) {
            time_vector.push_back(TimeFrameIndex(i));
        }
        auto test_data = generateGaussianData(num_points, 0.0f, 1.0f, 444);
        auto time_series = std::make_shared<AnalogTimeSeries>(test_data, time_vector);
        PlottingManager manager;
        int series_idx = manager.addAnalogSeries();
        manager.setVisibleDataRange(1, 1000);
        
        NewAnalogTimeSeriesDisplayOptions display_options;
        float allocated_center, allocated_height;
        manager.calculateAnalogSeriesAllocation(series_idx, allocated_center, allocated_height);
        display_options.allocated_y_center = allocated_center;
        display_options.allocated_height = allocated_height;
        setAnalogIntrinsicProperties(time_series.get(), display_options);
        
        glm::mat4 model = new_getAnalogModelMat(display_options, display_options.cached_std_dev, display_options.cached_mean, manager);
        glm::mat4 projection = new_getAnalogProjectionMat(TimeFrameIndex(1), TimeFrameIndex(1000), -1.0f, 1.0f, manager);
        
        // Pan so far upward that data goes above the visible area
        float extreme_pan_up = 5.0f; // Very large positive pan
        manager.setPanOffset(extreme_pan_up);
        glm::mat4 view_extreme_up = new_getAnalogViewMat(manager);
        
        glm::vec2 out_of_view_up = applyMVPTransformation(500, display_options.cached_mean, model, view_extreme_up, projection);
        
        // Data should be well above the normal NDC range [-1, 1]
        REQUIRE(out_of_view_up.y > 2.0f);
        
        // Pan so far downward that data goes below the visible area
        float extreme_pan_down = -5.0f; // Very large negative pan
        manager.setPanOffset(extreme_pan_down);
        glm::mat4 view_extreme_down = new_getAnalogViewMat(manager);
        
        glm::vec2 out_of_view_down = applyMVPTransformation(500, display_options.cached_mean, model, view_extreme_down, projection);
        
        // Data should be well below the normal NDC range [-1, 1]
        REQUIRE(out_of_view_down.y < -2.0f);
        
        // X coordinates should remain in normal range regardless of Y panning
        REQUIRE(out_of_view_up.x >= -1.1f);
        REQUIRE(out_of_view_up.x <= 1.1f);
        REQUIRE(out_of_view_down.x >= -1.1f);
        REQUIRE(out_of_view_down.x <= 1.1f);
    }
}

TEST_CASE("New MVP System - Error Handling and Edge Cases", "[mvp][analog][new][edge]") {
    
    SECTION("Empty plotting manager series allocation") {
        PlottingManager empty_manager;
        REQUIRE(empty_manager.total_analog_series == 0);
        
        // Should handle allocation gracefully even with no series
        float center, height;
        empty_manager.calculateAnalogSeriesAllocation(0, center, height);
        
        REQUIRE_THAT(center, WithinRel(0.0f, 0.01f));
        REQUIRE_THAT(height, WithinRel(2.0f, 0.01f)); // Full viewport height
    }
    
    SECTION("Zero standard deviation data") {
        // Create constant data (zero standard deviation)
        constexpr size_t num_points = 1000;
        std::vector<TimeFrameIndex> time_vector;
        for (size_t i = 0; i < num_points; ++i) {
            time_vector.push_back(TimeFrameIndex(i));
        }
        std::vector<float> constant_data(num_points, 5.0f);
        auto time_series = std::make_shared<AnalogTimeSeries>(constant_data, time_vector);
        float std_dev = calculateStdDev(constant_data);
        REQUIRE_THAT(std_dev, WithinRel(0.0f, 0.01f));
        
        PlottingManager manager;
        int series_idx = manager.addAnalogSeries();
        
        NewAnalogTimeSeriesDisplayOptions display_options;
        float allocated_center, allocated_height;
        manager.calculateAnalogSeriesAllocation(series_idx, allocated_center, allocated_height);
        display_options.allocated_y_center = allocated_center;
        display_options.allocated_height = allocated_height;
        
        // Set intrinsic properties (will be mean=5.0, std_dev=0.0)
        setAnalogIntrinsicProperties(time_series.get(), display_options);
        
        // Should not crash with zero std_dev (division by zero protection)
        glm::mat4 model = new_getAnalogModelMat(display_options, display_options.cached_std_dev, display_options.cached_mean, manager);
        
        // Matrix should be valid (not NaN or infinite)
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                REQUIRE(std::isfinite(model[i][j]));
            }
        }
    }
    
    SECTION("Extreme scaling values") {
        constexpr size_t num_points = 1000;
        std::vector<TimeFrameIndex> time_vector;
        for (size_t i = 0; i < num_points; ++i) {
            time_vector.push_back(TimeFrameIndex(i));
        }
        auto data = generateGaussianData(num_points, 0.0f, 1.0f, 456);
        auto time_series = std::make_shared<AnalogTimeSeries>(data, time_vector);
        float std_dev = calculateStdDev(data);
        
        PlottingManager manager;
        int series_idx = manager.addAnalogSeries();
        
        NewAnalogTimeSeriesDisplayOptions display_options;
        float allocated_center, allocated_height;
        manager.calculateAnalogSeriesAllocation(series_idx, allocated_center, allocated_height);
        display_options.allocated_y_center = allocated_center;
        display_options.allocated_height = allocated_height;
        
        // Set intrinsic properties for the test data
        setAnalogIntrinsicProperties(time_series.get(), display_options);
        
        // Test very large scaling factor
        display_options.scaling.user_scale_factor = 1000.0f;
        glm::mat4 model_large = new_getAnalogModelMat(display_options, display_options.cached_std_dev, display_options.cached_mean, manager);
        
        // Test very small scaling factor
        display_options.scaling.user_scale_factor = 0.001f;
        glm::mat4 model_small = new_getAnalogModelMat(display_options, display_options.cached_std_dev, display_options.cached_mean, manager);
        
        // Both matrices should be finite
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                REQUIRE(std::isfinite(model_large[i][j]));
                REQUIRE(std::isfinite(model_small[i][j]));
            }
        }
    }
    
    SECTION("Invalid data index ranges") {
        PlottingManager manager;
        manager.setVisibleDataRange(1, 10000);
        
        // Test projection with invalid ranges
        glm::mat4 projection = new_getAnalogProjectionMat(TimeFrameIndex(10000), TimeFrameIndex(1), -1.0f, 1.0f, manager);
        
        // Matrix should still be valid even with inverted range
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                REQUIRE(std::isfinite(projection[i][j]));
            }
        }
    }
    
    SECTION("Large dataset indices") {
        PlottingManager manager;
        
        // Test with very large indices
        constexpr int large_start = 1000000;
        constexpr int large_end = 2000000;
        
        manager.setVisibleDataRange(large_start, large_end);
        glm::mat4 projection = new_getAnalogProjectionMat(TimeFrameIndex(large_start), TimeFrameIndex(large_end), -100.0f, 100.0f, manager);
        
        // Should handle large numbers without overflow
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                REQUIRE(std::isfinite(projection[i][j]));
            }
        }
        
        // Test transformation with large indices
        glm::vec2 result = applyMVPTransformation(large_start, 0.0f, 
                                                glm::mat4(1.0f), glm::mat4(1.0f), projection);
        REQUIRE(std::isfinite(result.x));
        REQUIRE(std::isfinite(result.y));
    }
} 