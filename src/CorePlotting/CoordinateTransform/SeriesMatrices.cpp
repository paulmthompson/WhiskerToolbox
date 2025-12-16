#include "SeriesMatrices.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace CorePlotting {

// ============================================================================
// Utility Functions
// ============================================================================

bool validateOrthoParams(float & left, float & right,
                         float & bottom, float & top,
                         char const * context_name) {
    bool was_valid = true;
    constexpr float min_range = 1e-6f;   // Minimum range to prevent division by zero
    constexpr float max_abs_value = 1e8f;// Large but safe limit

    // 1. Ensure all values are finite
    if (!std::isfinite(left)) {
        std::cerr << "Warning [" << context_name << "]: Invalid left=" << left << ", using fallback\n";
        left = 0.0f;
        was_valid = false;
    }
    if (!std::isfinite(right)) {
        std::cerr << "Warning [" << context_name << "]: Invalid right=" << right << ", using fallback\n";
        right = 1000.0f;
        was_valid = false;
    }
    if (!std::isfinite(bottom)) {
        std::cerr << "Warning [" << context_name << "]: Invalid bottom=" << bottom << ", using fallback\n";
        bottom = -1.0f;
        was_valid = false;
    }
    if (!std::isfinite(top)) {
        std::cerr << "Warning [" << context_name << "]: Invalid top=" << top << ", using fallback\n";
        top = 1.0f;
        was_valid = false;
    }

    // 2. Ensure X range is valid (left < right with minimum separation)
    if (right <= left) {
        std::cerr << "Warning [" << context_name << "]: Invalid X range [" << left << ", " << right
                  << "], fixing to valid range\n";
        float center = (left + right) * 0.5f;
        left = center - min_range * 0.5f;
        right = center + min_range * 0.5f;
        was_valid = false;
    } else if ((right - left) < min_range) {
        std::cerr << "Warning [" << context_name << "]: X range too small [" << left << ", " << right
                  << "], expanding to minimum safe range\n";
        float center = (left + right) * 0.5f;
        left = center - min_range * 0.5f;
        right = center + min_range * 0.5f;
        was_valid = false;
    }

    // 3. Ensure Y range is valid
    if (top <= bottom) {
        std::cerr << "Warning [" << context_name << "]: Invalid Y range [" << bottom << ", " << top
                  << "], fixing to valid range\n";
        float center = (bottom + top) * 0.5f;
        bottom = center - min_range * 0.5f;
        top = center + min_range * 0.5f;
        was_valid = false;
    } else if ((top - bottom) < min_range) {
        std::cerr << "Warning [" << context_name << "]: Y range too small [" << bottom << ", " << top
                  << "], expanding to minimum safe range\n";
        float center = (bottom + top) * 0.5f;
        bottom = center - min_range * 0.5f;
        top = center + min_range * 0.5f;
        was_valid = false;
    }

    // 4. Clamp extreme values for X range
    if (std::abs(left) > max_abs_value || std::abs(right) > max_abs_value) {
        std::cerr << "Warning [" << context_name << "]: Extremely large X range [" << left << ", " << right
                  << "], clamping to safe range\n";
        float range = right - left;
        if (range > 2 * max_abs_value) {
            left = -max_abs_value;
            right = max_abs_value;
        } else {
            float center = (left + right) * 0.5f;
            center = std::clamp(center, -max_abs_value * 0.5f, max_abs_value * 0.5f);
            left = center - range * 0.5f;
            right = center + range * 0.5f;
        }
        was_valid = false;
    }

    return was_valid;
}

glm::mat4 validateMatrix(glm::mat4 const & matrix, char const * context_name) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (!std::isfinite(matrix[i][j])) {
                std::cerr << "Error [" << context_name << "]: Matrix contains invalid value at ["
                          << i << "][" << j << "]=" << matrix[i][j]
                          << ", using identity matrix\n";
                return glm::mat4(1.0f);
            }
        }
    }
    return matrix;
}

// ============================================================================
// Analog Time Series MVP Matrices
// ============================================================================

