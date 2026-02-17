#ifndef WHISKERTOOLBOX_V2_LINE_SUBSEGMENT_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_LINE_SUBSEGMENT_TRANSFORM_HPP

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
 * @brief Subsegment extraction method
 */
enum class LineSubsegmentMethod {
    Direct,      // Direct point extraction based on distance along line
    Parametric   // Use parametric polynomial interpolation
};

/**
 * @brief Parameters for line subsegment extraction
 * 
 * This transform extracts a subsegment from a line between two fractional positions
 * along its arc length. Two methods are available:
 * 
 * - Direct: Extracts points directly from the line, optionally preserving original spacing
 * - Parametric: Fits a parametric polynomial and resamples the subsegment uniformly
 * 
 * Example JSON:
 * ```json
 * {
 *   "start_position": 0.2,
 *   "end_position": 0.8,
 *   "method": "Direct",
 *   "polynomial_order": 3,
 *   "output_points": 50,
 *   "preserve_original_spacing": true
 * }
 * ```
 */
struct LineSubsegmentParams {
    // Start position as fraction of arc length (0.0 to 1.0)
    std::optional<float> start_position;
    
    // End position as fraction of arc length (0.0 to 1.0)
    std::optional<float> end_position;
    
    // Extraction method: "Direct" or "Parametric"
    std::optional<std::string> method;
    
    // Polynomial order for Parametric method (1-9)
    std::optional<int> polynomial_order;
    
    // Number of output points for Parametric method
    std::optional<int> output_points;
    
    // For Direct method: whether to preserve original point spacing
    std::optional<bool> preserve_original_spacing;

    // Helper methods with defaults
    float getStartPosition() const { return start_position.value_or(0.3f); }
    float getEndPosition() const { return end_position.value_or(0.7f); }
    
    LineSubsegmentMethod getMethod() const {
        auto m = method.value_or("Parametric");
        return (m == "Direct") ? LineSubsegmentMethod::Direct : LineSubsegmentMethod::Parametric;
    }
    
    int getPolynomialOrder() const { return polynomial_order.value_or(3); }
    int getOutputPoints() const { return output_points.value_or(50); }
    bool getPreserveOriginalSpacing() const { return preserve_original_spacing.value_or(true); }

    /**
     * @brief Validate and clamp parameters in-place
     * 
     * Call once before batch processing to:
     * - Clamp positions to [0, 1]
     * - Ensure start < end
     * - Clamp polynomial order to valid range
     */
    void validate();
};

// ============================================================================
// Transform Implementation (Unary - takes Line2D, returns Line2D)
// ============================================================================

/**
 * @brief Extract a subsegment from a line between specified fractional positions
 * 
 * This is a **unary** element-level transform that takes a Line2D as input
 * and returns a Line2D containing the extracted subsegment.
 * 
 * Two extraction methods are supported:
 * - Direct: Extracts points directly from the original line at the specified
 *   positions. With preserve_original_spacing=true, keeps original points within
 *   the range. With preserve_original_spacing=false, interpolates start/end points.
 * - Parametric: Fits a parametric polynomial to the line and generates a new
 *   set of uniformly-spaced points along the subsegment.
 * 
 * When applied to containers:
 * - LineData → LineData (one subsegment per input line)
 * 
 * Edge cases:
 * - Empty lines: Returned unchanged (empty)
 * - Single-point lines: Returned unchanged (single point)
 * - Invalid range (start >= end): Returns empty line
 * - Insufficient points for polynomial: Falls back to Direct method
 * 
 * @param line The line to extract subsegment from
 * @param params Parameters controlling the extraction
 * @return Line2D The extracted subsegment
 */
Line2D extractLineSubsegment(
        Line2D const & line,
        LineSubsegmentParams const & params);

/**
 * @brief Context-aware version with progress reporting
 */
Line2D extractLineSubsegmentWithContext(
        Line2D const & line,
        LineSubsegmentParams const & params,
        ComputeContext const & ctx);

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_LINE_SUBSEGMENT_TRANSFORM_HPP
