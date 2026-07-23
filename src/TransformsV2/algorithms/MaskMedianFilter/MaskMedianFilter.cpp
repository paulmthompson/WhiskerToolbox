/**
 * @file MaskMedianFilter.cpp
 * @brief Implementation of element-level mask median filtering (Mask2D → Mask2D).
 */

#include "MaskMedianFilter.hpp"

#include "CoreGeometry/Image.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/masks.hpp"
#include "Masks/utils/mask_utils.hpp"
#include "Masks/utils/median_filter.hpp"
#include "core/ComputeContext.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace Neuralyzer::Transforms::V2::Examples {

namespace {

/**
 * @brief Background border padding for tight-canvas rasterization
 * @pre window_size must be odd and positive
 */
int tightCanvasBorderPadding(int window_size) {
    return std::max(1, window_size / 2);
}

/// @brief Whether params specify an explicit full canvas (e.g. from MaskData image size)
bool hasExplicitCanvas(MaskMedianFilterParams const & params) {
    return params.image_width > 0 && params.image_height > 0;
}

/**
 * @brief Normalize window size to an odd value >= 1
 */
int normalizeWindowSize(int window_size) {
    if (window_size <= 0 || window_size % 2 == 0) {
        return 3;
    }
    return window_size;
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
 */
TightCanvas computeTightCanvas(Mask2D const & mask, int border_padding) {
    auto const [min_point, max_point] = get_bounding_box(mask);
    TightCanvas canvas;
    canvas.min_x = min_point.x;
    canvas.min_y = min_point.y;
    canvas.size.width = static_cast<int>(max_point.x - min_point.x + 1) + 2 * border_padding;
    canvas.size.height = static_cast<int>(max_point.y - min_point.y + 1) + 2 * border_padding;
    return canvas;
}

/**
 * @brief Convert mask points to a binary image on a tight local canvas with border padding
 * @pre mask must not be empty
 */
Image maskToBinaryImageTight(Mask2D const & mask, TightCanvas const & canvas, int border_padding) {
    std::vector<uint8_t> image_data(
            static_cast<size_t>(canvas.size.width) * static_cast<size_t>(canvas.size.height),
            0);

    for (auto const & point: mask) {
        int const x = static_cast<int>(point.x - canvas.min_x) + border_padding;
        int const y = static_cast<int>(point.y - canvas.min_y) + border_padding;
        image_data[static_cast<size_t>(y) * static_cast<size_t>(canvas.size.width) +
                   static_cast<size_t>(x)] = 1;
    }

    return {std::move(image_data), canvas.size};
}

/**
 * @brief Convert a padded tight-canvas binary image back to absolute mask coordinates
 */
Mask2D binaryImageToMaskWithTightCanvasOffset(
        Image const & binary_image,
        TightCanvas const & canvas,
        int border_padding) {
    int const offset_x = static_cast<int>(canvas.min_x) - border_padding;
    int const offset_y = static_cast<int>(canvas.min_y) - border_padding;

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
 * @brief Median-filter using an explicit full canvas (absolute mask coordinates)
 */
Mask2D applyMedianFilterOnExplicitCanvas(
        Mask2D const & mask,
        ImageSize const & image_size,
        int window_size) {
    Image const binary_image = mask_to_binary_image(mask, image_size);
    Image const filtered_image = median_filter(binary_image, window_size);
    return binary_image_to_mask(filtered_image);
}

/**
 * @brief Median-filter using a tight per-mask canvas derived from point extents
 * @pre mask must not be empty
 */
Mask2D applyMedianFilterOnTightCanvas(Mask2D const & mask, int window_size) {
    int const border_padding = tightCanvasBorderPadding(window_size);
    TightCanvas const canvas = computeTightCanvas(mask, border_padding);
    Image const binary_image = maskToBinaryImageTight(mask, canvas, border_padding);
    Image const filtered_image = median_filter(binary_image, window_size);
    return binaryImageToMaskWithTightCanvasOffset(filtered_image, canvas, border_padding);
}

}// namespace

Mask2D applyMedianFilter(
        Mask2D const & mask,
        MaskMedianFilterParams const & params) {

    if (mask.empty()) {
        return {};
    }

    int const window_size = normalizeWindowSize(params.window_size);

    Mask2D result;
    if (hasExplicitCanvas(params)) {
        ImageSize const image_size{params.image_width, params.image_height};
        result = applyMedianFilterOnExplicitCanvas(mask, image_size, window_size);
    } else {
        result = applyMedianFilterOnTightCanvas(mask, window_size);
    }

    if (result.empty()) {
        return {};
    }

    return result;
}

Mask2D applyMedianFilterWithContext(
        Mask2D const & mask,
        MaskMedianFilterParams const & params,
        ComputeContext const & ctx) {

    if (ctx.shouldCancel()) {
        return {};
    }

    ctx.reportProgress(0);

    auto const result = applyMedianFilter(mask, params);

    if (ctx.shouldCancel()) {
        return {};
    }

    ctx.reportProgress(100);
    return result;
}

}// namespace Neuralyzer::Transforms::V2::Examples
