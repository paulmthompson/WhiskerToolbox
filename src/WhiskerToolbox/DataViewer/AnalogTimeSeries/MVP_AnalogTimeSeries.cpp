#include "MVP_AnalogTimeSeries.hpp"

#include "DisplayOptions/TimeSeriesDisplayOptions.hpp"
#include "PlottingManager/PlottingManager.hpp"

#include <cmath>
#include <iostream>

// Model Matrix: Handles series-specific positioning and scaling for analog series
glm::mat4 getAnalogModelMat(AnalogTimeSeriesDisplayOptions const * display_options,
                            std::string const & key,
                            float stdDev,
                            int series_index,
                            float ySpacing,
                            float center_coord,
                            float global_zoom) {
    auto Model = glm::mat4(1.0f);

    // Check if this series has been positioned by VerticalSpaceManager
    // (VerticalSpaceManager sets a specific y_offset value and allocated_height > 0)
    if (display_options->y_offset != 0.0f && display_options->allocated_height > 0.0f) {
        // VerticalSpaceManager positioning: use calculated position and allocated space
        float const series_center_y = display_options->y_offset;

        // Calculate amplitude scaling based on allocated height and data characteristics
        // Use a reasonable portion of the allocated height for amplitude display
        float const usable_height = display_options->allocated_height * 0.8f;// 80% of allocated space

        // Scale amplitude to fit within the usable height
        // Use the original approach but base it on allocated space rather than corrupted scale_factor
        float const base_amplitude_scale = 1.0f / stdDev;// Basic amplitude normalization
        float const height_scale = usable_height / 1.0f; // Scale to fit in allocated height

        // Apply user controls: global zoom and user scale factor
        float const user_controls = display_options->user_scale_factor * global_zoom;
        float const amplitude_scale = base_amplitude_scale * height_scale * user_controls;

        // Apply translation: move to calculated center position
        Model = glm::translate(Model, glm::vec3(0.0f, series_center_y, 0.0f));

        // Apply scaling: scale amplitude to fit in allocated space
        Model = glm::scale(Model, glm::vec3(1.0f, amplitude_scale, 1.0f));

        std::cout << "  Analog '" << key << "' MVP: center_y=" << series_center_y
                  << ", allocated_height=" << display_options->allocated_height
                  << ", stdDev=" << stdDev << ", user_controls=" << user_controls
                  << ", amplitude_scale=" << amplitude_scale << std::endl;
    } else {
        // Legacy index-based positioning for backward compatibility
        float const y_offset = static_cast<float>(series_index) * ySpacing;

        // Apply translation: position based on index and spacing
        Model = glm::translate(Model, glm::vec3(0.0f, y_offset, 0.0f));

        // Center all series around zero
        Model = glm::translate(Model, glm::vec3(0.0f, center_coord, 0.0f));

        // Apply scaling: scale based on actual standard deviation and user settings
        // Fixed: Apply both user scale factor AND global zoom consistently
        float const legacy_amplitude_scale = (1.0f / stdDev) * display_options->user_scale_factor * global_zoom;
        Model = glm::scale(Model, glm::vec3(1.0f, legacy_amplitude_scale, 1.0f));
    }

    return Model;
}

// View Matrix: Handles global panning (applied to all series equally) for analog series
// Note: Pan offset is now handled in Projection matrix for dynamic viewport
glm::mat4 getAnalogViewMat() {
    auto View = glm::mat4(1.0f);
    // View = glm::translate(View, glm::vec3(0, _verticalPanOffset, 0)); // Moved to Projection

    return View;
}

// Projection Matrix: Maps world coordinates to screen coordinates for analog series
glm::mat4 getAnalogProjectionMat(float start_time,
                                 float end_time,
                                 float yMin,
                                 float yMax,
                                 float verticalPanOffset) {
    // Projection Matrix: Maps world coordinates to screen coordinates
    // - X axis: maps time range [start_time, end_time] to screen width
    // - Y axis: maps world Y coordinates [min_y, max_y] to screen height
    //
    // For dynamic viewport: adjust Y bounds based on content and pan offset
    float const dynamic_min_y = yMin + verticalPanOffset;
    float const dynamic_max_y = yMax + verticalPanOffset;

    auto Projection = glm::ortho(start_time, end_time, dynamic_min_y, dynamic_max_y);

    return Projection;
}

// Helper functions for intrinsic data properties
float calculateDataMean(std::vector<float> const & data) {
    if (data.empty()) return 0.0f;

    float sum = 0.0f;
    for (float value: data) {
        sum += value;
    }

    return sum / static_cast<float>(data.size());
}

