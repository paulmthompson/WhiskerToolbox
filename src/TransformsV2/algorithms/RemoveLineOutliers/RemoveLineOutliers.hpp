/**
 * @file RemoveLineOutliers.hpp
 * @brief Element-level geometric outlier removal for lines (Line2D → Line2D).
 */

#ifndef WHISKERTOOLBOX_V2_REMOVE_LINE_OUTLIERS_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_REMOVE_LINE_OUTLIERS_TRANSFORM_HPP

class Line2D;

namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for geometric outlier removal on a line
 *
 * Uses reflect-cpp for automatic JSON serialization/deserialization.
 *
 * Example JSON:
 * ```json
 * {
 *   "error_threshold": 5.0,
 *   "polynomial_order": 3
 * }
 * ```
 */
struct RemoveLineOutliersParams {
    /// Maximum allowed deviation from the fitted curve in pixels
    float error_threshold = 5.0f;

    /// Polynomial order used for iterative fitting (clamped to [1, 9] in validate())
    int polynomial_order = 3;

    /**
     * @brief Normalize and clamp parameters in-place
     *
     * Call once before batch processing to clamp polynomial_order to [1, 9].
     */
    void validate();
};

/**
 * @brief Remove geometric outlier points from an ordered line
 *
 * Element-level transform: Line2D → Line2D
 *
 * Iteratively fits a parametric polynomial and removes points that deviate
 * more than `error_threshold` pixels from the fitted curve. Requires at least
 * (polynomial_order + 2) points; otherwise returns the input unchanged.
 *
 * When applied to containers:
 * - LineData → LineData (one filtered line per input line)
 *
 * @param line Input ordered line
 * @param params Error threshold and polynomial order
 * @return Filtered line, or input unchanged if too few points or line is empty
 */
Line2D removeLineOutliers(
        Line2D const & line,
        RemoveLineOutliersParams const & params);

/**
 * @brief Remove line outliers with progress reporting and cancellation support
 *
 * @param line Input ordered line
 * @param params Error threshold and polynomial order
 * @param ctx Compute context for progress and cancellation
 * @return Filtered line, or input unchanged on cancellation
 */
Line2D removeLineOutliersWithContext(
        Line2D const & line,
        RemoveLineOutliersParams const & params,
        ComputeContext const & ctx);

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_REMOVE_LINE_OUTLIERS_TRANSFORM_HPP
