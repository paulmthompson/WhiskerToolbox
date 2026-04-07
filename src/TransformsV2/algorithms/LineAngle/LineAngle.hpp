#ifndef WHISKERTOOLBOX_V2_LINE_ANGLE_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_LINE_ANGLE_TRANSFORM_HPP

#include "extension/ElementTransform.hpp"

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
 * @brief Angle calculation method
 */
enum class LineAngleMethod {
    DirectPoints,   // Calculate angle directly between two points on the line
    PolynomialFit   // Fit a polynomial and calculate angle from derivative
};

/**
 * @brief Parameters for line angle calculation
 * 
 * This transform computes the angle at a specified position along a line.
 * The angle can be calculated relative to a configurable reference vector.
 * 
 * Example JSON:
 * ```json
 * {
 *   "position": 0.2,
 *   "method": "DirectPoints",
 *   "polynomial_order": 3,
 *   "reference_x": 1.0,
 *   "reference_y": 0.0
 * }
 * ```
 */
struct LineAngleParams {
    // Position along the line (0.0-1.0) where 0 is start, 1 is end
    float position = 0.2f;

    // Angle calculation method: "DirectPoints" or "PolynomialFit"
    LineAngleMethod method = LineAngleMethod::DirectPoints;

    // Polynomial order for PolynomialFit method (1-9)
    int polynomial_order = 3;

    // Reference vector components (angle is measured from this direction)
    float reference_x = 1.0f;
    float reference_y = 0.0f;

    /**
     * @brief Normalize and clamp parameters in-place
     * 
     * Call once before batch processing to:
     * - Clamp position to [0, 1]
     * - Normalize reference vector (defaults to x-axis if zero)
     */
    void validate();
};

// ============================================================================
// Transform Implementation (Unary - takes Line2D, returns float)
// ============================================================================

/**
 * @brief Calculate the angle at a specified position along a line
 * 
 * This is a **unary** element-level transform that takes a Line2D as input
 * and returns the angle in degrees at the specified position along the line.
 * 
 * The angle is measured relative to the reference vector (default: positive x-axis).
 * Positive angles are counter-clockwise from the reference.
 * 
 * Two calculation methods are supported:
 * - DirectPoints: Calculate angle from tangent vector between adjacent points
 * - PolynomialFit: Fit a polynomial to the line and calculate angle from derivative
 * 
 * When applied to containers:
 * - LineData → AnalogTimeSeries (one angle per timestamp)
 * 
 * For batch processing, call params.validate() once before processing
 * to pre-compute normalized reference vectors and clamped positions.
 * 
 * @param line The line to calculate angle from
 * @param params Parameters controlling the calculation (call validate() first for batch)
 * @return float Angle in degrees (-180 to 180)
 *
 * @pre When method is PolynomialFit, params.getPolynomialOrder() >= 0 (enforcement: none)
 */
float calculateLineAngle(
        Line2D const & line,
        LineAngleParams const & params);

/**
 * @brief Context-aware version with progress reporting
 */
float calculateLineAngleWithContext(
        Line2D const & line,
        LineAngleParams const & params,
        ComputeContext const & ctx);

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_LINE_ANGLE_TRANSFORM_HPP
