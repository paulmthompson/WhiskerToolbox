/**
 * @file MaskSkeletonize.cpp
 * @brief Implementation of element-level mask skeletonization (Mask2D → Mask2D).
 */

#include "MaskSkeletonize.hpp"

#include "CoreGeometry/Image.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/masks.hpp"
#include "Masks/utils/mask_utils.hpp"
#include "Masks/utils/medial_axis_skeletonize.hpp"
#include "Masks/utils/skeletonize.hpp"
#include "core/ComputeContext.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace WhiskerToolbox::Transforms::V2::Examples {

namespace {

/// @brief One-pixel border so Zhang–Suen thinning is not biased at the raster edge
static constexpr int kTightCanvasBorderPadding = 1;

/// @brief Whether params specify an explicit full canvas (e.g. from MaskData image size)
bool hasExplicitCanvas(MaskSkeletonizeParams const & params) {
    return params.image_width > 0 && params.image_height > 0;
}

/**
 * @brief Canvas sized to the axis-aligned bounding box of mask points plus border padding
 */
struct TightCanvas {
    ImageSize size;
    uint32_t min_x = 0;
    uint32_t min_y = 0;
};

/**
 * @brief Compute tight canvas dimensions from mask point min/max extents
 * @pre mask must not be empty
 * @post Canvas includes @ref kTightCanvasBorderPadding pixels of background on each side
 */
TightCanvas computeTightCanvas(Mask2D const & mask) {
    auto const [min_point, max_point] = get_bounding_box(mask);
    TightCanvas canvas;
    canvas.min_x = min_point.x;
    canvas.min_y = min_point.y;
    canvas.size.width = static_cast<int>(max_point.x - min_point.x + 1) +
                        2 * kTightCanvasBorderPadding;
    canvas.size.height = static_cast<int>(max_point.y - min_point.y + 1) +
                         2 * kTightCanvasBorderPadding;
    return canvas;
}

/**
 * @brief Convert mask points to a binary image on a tight local canvas with border padding
 * @pre mask must not be empty
 */
Image maskToBinaryImageTight(Mask2D const & mask, TightCanvas const & canvas) {
    std::vector<uint8_t> image_data(
            static_cast<size_t>(canvas.size.width) * static_cast<size_t>(canvas.size.height),
            0);

    for (auto const & point: mask) {
        int const x = static_cast<int>(point.x - canvas.min_x) + kTightCanvasBorderPadding;
        int const y = static_cast<int>(point.y - canvas.min_y) + kTightCanvasBorderPadding;
        image_data[static_cast<size_t>(y) * static_cast<size_t>(canvas.size.width) +
                   static_cast<size_t>(x)] = 1;
    }

    return {std::move(image_data), canvas.size};
}

/**
 * @brief Convert a padded tight-canvas binary image back to absolute mask coordinates
 */
Mask2D binaryImageToMaskWithTightCanvasOffset(Image const & binary_image, TightCanvas const & canvas) {
    int const offset_x = static_cast<int>(canvas.min_x) - kTightCanvasBorderPadding;
    int const offset_y = static_cast<int>(canvas.min_y) - kTightCanvasBorderPadding;

    std::vector<Point2D<uint32_t>> mask_points;

    for (int y = 0; y < binary_image.size.height; ++y) {
        for (int x = 0; x < binary_image.size.width; ++x) {
            if (binary_image.at(y, x) > 0) {
                mask_points.emplace_back(static_cast<uint32_t>(x + offset_x),
                                         static_cast<uint32_t>(y + offset_y));
            }
        }
    }

    return Mask2D(std::move(mask_points));
}

/**
 * @brief Apply the selected skeletonization algorithm to a binary image
 */
Image skeletonizeBinaryImage(Image const & binary_image, SkeletonizeMethod method) {
    switch (method) {
        case SkeletonizeMethod::MedialAxis:
            return medial_axis_skeletonize(binary_image);
        case SkeletonizeMethod::ZhangSuen:
            return fast_skeletonize(binary_image);
    }
    return fast_skeletonize(binary_image);
}

/**
 * @brief Skeletonize using an explicit full canvas (absolute mask coordinates)
 */
Mask2D skeletonizeMaskOnExplicitCanvas(
        Mask2D const & mask,
        ImageSize const & image_size,
        SkeletonizeMethod method) {
    Image const binary_image = mask_to_binary_image(mask, image_size);
    Image const skeleton_image = skeletonizeBinaryImage(binary_image, method);
    return binary_image_to_mask(skeleton_image);
}

/**
 * @brief Skeletonize using a tight per-mask canvas derived from point extents
 * @pre mask must not be empty
 */
Mask2D skeletonizeMaskOnTightCanvas(Mask2D const & mask, SkeletonizeMethod method) {
    TightCanvas const canvas = computeTightCanvas(mask);
    Image const binary_image = maskToBinaryImageTight(mask, canvas);
    Image const skeleton_image = skeletonizeBinaryImage(binary_image, method);
    return binaryImageToMaskWithTightCanvasOffset(skeleton_image, canvas);
}

}// namespace

Mask2D skeletonizeMask(
        Mask2D const & mask,
        MaskSkeletonizeParams const & params) {

    if (mask.empty()) {
        return {};
    }

    Mask2D result;
    if (hasExplicitCanvas(params)) {
        ImageSize const image_size{params.image_width, params.image_height};
        result = skeletonizeMaskOnExplicitCanvas(mask, image_size, params.method);
    } else {
        result = skeletonizeMaskOnTightCanvas(mask, params.method);
    }

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
