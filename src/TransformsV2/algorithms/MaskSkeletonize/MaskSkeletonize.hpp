/**
 * @file MaskSkeletonize.hpp
 * @brief Element-level mask skeletonization transform (Mask2D → Mask2D).
 */

#ifndef WHISKERTOOLBOX_V2_MASK_SKELETONIZE_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_MASK_SKELETONIZE_TRANSFORM_HPP

class Mask2D;

namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Skeletonization algorithm selection
 */
enum class SkeletonizeMethod {
    ZhangSuen,///< Morphological thinning (Zhang–Suen), adapted from scikit-image
    MedialAxis///< Distance-transform medial axis, Felzenszwalb EDT + scikit-image thinning
};

/**
 * @brief Parameters for mask skeletonization
 *
 * Uses reflect-cpp for automatic JSON serialization/deserialization.
 *
 * Example JSON:
 * ```json
 * {}
 * ```
 *
 * Optional canvas dimensions (when both are positive):
 * ```json
 * {
 *   "image_width": 640,
 *   "image_height": 480
 * }
 * ```json
 * {
 *   "method": "MedialAxis",
 *   "image_width": 640,
 *   "image_height": 480
 * }
 * ```
 */
struct MaskSkeletonizeParams {
    /// Thinning algorithm to apply
    SkeletonizeMethod method = SkeletonizeMethod::ZhangSuen;

    /// Canvas width in pixels; <= 0 derives size from mask point extents
    int image_width = -1;

    /// Canvas height in pixels; <= 0 derives size from mask point extents
    int image_height = -1;
};

/**
 * @brief Skeletonize a single mask element
 *
 * Element-level transform: Mask2D → Mask2D
 *
 * Converts the mask to a binary image on a canvas, applies the selected skeletonization
 * algorithm, and converts the result back to mask points.
 *
 * When canvas dimensions are omitted, the raster is sized to the mask bounding box
 * plus a one-pixel background border so thinning is not biased at the image edge.
 *
 * When applied to containers:
 * - MaskData → MaskData (one skeletonized mask per input mask)
 *
 * @param mask Input mask points
 * @param params Optional full-canvas dimensions; when unset, canvas is sized to mask extents
 * @return Skeletonized mask, or empty mask if input is empty or skeletonization yields no pixels
 */
Mask2D skeletonizeMask(
        Mask2D const & mask,
        MaskSkeletonizeParams const & params);

/**
 * @brief Skeletonize a mask with progress reporting and cancellation support
 *
 * @param mask Input mask points
 * @param params Optional full-canvas dimensions; when unset, canvas is sized to mask extents
 * @param ctx Compute context for progress and cancellation
 * @return Skeletonized mask, or empty mask on cancellation or empty input
 */
Mask2D skeletonizeMaskWithContext(
        Mask2D const & mask,
        MaskSkeletonizeParams const & params,
        ComputeContext const & ctx);

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_MASK_SKELETONIZE_TRANSFORM_HPP
