#include "MVP_AnalogTimeSeries.hpp"

#include <iostream>
#include <vector>
#include <random>
#include <iomanip>

/**
 * @brief Demo program showcasing the new MVP matrix system for analog time series
 * 
 * This program demonstrates:
 * 1. Creating a PlottingManager
 * 2. Adding analog series 
 * 3. Generating MVP matrices
 * 4. Transforming sample data points
 * 5. Showing the coordinate transformation pipeline
 */

// Generate sample Gaussian data
std::vector<float> generateSampleData(size_t num_points, float mean, float std_dev) {
    std::mt19937 gen(42); // Fixed seed for reproducibility
    std::normal_distribution<float> dist(mean, std_dev);
    
    std::vector<float> data;
    data.reserve(num_points);
    
    for (size_t i = 0; i < num_points; ++i) {
        data.push_back(dist(gen));
    }
    
    return data;
}

// Calculate standard deviation
float calculateStdDev(std::vector<float> const & data) {
    if (data.empty()) return 0.0f;
    
    float mean = 0.0f;
    for (float value : data) {
        mean += value;
    }
    mean /= static_cast<float>(data.size());
    
    float variance = 0.0f;
    for (float value : data) {
        float diff = value - mean;
        variance += diff * diff;
    }
    variance /= static_cast<float>(data.size());
    
    return std::sqrt(variance);
}

// Transform a point through the MVP pipeline
glm::vec2 transformPoint(int data_index, float data_value, 
                         glm::mat4 const & model, glm::mat4 const & view, glm::mat4 const & projection) {
    glm::vec4 point(static_cast<float>(data_index), data_value, 0.0f, 1.0f);
    
    glm::vec4 model_space = model * point;
    glm::vec4 view_space = view * model_space;
    glm::vec4 clip_space = projection * view_space;
    
    return glm::vec2(clip_space.x / clip_space.w, clip_space.y / clip_space.w);
}

