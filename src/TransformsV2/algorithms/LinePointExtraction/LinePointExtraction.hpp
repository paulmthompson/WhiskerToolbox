#ifndef NEURALYZER_V2_LINE_POINT_EXTRACTION_TRANSFORM_HPP
#define NEURALYZER_V2_LINE_POINT_EXTRACTION_TRANSFORM_HPP

class Line2D;
template<typename T>
struct Point2D;

namespace Neuralyzer::Transforms::V2 {
struct ComputeContext;
}

namespace Neuralyzer::Transforms::V2::Examples {

/**
 * @brief Point extraction method
 */
enum class LinePointExtractionMethod {
    Direct,     // Direct point selection based on distance along line
    Parametric  // Use parametric polynomial interpolation
};

/**
 * @brief Parameters for line point extraction
 * 
 * This transform extracts a point at a specified fractional position along a line.
 * The extraction can use direct distance-based interpolation or parametric
 * polynomial fitting for smoother results.
 * 
 * Example JSON:
 * ```json
 * {
 *   "position": 0.5,
 *   "method": "Direct",
 *   "polynomial_order": 3,
 *   "use_interpolation": true
 * }
 * ```
 */
struct LinePointExtractionParams {
    /// Position along the line (0.0–1.0) where 0 is start, 1 is end
    float position = 0.5f;

    /// Extraction method
    LinePointExtractionMethod method = LinePointExtractionMethod::Direct;

    /// Polynomial order for Parametric method (1–9)
    int polynomial_order = 3;

    /// Whether to interpolate between points (Direct method) vs use nearest
    bool use_interpolation = true;

    /**
     * @brief Normalize and clamp parameters in-place
     *
     * Call once before batch processing to:
     * - Clamp position to [0, 1]
     * - Clamp polynomial_order to valid range
     */
    void validate();
};

// ============================================================================
// Transform Implementation (Unary - takes Line2D, returns Point2D<float>)
// ============================================================================

/**
 * @brief Extract a point at a specified position along a line
 * 
 * This is a **unary** element-level transform that takes a Line2D as input
 * and returns a Point2D<float> at the specified fractional position along the line.
 * 
 * Two extraction methods are supported:
 * - Direct: Use distance-based interpolation along the line
 * - Parametric: Fit a polynomial and evaluate at the specified position
 * 
 * When applied to containers:
 * - LineData → PointData (one point per timestamp per line)
 * 
 * For batch processing, call params.validate() once before processing
 * to pre-compute clamped parameter values.
 * 
 * @param line The line to extract a point from
 * @param params Parameters controlling the extraction (call validate() first for batch)
 * @return Point2D<float> The extracted point coordinates
 */
Point2D<float> extractLinePoint(
        Line2D const & line,
        LinePointExtractionParams const & params);

/**
 * @brief Context-aware version with progress reporting
 */
Point2D<float> extractLinePointWithContext(
        Line2D const & line,
        LinePointExtractionParams const & params,
        ComputeContext const & ctx);

}// namespace Neuralyzer::Transforms::V2::Examples

#endif// NEURALYZER_V2_LINE_POINT_EXTRACTION_TRANSFORM_HPP
