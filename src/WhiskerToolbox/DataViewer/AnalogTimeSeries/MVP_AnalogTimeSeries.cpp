#include "MVP_AnalogTimeSeries.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/utils/statistics.hpp"
#include "PlottingManager/PlottingManager.hpp"

#include <cmath>
#include <iostream>


void setAnalogIntrinsicProperties(AnalogTimeSeries const * analog,
                                  NewAnalogTimeSeriesDisplayOptions & display_options) {
    if (!analog) {
        display_options.data_cache.cached_mean = 0.0f;
        display_options.data_cache.cached_std_dev = 0.0f;
        display_options.data_cache.mean_cache_valid = true;
        display_options.data_cache.std_dev_cache_valid = true;
        return;
    }

    // Calculate mean
    display_options.data_cache.cached_mean = calculate_mean(*analog);
    display_options.data_cache.mean_cache_valid = true;

    display_options.data_cache.cached_std_dev = calculate_std_dev_approximate(*analog);
    display_options.data_cache.std_dev_cache_valid = true;
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
    float const height_scale = display_options.layout.allocated_height * 0.8f;
    float const final_y_scale = total_y_scale * height_scale;

    // Build the transformation to achieve: (data_value - data_mean) * scale + allocated_center
    // This ensures data_mean maps exactly to allocated_center

    // Method: Use matrix construction to implement the formula directly
    // For Y coordinate: y_out = (y_in - data_mean) * final_y_scale + allocated_center
    // This can be written as: y_out = y_in * final_y_scale - data_mean * final_y_scale + allocated_center
    // So: y_out = y_in * final_y_scale + (allocated_center - data_mean * final_y_scale)

    float const y_offset = display_options.layout.allocated_y_center - data_mean * final_y_scale;

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

glm::mat4 new_getAnalogProjectionMat(TimeFrameIndex start_data_index,
                                     TimeFrameIndex end_data_index,
                                     float y_min,
                                     float y_max,
                                     PlottingManager const & plotting_manager) {

    static_cast<void>(plotting_manager);
    // Map data indices to normalized device coordinates [-1, 1]
    // X-axis: map [start_data_index, end_data_index] to screen width
    // Y-axis: map [y_min, y_max] to viewport height
    //
    // Note: Pan offset is handled in the View matrix for analog series,
    // so we don't apply it here to avoid double application

    auto const data_start = static_cast<float>(start_data_index.getValue());
    auto const data_end = static_cast<float>(end_data_index.getValue());

    // Validate and sanitize input parameters to prevent OpenGL state corruption
    float safe_data_start = data_start;
    float safe_data_end = data_end;
    float safe_y_min = y_min;
    float safe_y_max = y_max;
    
    // Check for and fix invalid or extreme values that could cause NaN/Infinity
    
    // 1. Ensure all values are finite
    if (!std::isfinite(safe_data_start)) {
        std::cout << "Warning: Invalid data_start=" << safe_data_start << ", using fallback" << std::endl;
        safe_data_start = 0.0f;
    }
    if (!std::isfinite(safe_data_end)) {
        std::cout << "Warning: Invalid data_end=" << safe_data_end << ", using fallback" << std::endl;
        safe_data_end = 1000.0f;
    }
    if (!std::isfinite(safe_y_min)) {
        std::cout << "Warning: Invalid y_min=" << safe_y_min << ", using fallback" << std::endl;
        safe_y_min = -1.0f;
    }
    if (!std::isfinite(safe_y_max)) {
        std::cout << "Warning: Invalid y_max=" << safe_y_max << ", using fallback" << std::endl;
        safe_y_max = 1.0f;
    }
    
    // 2. Ensure data range is valid (start < end with minimum separation)
    constexpr float min_range = 1e-6f;  // Minimum range to prevent division by zero
    if (safe_data_end <= safe_data_start) {
        std::cout << "Warning: Invalid data range [" << safe_data_start << ", " << safe_data_end 
                  << "], fixing to valid range" << std::endl;
        float center = (safe_data_start + safe_data_end) * 0.5f;
        safe_data_start = center - min_range * 0.5f;
        safe_data_end = center + min_range * 0.5f;
    } else if ((safe_data_end - safe_data_start) < min_range) {
        std::cout << "Warning: Data range too small [" << safe_data_start << ", " << safe_data_end 
                  << "], expanding to minimum safe range" << std::endl;
        float center = (safe_data_start + safe_data_end) * 0.5f;
        safe_data_start = center - min_range * 0.5f;
        safe_data_end = center + min_range * 0.5f;
    }
    
    // 3. Ensure Y range is valid
    if (safe_y_max <= safe_y_min) {
        std::cout << "Warning: Invalid Y range [" << safe_y_min << ", " << safe_y_max 
                  << "], fixing to valid range" << std::endl;
        float center = (safe_y_min + safe_y_max) * 0.5f;
        safe_y_min = center - min_range * 0.5f;
        safe_y_max = center + min_range * 0.5f;
    } else if ((safe_y_max - safe_y_min) < min_range) {
        std::cout << "Warning: Y range too small [" << safe_y_min << ", " << safe_y_max 
                  << "], expanding to minimum safe range" << std::endl;
        float center = (safe_y_min + safe_y_max) * 0.5f;
        safe_y_min = center - min_range * 0.5f;
        safe_y_max = center + min_range * 0.5f;
    }
    
    // 4. Clamp extreme values to prevent numerical issues
    constexpr float max_abs_value = 1e8f;  // Large but safe limit
    if (std::abs(safe_data_start) > max_abs_value || std::abs(safe_data_end) > max_abs_value) {
        std::cout << "Warning: Extremely large data range [" << safe_data_start << ", " << safe_data_end 
                  << "], clamping to safe range" << std::endl;
        float range = safe_data_end - safe_data_start;
        if (range > 2 * max_abs_value) {
            safe_data_start = -max_abs_value;
            safe_data_end = max_abs_value;
        } else {
            float center = (safe_data_start + safe_data_end) * 0.5f;
            center = std::clamp(center, -max_abs_value * 0.5f, max_abs_value * 0.5f);
            safe_data_start = center - range * 0.5f;
            safe_data_end = center + range * 0.5f;
        }
    }

    // Create orthographic projection matrix with validated parameters
    auto Projection = glm::ortho(safe_data_start, safe_data_end, safe_y_min, safe_y_max);
    
    // Final validation: check that the resulting matrix is valid
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (!std::isfinite(Projection[i][j])) {
                std::cout << "Error: Projection matrix contains invalid value at [" << i << "][" << j 
                          << "]=" << Projection[i][j] << ", using identity matrix" << std::endl;
                return glm::mat4(1.0f);  // Return identity matrix as safe fallback
            }
        }
    }

    return Projection;
}


float getCachedStdDev(AnalogTimeSeries const & series, NewAnalogTimeSeriesDisplayOptions & display_options) {
    if (!display_options.data_cache.std_dev_cache_valid) {
        // Calculate and cache the standard deviation
        display_options.data_cache.cached_std_dev = calculate_std_dev(series);
        display_options.data_cache.std_dev_cache_valid = true;
    }
    return display_options.data_cache.cached_std_dev;
}

void invalidateDisplayCache(NewAnalogTimeSeriesDisplayOptions & display_options) {
    display_options.data_cache.std_dev_cache_valid = false;
}
