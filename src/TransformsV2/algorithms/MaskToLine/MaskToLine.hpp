/**
 * @file MaskToLine.hpp
 * @brief Element-level mask-to-line transform (Mask2D → Line2D).
 */

#ifndef NEURALYZER_V2_MASK_TO_LINE_TRANSFORM_HPP
#define NEURALYZER_V2_MASK_TO_LINE_TRANSFORM_HPP

class Line2D;
class Mask2D;

namespace Neuralyzer::Transforms::V2 {
struct ComputeContext;
}

namespace Neuralyzer::Transforms::V2::Examples {

/**
 * @brief Parameters for ordering mask points into a line
 *
 * Uses reflect-cpp for automatic JSON serialization/deserialization.
 *
 * Example JSON:
 * ```json
 * {
 *   "reference_x": 0.0,
 *   "reference_y": 0.0,
 *   "subsample_factor": 1
 * }
 * ```
 */
struct MaskToLineParams {
    /// X coordinate of the reference point used to orient the line (whisker base)
    float reference_x = 0.0f;

    /// Y coordinate of the reference point used to orient the line (whisker base)
    float reference_y = 0.0f;

    /// Subsample input mask points before ordering (1 = use all points)
    int subsample_factor = 1;
};

/**
 * @brief Order mask points into a directed line polyline
 *
 * Element-level transform: Mask2D → Line2D
 *
 * Converts unordered mask pixels into an ordered Line2D using `order_line`.
 * For thick masks, chain with `SkeletonizeMask` first. For post-processing,
 * chain with `RemoveLineOutliers` and/or `ResampleLine`.
 *
 * When applied to containers:
 * - MaskData → LineData (one line per input mask)
 *
 * @param mask Input mask points
 * @param params Reference point and subsampling settings
 * @return Ordered line, or empty line if input is empty
 */
Line2D maskToLine(
        Mask2D const & mask,
        MaskToLineParams const & params);

/**
 * @brief Order mask points into a line with progress reporting and cancellation support
 *
 * @param mask Input mask points
 * @param params Reference point and subsampling settings
 * @param ctx Compute context for progress and cancellation
 * @return Ordered line, or empty line on cancellation or empty input
 */
Line2D maskToLineWithContext(
        Mask2D const & mask,
        MaskToLineParams const & params,
        ComputeContext const & ctx);

}// namespace Neuralyzer::Transforms::V2::Examples

#endif// NEURALYZER_V2_MASK_TO_LINE_TRANSFORM_HPP
