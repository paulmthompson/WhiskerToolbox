#ifndef WHISKERTOOLBOX_V2_LINE_CURVATURE_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_LINE_CURVATURE_TRANSFORM_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <optional>
#include <string>

class Line2D;

namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Curvature calculation method
 */
enum class LineCurvatureMethod {
    PolynomialFit   // Fit a polynomial and calculate curvature from second derivative
};

/**
 * @brief Parameters for line curvature calculation
 * 
 * This transform computes the curvature at a specified position along a line
 * using polynomial fitting. Curvature is calculated from the first and second
 * derivatives of the fitted polynomial.
 * 
 * Example JSON:
 * ```json
 * {
 *   "position": 0.5,
 *   "method": "PolynomialFit",
 *   "polynomial_order": 3,
 *   "fitting_window_percentage": 0.1
 * }
 * ```
 */
struct LineCurvatureParams {
    // Position along the line (0.0-1.0) where 0 is start, 1 is end
    std::optional<float> position;

    // Curvature calculation method: "PolynomialFit"
    std::optional<std::string> method;

    // Polynomial order for fitting (2-9)
    std::optional<int> polynomial_order;

    // Fitting window as percentage of line length (0.0-1.0)
    std::optional<float> fitting_window_percentage;

    // Helper methods with defaults
    float getPosition() const { return position.value_or(0.5f); }
    
    LineCurvatureMethod getMethod() const {
        auto m = method.value_or("PolynomialFit");
        return LineCurvatureMethod::PolynomialFit;  // Only method for now
    }
    
    int getPolynomialOrder() const { return polynomial_order.value_or(3); }
    float getFittingWindowPercentage() const { return fitting_window_percentage.value_or(0.1f); }

    /**
     * @brief Normalize and clamp parameters in-place
     * 
     * Call once before batch processing to:
     * - Clamp position to [0, 1]
     * - Clamp fitting_window_percentage to [0, 1]
     * - Clamp polynomial_order to [2, 9]
     */
    void validate();
};

// ============================================================================
// Transform Implementation (Unary - takes Line2D, returns float)
// ============================================================================

/**
 * @brief Calculate the curvature at a specified position along a line
 * 
 * This is a **unary** element-level transform that takes a Line2D as input
 * and returns the curvature value at the specified position along the line.
 * 
 * Curvature is calculated using the formula:
 * k = |x'*y'' - y'*x''| / (x'^2 + y'^2)^(3/2)
 * 
 * where x', y' are first derivatives and x'', y'' are second derivatives
 * of a parametric polynomial fit to the line.
 * 
 * When applied to containers:
 * - LineData → AnalogTimeSeries (one curvature value per timestamp)
 * 
 * For batch processing, call params.validate() once before processing
 * to pre-compute clamped parameters.
 * 
 * @param line The line to calculate curvature from
 * @param params Parameters controlling the calculation (call validate() first for batch)
 * @return float Curvature value, or NaN if calculation fails
 */
float calculateLineCurvature(
        Line2D const & line,
        LineCurvatureParams const & params);

/**
 * @brief Context-aware version with progress reporting
 */
float calculateLineCurvatureWithContext(
        Line2D const & line,
        LineCurvatureParams const & params,
        ComputeContext const & ctx);

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_LINE_CURVATURE_TRANSFORM_HPP