glm::mat4 getAnalogModelMatrix(AnalogSeriesMatrixParams const & params) {
    // Calculate intrinsic scaling (3 standard deviations for full range)
    // This maps ±3*std_dev (from the mean) to ±1.0 in normalized space
    // Protect against division by zero
    float const safe_std_dev = (params.std_dev > 1e-6f) ? params.std_dev : 1.0f;
    float const intrinsic_scale = 1.0f / (3.0f * safe_std_dev);

    // Combine all scaling factors: intrinsic, user, and global
    float const total_y_scale = intrinsic_scale *
                                params.intrinsic_scale *
                                params.user_scale_factor *
                                params.global_zoom *
                                params.global_vertical_scale;

    // Scale to fit within allocated height (use 80% of allocated space for safety)
    // This means ±3*std_dev (from mean) will span ±80% of the allocated height
    float const height_scale = params.allocated_height * 0.8f;
    float const final_y_scale = total_y_scale * height_scale;

    // Build the transformation to achieve: (data_value - data_mean) * scale + allocated_center + user_offset
    // This ensures data_mean maps exactly to allocated_center, then applies user offset
    // Formula: y_out = y_in * final_y_scale + (allocated_center - data_mean * final_y_scale) + user_offset

    float const y_offset = params.allocated_y_center - params.data_mean * final_y_scale + params.user_vertical_offset;

    // Construct affine transformation matrix
    glm::mat4 Model(1.0f);
    Model[1][1] = final_y_scale;// Y scaling
    Model[3][1] = y_offset;     // Y translation (including user offset)

    return Model;
}

glm::mat4 getAnalogViewMatrix(ViewProjectionParams const & params) {
    glm::mat4 View(1.0f);

    // Apply global vertical panning
    if (params.vertical_pan_offset != 0.0f) {
        View = glm::translate(View, glm::vec3(0.0f, params.vertical_pan_offset, 0.0f));
    }

    return View;
}

glm::mat4 getAnalogProjectionMatrix(TimeFrameIndex start_time_index,
                                    TimeFrameIndex end_time_index,
                                    float y_min,
                                    float y_max) {
    float left = static_cast<float>(start_time_index.getValue());
    float right = static_cast<float>(end_time_index.getValue());
    float bottom = y_min;
    float top = y_max;

    // Validate and fix parameters
    validateOrthoParams(left, right, bottom, top, "AnalogProjection");

    // Create orthographic projection matrix
    glm::mat4 Projection = glm::ortho(left, right, bottom, top);

    // Final validation
    return validateMatrix(Projection, "AnalogProjection");
}

// ============================================================================
// Digital Event Series MVP Matrices
// ============================================================================

glm::mat4 getEventModelMatrix(EventSeriesMatrixParams const & params) {
    glm::mat4 Model(1.0f);

    if (params.plotting_mode == EventSeriesMatrixParams::PlottingMode::FullCanvas) {
        // Full Canvas Mode: extend full viewport height, centered
        float const height_scale = (params.viewport_y_max - params.viewport_y_min) * params.margin_factor;
        float const center_y = (params.viewport_y_max + params.viewport_y_min) * 0.5f;
        Model[1][1] = height_scale * 0.5f;// map [-1,1] -> full height with margin
        Model[3][1] = center_y;

    } else {// Stacked mode
        // Events are positioned within allocated space (like analog series)
        // Prefer explicit event height if provided, but never exceed allocated lane
        float const desired_height = (params.event_height > 0.0f)
                                             ? params.event_height
                                             : params.allocated_height;
        float const height_scale = std::min(desired_height, params.allocated_height) * params.margin_factor;

        // Apply scaling for allocated height
        Model[1][1] = height_scale * 0.5f;// Half scale because we map [-1,1] to allocated height

        // Apply translation to allocated center
        Model[3][1] = params.allocated_y_center;
    }

    // Apply global scaling factors
    Model[1][1] *= params.global_vertical_scale;

    return Model;
}

glm::mat4 getEventViewMatrix(EventSeriesMatrixParams const & params,
                             ViewProjectionParams const & view_params) {
    glm::mat4 View(1.0f);

    // Panning behavior depends on plotting mode
    if (params.plotting_mode == EventSeriesMatrixParams::PlottingMode::FullCanvas) {
        // Full Canvas Mode: Events stay viewport-pinned (like digital intervals)
        // No panning applied - events remain fixed to viewport bounds

    } else {// Stacked mode
        // Events move with content (like analog series)
        // Apply global vertical panning
        if (view_params.vertical_pan_offset != 0.0f) {
            View = glm::translate(View, glm::vec3(0.0f, view_params.vertical_pan_offset, 0.0f));
        }
    }

    return View;
}

glm::mat4 getEventProjectionMatrix(TimeFrameIndex start_time_index,
                                   TimeFrameIndex end_time_index,
                                   float y_min,
                                   float y_max) {
    float left = static_cast<float>(start_time_index.getValue());
    float right = static_cast<float>(end_time_index.getValue());
    float bottom = y_min;
    float top = y_max;

    // Validate and fix parameters
    validateOrthoParams(left, right, bottom, top, "EventProjection");

    // Create orthographic projection matrix
    glm::mat4 Projection = glm::ortho(left, right, bottom, top);

    // Final validation
    return validateMatrix(Projection, "EventProjection");
}