void setAnalogIntrinsicProperties(std::vector<float> const & data,
                                  NewAnalogTimeSeriesDisplayOptions & display_options) {
    if (data.empty()) {
        display_options.cached_mean = 0.0f;
        display_options.cached_std_dev = 0.0f;
        display_options.mean_cache_valid = true;
        display_options.std_dev_cache_valid = true;
        return;
    }

    // Calculate mean
    display_options.cached_mean = calculateDataMean(data);
    display_options.mean_cache_valid = true;

    // Calculate standard deviation
    float variance = 0.0f;
    for (float value: data) {
        float diff = value - display_options.cached_mean;
        variance += diff * diff;
    }
    variance /= static_cast<float>(data.size());

    display_options.cached_std_dev = std::sqrt(variance);
    display_options.std_dev_cache_valid = true;
}


// New MVP matrix functions
glm::mat4 new_getAnalogModelMat(NewAnalogTimeSeriesDisplayOptions const & display_options,
                                float std_dev,
                                float data_mean,
                                PlottingManager const & plotting_manager) {
    // Calculate intrinsic scaling (3 standard deviations for full range)
    // This maps ±3*std_dev (from the mean) to ±1.0 in normalized space
    // Protect against division by zero
    float const safe_std_dev = (std_dev > 1e-6f) ? std_dev : 1.0f;
    float const intrinsic_scale = 1.0f / (3.0f * safe_std_dev);

    // Combine all scaling factors: intrinsic, user, and global
    float const total_y_scale = intrinsic_scale *
                                display_options.scaling.intrinsic_scale *
                                display_options.scaling.user_scale_factor *
                                display_options.scaling.global_zoom *
                                plotting_manager.global_zoom *
                                plotting_manager.global_vertical_scale;

    // Scale to fit within allocated height (use 80% of allocated space for safety)
    // This means ±3*std_dev (from mean) will span ±80% of the allocated height
    float const height_scale = display_options.allocated_height * 0.8f;
    float const final_y_scale = total_y_scale * height_scale;

    // Build the transformation to achieve: (data_value - data_mean) * scale + allocated_center
    // This ensures data_mean maps exactly to allocated_center

    // Method: Use matrix construction to implement the formula directly
    // For Y coordinate: y_out = (y_in - data_mean) * final_y_scale + allocated_center
    // This can be written as: y_out = y_in * final_y_scale - data_mean * final_y_scale + allocated_center
    // So: y_out = y_in * final_y_scale + (allocated_center - data_mean * final_y_scale)

    float const y_offset = display_options.allocated_y_center - data_mean * final_y_scale;

    // Use explicit matrix construction to avoid confusion with GLM order
    // We want the transformation: y_out = y_in * final_y_scale + y_offset
    // This is an affine transformation: [scale 0 offset; 0 scale 0; 0 0 1]

    glm::mat4 Model(1.0f);
    Model[1][1] = final_y_scale;// Y scaling
    Model[3][1] = y_offset;     // Y translation

    // Apply any additional user-specified offsets
    if (display_options.scaling.user_vertical_offset != 0.0f) {
        Model = glm::translate(Model, glm::vec3(0.0f, display_options.scaling.user_vertical_offset, 0.0f));
    }

    return Model;
}

glm::mat4 new_getAnalogViewMat(PlottingManager const & plotting_manager) {
    auto View = glm::mat4(1.0f);

    // Apply global vertical panning
    if (plotting_manager.vertical_pan_offset != 0.0f) {
        View = glm::translate(View, glm::vec3(0.0f, plotting_manager.vertical_pan_offset, 0.0f));
    }

    return View;
}

glm::mat4 new_getAnalogProjectionMat(int start_data_index,
                                     int end_data_index,
                                     float y_min,
                                     float y_max,
                                     PlottingManager const & plotting_manager) {
    // Map data indices to normalized device coordinates [-1, 1]
    // X-axis: map [start_data_index, end_data_index] to screen width
    // Y-axis: map [y_min, y_max] to viewport height with pan offset

    auto const data_start = static_cast<float>(start_data_index);
    auto const data_end = static_cast<float>(end_data_index);

    // Apply vertical pan offset to Y bounds
    float const adjusted_y_min = y_min + plotting_manager.vertical_pan_offset;
    float const adjusted_y_max = y_max + plotting_manager.vertical_pan_offset;

    // Create orthographic projection matrix
    // This maps world coordinates to normalized device coordinates [-1, 1]
    auto Projection = glm::ortho(data_start, data_end,
                                 adjusted_y_min, adjusted_y_max);

    return Projection;
}
