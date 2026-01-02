#include "SeriesMatrices.hpp"
#include "Layout/LayoutTransform.hpp"

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
// View and Projection Matrices (shared across data types)
// ============================================================================

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
// LayoutTransform-based API
// ============================================================================

glm::mat4 createProjectionMatrix(TimeFrameIndex start_time,
                                 TimeFrameIndex end_time,
                                 float y_min,
                                 float y_max) {
    float left = static_cast<float>(start_time.getValue());
    float right = static_cast<float>(end_time.getValue());
    float bottom = y_min;
    float top = y_max;

    // Validate and fix parameters
    validateOrthoParams(left, right, bottom, top, "Projection");

    // Create orthographic projection matrix
    glm::mat4 Projection = glm::ortho(left, right, bottom, top);

    // Final validation
    return validateMatrix(Projection, "Projection");
}

}// namespace CorePlotting
