/**
 * @file MaskMedianFilter.hpp
 * @brief Element-level mask median filtering transform (Mask2D → Mask2D).
 */

#ifndef NEURALYZER_V2_MASK_MEDIAN_FILTER_TRANSFORM_HPP
#define NEURALYZER_V2_MASK_MEDIAN_FILTER_TRANSFORM_HPP

class Mask2D;

namespace Neuralyzer::Transforms::V2 {
struct ComputeContext;
}

namespace Neuralyzer::Transforms::V2::Examples {

/**
 * @brief Parameters for mask median filtering
 *
 * Uses reflect-cpp for automatic JSON serialization/deserialization.
 *
 * Example JSON:
 * ```json
 * {}
 * ```
 *
 * With explicit canvas and window size:
 * ```json
 * {
 *   "window_size": 3,
 *   "image_width": 640,
 *   "image_height": 480
 * }
 * ```
 */
struct MaskMedianFilterParams {
    /// Square window size for median filtering (must be odd and >= 1)
    int window_size = 3;

    /// Canvas width in pixels; <= 0 derives size from mask point extents
    int image_width = -1;

    /// Canvas height in pixels; <= 0 derives size from mask point extents
    int image_height = -1;
};

/**
 * @brief Apply median filtering to a single mask element
 *
 * Element-level transform: Mask2D → Mask2D
 *
 * Rasterizes the mask to a binary image, applies median filtering to remove
 * salt-and-pepper noise and smooth jagged boundaries, then converts back to mask points.
 *
 * When canvas dimensions are omitted, the raster is sized to the mask bounding box
 * plus background border padding derived from `window_size`.
 *
 * When applied to containers:
 * - MaskData → MaskData (one filtered mask per input mask)
 *
 * @param mask Input mask points
 * @param params Window size and optional full-canvas dimensions
 * @return Filtered mask, or empty mask if input is empty or filtering removes all pixels
 */
Mask2D applyMedianFilter(
        Mask2D const & mask,
        MaskMedianFilterParams const & params);

/**
 * @brief Apply median filtering with progress reporting and cancellation support
 *
 * @param mask Input mask points
 * @param params Window size and optional full-canvas dimensions
 * @param ctx Compute context for progress and cancellation
 * @return Filtered mask, or empty mask on cancellation or empty input
 */
Mask2D applyMedianFilterWithContext(
        Mask2D const & mask,
        MaskMedianFilterParams const & params,
        ComputeContext const & ctx);

}// namespace Neuralyzer::Transforms::V2::Examples

#endif// NEURALYZER_V2_MASK_MEDIAN_FILTER_TRANSFORM_HPP
