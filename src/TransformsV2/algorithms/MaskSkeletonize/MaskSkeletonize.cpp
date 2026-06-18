/**
 * @file MaskSkeletonize.cpp
 * @brief Implementation of element-level mask skeletonization (Mask2D → Mask2D).
 */

#include "MaskSkeletonize.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/masks.hpp"
#include "Masks/utils/mask_utils.hpp"
#include "Masks/utils/skeletonize.hpp"
#include "core/ComputeContext.hpp"

namespace WhiskerToolbox::Transforms::V2::Examples {

namespace {

constexpr int kDefaultCanvasSize = 256;

/**
 * @brief Resolve binary-image canvas size from parameters
 * @pre When using explicit dimensions, both width and height must be positive
 */
ImageSize resolveCanvasSize(MaskSkeletonizeParams const & params) {
    if (params.image_width > 0 && params.image_height > 0) {
        return ImageSize{params.image_width, params.image_height};
    }
    return ImageSize{kDefaultCanvasSize, kDefaultCanvasSize};
}

}// namespace

Mask2D skeletonizeMask(
        Mask2D const & mask,
        MaskSkeletonizeParams const & params) {

    if (mask.empty()) {
        return {};
    }

    ImageSize const image_size = resolveCanvasSize(params);
    Image const binary_image = mask_to_binary_image(mask, image_size);
    Image const skeleton_image = fast_skeletonize(binary_image);
    Mask2D const result = binary_image_to_mask(skeleton_image);

    if (result.empty()) {
        return {};
    }

    return result;
}

Mask2D skeletonizeMaskWithContext(
        Mask2D const & mask,
        MaskSkeletonizeParams const & params,
        ComputeContext const & ctx) {

    if (ctx.shouldCancel()) {
        return {};
    }

    ctx.reportProgress(0);

    auto const result = skeletonizeMask(mask, params);

    if (ctx.shouldCancel()) {
        return {};
    }

    ctx.reportProgress(100);
    return result;
}

}// namespace WhiskerToolbox::Transforms::V2::Examples
