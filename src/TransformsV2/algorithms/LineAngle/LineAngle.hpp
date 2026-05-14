#ifndef WHISKERTOOLBOX_V2_LINE_ANGLE_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_LINE_ANGLE_TRANSFORM_HPP

class Line2D;

namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Angle calculation method
 */
enum class LineAngleMethod {
    DirectPoints,  // Secant over a sliding arc-length window around the midpoint
    PolynomialFit  // Local polynomial in the window; tangent at midpoint from coefficients
};

/**
 * @brief Parameters for line angle calculation
 *
 * Example JSON:
 * ```json
 * {
 *   "position": 0.2,
 *   "window": 0.2,
 *   "method": "DirectPoints",
 *   "polynomial_order": 3,
 *   "axis_x_x": 1.0,
 *   "axis_x_y": 0.0,
 *   "axis_y_x": 0.0,
 *   "axis_y_y": 1.0
 * }
 * ```
 */
struct LineAngleParams {
    /// Midpoint along cumulative arc length (0 = start, 1 = end).
    float position = 0.2f;

    /// Full width of arc-length window (0–1), shared by direct and polynomial paths.
    float window = 0.2f;

    LineAngleMethod method = LineAngleMethod::DirectPoints;

    int polynomial_order = 3;

    float axis_x_x = 1.0f;
    float axis_x_y = 0.0f;
    float axis_y_x = 0.0f;
    float axis_y_y = 1.0f;

    /**
     * @brief Clamp numeric parameters in-place before batch processing.
     */
    void validate();
};

// ============================================================================
// Transform Implementation (Unary - takes Line2D, returns float)
// ============================================================================

/**
 * @brief Calculate the angle at a specified position along a line
 *
 * @param line The line to calculate angle from
 * @param params Parameters controlling the calculation (call validate() first for batch)
 * @return float Angle in degrees (-180 to 180), or NaN if line has fewer than 2 points
 *
 * @pre When method is PolynomialFit, polynomial_order >= 0 (enforcement: none)
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