// ============================================================================
// Digital Interval Series MVP Matrices
// ============================================================================

glm::mat4 getIntervalModelMatrix(IntervalSeriesMatrixParams const & params) {
    glm::mat4 Model(1.0f);

    // Apply global zoom scaling
    float const global_scale = params.global_zoom * params.global_vertical_scale;

    if (params.extend_full_canvas) {
        // Scale to use the full allocated height with margin factor
        float const height_scale = params.allocated_height * params.margin_factor * 0.5f;
        Model = glm::scale(Model, glm::vec3(1.0f, height_scale * global_scale, 1.0f));

        // Translate to allocated center position
        Model = glm::translate(Model, glm::vec3(0.0f, params.allocated_y_center, 0.0f));
    } else {
        // Apply standard scaling and positioning
        Model = glm::scale(Model, glm::vec3(1.0f, global_scale, 1.0f));
        Model = glm::translate(Model, glm::vec3(0.0f, params.allocated_y_center, 0.0f));
    }

    return Model;
}

glm::mat4 getIntervalViewMatrix(ViewProjectionParams const & params) {
    // Digital intervals remain viewport-pinned (do not move with panning)
    // They always extend from top to bottom of the current view
    static_cast<void>(params);// Unused
    return glm::mat4(1.0f);
}

glm::mat4 getIntervalProjectionMatrix(TimeFrameIndex start_time_index,
                                      TimeFrameIndex end_time_index,
                                      float viewport_y_min,
                                      float viewport_y_max) {
    float left = static_cast<float>(start_time_index.getValue());
    float right = static_cast<float>(end_time_index.getValue());
    float bottom = viewport_y_min;
    float top = viewport_y_max;

    // Validate and fix parameters
    validateOrthoParams(left, right, bottom, top, "IntervalProjection");

    // Create orthographic projection matrix
    glm::mat4 Projection = glm::ortho(left, right, bottom, top);

    // Final validation
    return validateMatrix(Projection, "IntervalProjection");
}

// ============================================================================
// Inverse Transform Utilities
// ============================================================================

namespace {
/**
 * @brief Calculate the combined Y scale factor for analog series
 * 
 * This is extracted to avoid duplication between forward and inverse transforms.
 */
float calculateAnalogYScale(AnalogSeriesMatrixParams const & params) {
    // Calculate intrinsic scaling (same as getAnalogModelMatrix)
    float const safe_std_dev = (params.std_dev > 1e-6f) ? params.std_dev : 1.0f;
    float const intrinsic_scale = 1.0f / (3.0f * safe_std_dev);

    // Combine all scaling factors
    float const total_y_scale = intrinsic_scale *
                                params.intrinsic_scale *
                                params.user_scale_factor *
                                params.global_zoom *
                                params.global_vertical_scale;

    // Scale to fit within allocated height (80% margin)
    float const height_scale = params.allocated_height * 0.8f;
    return total_y_scale * height_scale;
}
} // anonymous namespace

float worldYToAnalogValue(float world_y, AnalogSeriesMatrixParams const & params) {
    float const final_y_scale = calculateAnalogYScale(params);
    
    // Guard against division by zero
    if (std::abs(final_y_scale) < 1e-10f) {
        return params.data_mean;
    }
    
    // Inverse of: y_world = (y_data - data_mean) * scale + allocated_y_center + user_offset
    // Solving for y_data:
    //   y_world - allocated_y_center - user_offset = (y_data - data_mean) * scale
    //   (y_world - allocated_y_center - user_offset) / scale = y_data - data_mean
    //   y_data = (y_world - allocated_y_center - user_offset) / scale + data_mean
    
    float const y_offset = params.allocated_y_center + params.user_vertical_offset;
    return (world_y - y_offset) / final_y_scale + params.data_mean;
}

float analogValueToWorldY(float data_value, AnalogSeriesMatrixParams const & params) {
    float const final_y_scale = calculateAnalogYScale(params);
    
    // Forward transform: y_world = (y_data - data_mean) * scale + allocated_y_center + user_offset
    float const y_offset = params.allocated_y_center + params.user_vertical_offset;
    return (data_value - params.data_mean) * final_y_scale + y_offset;
}

}// namespace CorePlotting