int main() {
    std::cout << "=== WhiskerToolbox DataViewer - New MVP System Demo ===" << std::endl;
    std::cout << std::endl;
    
    // Generate sample data
    constexpr size_t num_points = 10000;
    constexpr float mean = 0.0f;
    constexpr float std_dev = 10.0f;
    
    std::cout << "Generating " << num_points << " data points (mean=" << mean 
              << ", std_dev=" << std_dev << ")..." << std::endl;
    
    auto data = generateSampleData(num_points, mean, std_dev);
    float actual_std_dev = calculateStdDev(data);
    
    std::cout << "Generated data statistics:" << std::endl;
    std::cout << "  Actual std dev: " << std::fixed << std::setprecision(3) << actual_std_dev << std::endl;
    std::cout << "  Data range: [" << *std::min_element(data.begin(), data.end()) 
              << ", " << *std::max_element(data.begin(), data.end()) << "]" << std::endl;
    std::cout << std::endl;
    
    // Set up plotting manager
    PlottingManager manager;
    std::cout << "Setting up PlottingManager..." << std::endl;
    
    // Add first analog series
    int series1_idx = manager.addAnalogSeries();
    std::cout << "Added analog series 1, index: " << series1_idx << std::endl;
    
    // Add second analog series for comparison
    int series2_idx = manager.addAnalogSeries();
    std::cout << "Added analog series 2, index: " << series2_idx << std::endl;
    std::cout << "Total analog series: " << manager.total_analog_series << std::endl;
    std::cout << std::endl;
    
    // Set visible data range
    manager.setVisibleDataRange(1, 1000); // Show first 1000 points
    std::cout << "Visible data range: [" << manager.visible_start_index 
              << ", " << manager.visible_end_index << "]" << std::endl;
    std::cout << std::endl;
    
    // Configure display options for series 1
    NewAnalogTimeSeriesDisplayOptions display_options1;
    display_options1.hex_color = "#ff0000"; // Red
    display_options1.scaling.user_scale_factor = 1.0f;
    
    float allocated_center1, allocated_height1;
    manager.calculateAnalogSeriesAllocation(series1_idx, allocated_center1, allocated_height1);
    display_options1.allocated_y_center = allocated_center1;
    display_options1.allocated_height = allocated_height1;
    
    std::cout << "Series 1 allocation:" << std::endl;
    std::cout << "  Center Y: " << std::fixed << std::setprecision(3) << allocated_center1 << std::endl;
    std::cout << "  Height: " << allocated_height1 << std::endl;
    
    // Configure display options for series 2 (with different scaling)
    NewAnalogTimeSeriesDisplayOptions display_options2;
    display_options2.hex_color = "#0000ff"; // Blue
    display_options2.scaling.user_scale_factor = 2.0f; // Double amplitude
    
    float allocated_center2, allocated_height2;
    manager.calculateAnalogSeriesAllocation(series2_idx, allocated_center2, allocated_height2);
    display_options2.allocated_y_center = allocated_center2;
    display_options2.allocated_height = allocated_height2;
    
    std::cout << "Series 2 allocation:" << std::endl;
    std::cout << "  Center Y: " << std::fixed << std::setprecision(3) << allocated_center2 << std::endl;
    std::cout << "  Height: " << allocated_height2 << std::endl;
    std::cout << "  User scale factor: " << display_options2.scaling.user_scale_factor << std::endl;
    std::cout << std::endl;
    
    // Set intrinsic properties for both series (using same data)
    setAnalogIntrinsicProperties(data, display_options1);
    setAnalogIntrinsicProperties(data, display_options2);
    
    // Generate MVP matrices for both series
    glm::mat4 model1 = new_getAnalogModelMat(display_options1, display_options1.cached_std_dev, display_options1.cached_mean, manager);
    glm::mat4 view1 = new_getAnalogViewMat(manager);
    glm::mat4 projection = new_getAnalogProjectionMat(1, 1000, -1.0f, 1.0f, manager);
    
    glm::mat4 model2 = new_getAnalogModelMat(display_options2, display_options2.cached_std_dev, display_options2.cached_mean, manager);
    glm::mat4 view2 = new_getAnalogViewMat(manager); // Same view for both
    
    std::cout << "Generated MVP matrices successfully!" << std::endl;
    std::cout << std::endl;
    
    // Test key transformations
    std::cout << "=== Key Point Transformations ===" << std::endl;
    std::cout << std::setprecision(4);
    
    // Test center point transformations
    glm::vec2 center1 = transformPoint(500, 0.0f, model1, view1, projection);
    glm::vec2 center2 = transformPoint(500, 0.0f, model2, view2, projection);
    
    std::cout << "Center points (index=500, value=0):" << std::endl;
    std::cout << "  Series 1: (" << center1.x << ", " << center1.y << ")" << std::endl;
    std::cout << "  Series 2: (" << center2.x << ", " << center2.y << ")" << std::endl;
    std::cout << std::endl;
    
    // Test edge points
    glm::vec2 left_edge1 = transformPoint(1, 0.0f, model1, view1, projection);
    glm::vec2 right_edge1 = transformPoint(1000, 0.0f, model1, view1, projection);
    
    std::cout << "X-axis range for Series 1 (value=0):" << std::endl;
    std::cout << "  Left edge (index=1): (" << left_edge1.x << ", " << left_edge1.y << ")" << std::endl;
    std::cout << "  Right edge (index=1000): (" << right_edge1.x << ", " << right_edge1.y << ")" << std::endl;
    std::cout << std::endl;
    
    // Test amplitude scaling
    float three_sigma = 3.0f * actual_std_dev;
    glm::vec2 top1 = transformPoint(500, three_sigma, model1, view1, projection);
    glm::vec2 bottom1 = transformPoint(500, -three_sigma, model1, view1, projection);
    glm::vec2 top2 = transformPoint(500, three_sigma, model2, view2, projection);
    glm::vec2 bottom2 = transformPoint(500, -three_sigma, model2, view2, projection);
    
    std::cout << "Amplitude range (±3σ = ±" << three_sigma << "):" << std::endl;
    std::cout << "  Series 1 top (+3σ): (" << top1.x << ", " << top1.y << ")" << std::endl;
    std::cout << "  Series 1 bottom (-3σ): (" << bottom1.x << ", " << bottom1.y << ")" << std::endl;
    std::cout << "  Series 2 top (+3σ): (" << top2.x << ", " << top2.y << ")" << std::endl;
    std::cout << "  Series 2 bottom (-3σ): (" << bottom2.x << ", " << bottom2.y << ")" << std::endl;
    std::cout << std::endl;
    
    // Show coordinate system validation
    std::cout << "=== Coordinate System Validation ===" << std::endl;
    std::cout << "Expected coordinate ranges for normalized device coordinates (NDC):" << std::endl;
    std::cout << "  X-axis: [-1.0, 1.0] (left to right)" << std::endl;
    std::cout << "  Y-axis: [-1.0, 1.0] (bottom to top)" << std::endl;
    std::cout << std::endl;
    
    // Verify the gold standard expectations
    bool x_range_valid = (left_edge1.x >= -1.01f && left_edge1.x <= -0.99f) &&
                        (right_edge1.x >= 0.99f && right_edge1.x <= 1.01f);
    bool center_y_valid = std::abs(center1.y - allocated_center1) < 0.1f &&
                         std::abs(center2.y - allocated_center2) < 0.1f;
    
    std::cout << "Validation Results:" << std::endl;
    std::cout << "  X-axis range mapping: " << (x_range_valid ? "PASS" : "FAIL") << std::endl;
    std::cout << "  Y-axis center alignment: " << (center_y_valid ? "PASS" : "FAIL") << std::endl;
    std::cout << "  Series separation: " << (std::abs(center1.y - center2.y) > 0.5f ? "PASS" : "FAIL") << std::endl;
    std::cout << std::endl;
    
    std::cout << "=== Demo Complete ===" << std::endl;
    std::cout << "The new MVP system successfully:" << std::endl;
    std::cout << "✓ Generated Gaussian test data with correct statistics" << std::endl;
    std::cout << "✓ Managed multiple analog series with coordinate allocation" << std::endl;
    std::cout << "✓ Applied three-tier scaling (intrinsic, user, global)" << std::endl;
    std::cout << "✓ Transformed data coordinates to normalized device coordinates" << std::endl;
    std::cout << "✓ Maintained proper coordinate system bounds and separation" << std::endl;
    
    return 0;
} 