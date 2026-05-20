#ifndef WHISKERTOOLBOX_V2_LINE_RESAMPLE_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_LINE_RESAMPLE_TRANSFORM_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <algorithm>
#include <optional>
#include <string>

class Line2D;

namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Line simplification algorithm selection
 */
enum class LineResampleMethod {
    FixedSpacing,    ///< Resample to target spacing between points
    DouglasPeucker,  ///< Simplify using Douglas-Peucker algorithm
    PolynomialSmooth ///< Smooth via parametric polynomial fit and resample
};

/**
 * @brief Parameters for line resampling transform
 *
 * This transform resamples or simplifies lines based on the selected algorithm:
 * - FixedSpacing: Creates evenly-spaced points along the line
 * - DouglasPeucker: Removes points while preserving shape within epsilon tolerance
 * - PolynomialSmooth: Fits arc-length parametric polynomials and resamples at target spacing
 *
 * Example JSON:
 * ```json
 * {
 *   "method": "PolynomialSmooth",
 *   "target_spacing": 5.0,
 *   "epsilon": 2.0,
 *   "polynomial_order": 3
 * }
 * ```
 */
struct LineResampleParams {
    // Algorithm to use: "FixedSpacing", "DouglasPeucker", or "PolynomialSmooth"
    std::optional<std::string> method;

    // Target spacing between points in pixels (for FixedSpacing and PolynomialSmooth)
    // Must be strictly positive (> 0)
    std::optional<rfl::Validator<float, rfl::ExclusiveMinimum<0.0f>>> target_spacing;

    // Maximum perpendicular distance tolerance for point removal (for DouglasPeucker)
    // Must be strictly positive (> 0)
    std::optional<rfl::Validator<float, rfl::ExclusiveMinimum<0.0f>>> epsilon;

    // Polynomial order for PolynomialSmooth (1-9)
    std::optional<int> polynomial_order;

    /// @brief Resolve method string to enum
    [[nodiscard]] LineResampleMethod getMethod() const {
        auto const m = method.value_or("FixedSpacing");
        if (m == "DouglasPeucker" || m == "Douglas-Peucker") {
            return LineResampleMethod::DouglasPeucker;
        }
        if (m == "PolynomialSmooth" || m == "Polynomial Smooth") {
            return LineResampleMethod::PolynomialSmooth;
        }
        return LineResampleMethod::FixedSpacing;
    }

    [[nodiscard]] float getTargetSpacing() const {
        return target_spacing.has_value() ? target_spacing.value().value() : 5.0f;
    }

    [[nodiscard]] float getEpsilon() const {
        return epsilon.has_value() ? epsilon.value().value() : 2.0f;
    }

    /// @brief Polynomial order clamped to [1, 9], default 3
    [[nodiscard]] int getPolynomialOrder() const {
        int const order = polynomial_order.value_or(3);
        return std::max(1, std::min(order, 9));
    }
};

// ============================================================================
// Transform Implementation (Unary - takes Line2D, returns Line2D)
// ============================================================================

/**
 * @brief Resample or simplify a line based on the selected algorithm
 *
 * This is a **unary** element-level transform that takes a Line2D as input
 * and returns a resampled/simplified Line2D.
 *
 * Algorithm descriptions:
 *
 * **FixedSpacing**: Iterates through the polyline, placing new points at
 * target_spacing intervals. Interpolates between original points as needed.
 * Preserves first and last points.
 *
 * **DouglasPeucker**: Recursively simplifies a polyline by removing points
 * that are within epsilon distance of the line segment between two endpoints.
 * Preserves overall shape while reducing point count.
 *
 * **PolynomialSmooth**: Fits parametric polynomials x(t), y(t) with arc-length
 * parameterization, then generates evenly-spaced points along the fitted curve.
 * Falls back to FixedSpacing resampling if fitting fails.
 *
 * When applied to containers:
 * - LineData → LineData (one resampled line per input line)
 *
 * Edge cases:
 * - Empty lines: Returned unchanged (empty)
 * - Single-point lines: Returned unchanged (cannot resample)
 * - Two-point lines: Returned unchanged (minimal representation)
 *
 * @param line The line to resample/simplify
 * @param params Parameters specifying algorithm and settings
 * @return Line2D The resampled/simplified line
 */
Line2D resampleLine(
        Line2D const & line,
        LineResampleParams const & params);

/**
 * @brief Context-aware version with progress reporting
 */
Line2D resampleLineWithContext(
        Line2D const & line,
        LineResampleParams const & params,
        ComputeContext const & ctx);

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_LINE_RESAMPLE_TRANSFORM_HPP
