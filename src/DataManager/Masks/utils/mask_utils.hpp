#ifndef MASK_UTILS_HPP
#define MASK_UTILS_HPP

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
 * @brief Applies a binary image processing function to mask data
 * 
 * This utility function abstracts the common pattern of converting mask data
 * to binary images, applying an algorithm, and converting back to mask data.
 * It handles the conversion process and provides progress reporting.
 * 
 * @param mask_data The input mask data to process
 * @param binary_processor Function that takes a binary image and returns a processed binary image
 * @param progress_callback Function for progress reporting (0-100)
 * @param preserve_empty_masks If true, empty masks will be preserved in output (default: false)
 * 
 * @return A new MaskData containing the processed masks
 * 
 * @note The binary_processor function should expect Image struct and return Image struct
 */
std::shared_ptr<MaskData> apply_binary_image_algorithm(
    MaskData const * mask_data,
    std::function<Image(Image const &)> binary_processor,
    std::function<void(int)> progress_callback = [](int){},
    bool preserve_empty_masks = false);

/**
 * @brief Converts a single mask to a binary image
 * 
 * @param mask The mask points to convert
 * @param image_size The dimensions of the output image
 * @return Image Binary image where mask points are set to 1, others to 0
 */
Image mask_to_binary_image(std::vector<Point2D<uint32_t>> const & mask, ImageSize image_size);

/**
 * @brief Converts a binary image back to mask points
 * 
 * @param binary_image The binary image to convert
 * @return std::vector<Point2D<uint32_t>> Vector of points where image value > 0
 */
std::vector<Point2D<uint32_t>> binary_image_to_mask(Image const & binary_image);

/**
 * @brief Resize a mask from one image size to another using nearest neighbor interpolation
 *
 * Converts mask coordinates from source image dimensions to destination image dimensions
 * using custom nearest neighbor interpolation. This ensures the mask remains homogeneous
 * (binary) after resizing.
 *
 * @param mask The input mask to resize
 * @param source_size The original image size that the mask coordinates correspond to
 * @param dest_size The target image size to resize the mask to
 * @return New mask with coordinates scaled to the destination image size
 *
 * @note If source or destination dimensions are invalid (<=0), returns empty mask
 * @note Uses custom nearest neighbor interpolation to maintain binary mask properties
 * @note Does not depend on OpenCV - uses only standard library functions
 */
Mask2D resize_mask(Mask2D const & mask, ImageSize const & source_size, ImageSize const & dest_size);

#endif // MASK_UTILS_HPP 