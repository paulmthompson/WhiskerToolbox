#ifndef MASK_UTILS_HPP
#define MASK_UTILS_HPP

/**
 * @file mask_utils.hpp
 * @brief Utilities for mask raster conversion, resize, and binary image processing.
 *
 * @par Mask raster vs continuous geometry
 * This header operates on **discrete mask rasters** (`Mask2D` sparse pixel lists).
 * It does not perform continuous point/line scaling; see DeepLearning channel
 * encoders/decoders for float geometry.
 *
 * @par Spatial convention (resize functions)
 * `map_dest_to_source` and `resize_mask` use the **pixel-center** grid
 * (`align_corners=false`), consistent with:
 * - PyTorch `ImageEncoder` / `interpolate(..., align_corners=false)`
 * - OpenCV `cv2.resize` sampling geometry
 * - `TensorToMask2D` mask decode (DeepLearning)
 *
 * Integer pixel indices are treated as **pixel centers** at `(x + 0.5, y + 0.5)`
 * in continuous image space. Inverse nearest-neighbor mapping per axis:
 *
 * @code
 * src = clamp(lround((dest + 0.5) * source_size / dest_size - 0.5), 0, source_size - 1)
 * @endcode
 *
 * see `docs/developer/DataObjects/Masks/utils/mask_utils.qmd` for the
 * pixel-center vs edge-aligned comparison table.
 *
 * @par Non-resize functions
 * `mask_to_binary_image` and `binary_image_to_mask` place or read pixels on an
 * integer grid **without** resampling — no spatial convention applies beyond
 * canvas bounds.
 */

#include "CoreGeometry/Image.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

class MaskData;

/**
 * @brief Applies a binary image processing function to mask data.
 *
 * Abstracts the common pattern of converting mask data to binary images,
 * applying an algorithm, and converting back to mask data.
 *
 * @param mask_data The input mask data to process.
 * @param binary_processor Function that takes a binary image and returns a processed binary image.
 * @param progress_callback Function for progress reporting (0–100).
 * @param preserve_empty_masks If true, empty masks are preserved in the output.
 *
 * @return A new MaskData containing the processed masks.
 *
 * @note No spatial resize — uses `mask_data` image size as-is.
 * @note @p binary_processor should expect `Image` and return `Image`.
 */
std::shared_ptr<MaskData> apply_binary_image_algorithm(
        MaskData const * mask_data,
        std::function<Image(Image const &)> const & binary_processor,
        std::function<void(int)> const & progress_callback = [](int) {},
        bool preserve_empty_masks = false);

/**
 * @brief Converts a single mask to a binary image.
 *
 * @param mask The mask points to convert.
 * @param image_size The dimensions of the output image.
 *
 * @return Binary image where mask points are set to 1 and others to 0.
 *
 * @note No spatial resize — writes mask points into `image_size` verbatim.
 */
Image mask_to_binary_image(Mask2D const & mask, ImageSize image_size);

/**
 * @brief Converts a binary image back to mask points.
 *
 * @param binary_image The binary image to convert.
 *
 * @return Mask containing points where the image value is greater than zero.
 *
 * @note No spatial resize — reads `binary_image` pixels directly.
 */
Mask2D binary_image_to_mask(Image const & binary_image);

/**
 * @brief Map a destination pixel index to the nearest source index (one axis).
 *
 * @note Spatial convention: **PixelCenter** (`align_corners=false`).
 *       Inverse nearest-neighbor: samples the source cell that owns the
 *       destination pixel center.
 *
 * @param dest_coord Destination pixel index along one axis.
 * @param source_size Source extent along that axis (must be > 0).
 * @param dest_size Destination extent along that axis (must be > 0).
 * @return Clamped source index in `[0, source_size - 1]`.
 *
 * @pre source_size > 0 and dest_size > 0
 */
[[nodiscard]] int map_dest_to_source(int dest_coord, int source_size, int dest_size);

/**
 * @brief Resize a mask raster between canvas sizes using inverse nearest-neighbor.
 *
 * Rasterizes the source mask, resamples the binary grid with
 * @ref map_dest_to_source per destination pixel, then re-extracts mask points.
 *
 * @note Spatial convention: **PixelCenter** (`align_corners=false`), same as
 *       `TensorToMask2D` when upsampling model output to `target_image_size`.
 *
 * @param mask Input mask in `source_size` coordinates.
 * @param source_size Canvas the input coordinates refer to.
 * @param dest_size Target canvas size.
 *
 * @return Mask in `dest_size` coordinates, or empty on invalid input.
 *
 * @note Returns empty if @p mask is empty or any canvas dimension is invalid (<= 0).
 * @note Uses nearest-neighbor resampling to keep the mask binary.
 */
Mask2D resize_mask(Mask2D const & mask, ImageSize const & source_size, ImageSize const & dest_size);

#endif// MASK_UTILS_HPP
