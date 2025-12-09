#ifndef WHISKERTOOLBOX_V2_LINE_RESAMPLE_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_LINE_RESAMPLE_TRANSFORM_HPP

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
 * @brief Line simplification algorithm selection
 */
enum class LineResampleMethod {
    FixedSpacing,   // Resample to target spacing between points
    DouglasPeucker  // Simplify using Douglas-Peucker algorithm
};

/**
 * @brief Parameters for line resampling transform
 * 
 * This transform resamples or simplifies lines based on the selected algorithm:
 * - FixedSpacing: Creates evenly-spaced points along the line
 * - DouglasPeucker: Removes points while preserving shape within epsilon tolerance
 * 
 * Example JSON:
 * ```json
 * {
 *   "method": "FixedSpacing",
 *   "target_spacing": 5.0,
 *   "epsilon": 2.0
 * }
 * ```
 */
struct LineResampleParams {
    // Algorithm to use: "FixedSpacing" or "DouglasPeucker"
    std::optional<std::string> method;

    // Target spacing between points in pixels (for FixedSpacing)
    // Must be strictly positive (> 0)
    std::optional<rfl::Validator<float, rfl::ExclusiveMinimum<0.0f>>> target_spacing;

    // Maximum perpendicular distance tolerance for point removal (for DouglasPeucker)
    // Must be strictly positive (> 0)
    std::optional<rfl::Validator<float, rfl::ExclusiveMinimum<0.0f>>> epsilon;

    // Helper methods with defaults
    LineResampleMethod getMethod() const {
        auto m = method.value_or("FixedSpacing");
        return (m == "DouglasPeucker" || m == "Douglas-Peucker") 
            ? LineResampleMethod::DouglasPeucker 
            : LineResampleMethod::FixedSpacing;
    }

    float getTargetSpacing() const {
        return target_spacing.has_value() ? target_spacing.value().value() : 5.0f;
    }

    float getEpsilon() const {
        return epsilon.has_value() ? epsilon.value().value() : 2.0f;
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
