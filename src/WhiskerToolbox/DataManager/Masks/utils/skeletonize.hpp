#ifndef SKELETONIZE_HPP
#define SKELETONIZE_HPP

#include "Image.hpp"
#include "ImageSize/ImageSize.hpp"

#include <cstdint>
#include <vector>

/**
 * @brief Performs fast skeletonization of a binary image using morphological thinning.
 * 
 * This function reduces binary objects in an image to their skeletal representation
 * using an iterative morphological thinning algorithm adapted from scikit-image.
 * The skeleton preserves the topology and general shape of the original objects
 * while reducing them to lines of single-pixel width.
 * 
 * @param image Input binary image as a 1D vector where non-zero values represent foreground pixels
 * @param height Height of the input image in pixels
 * @param width Width of the input image in pixels
 * 
 * @pre image.size() must equal height * width
 * @pre height and width must be greater than 0
 * @pre Image data should be binary (0 or non-zero values)
 * @pre Image data is expected in row-major order: index = row * width + col
 * 
 * @post Returns a binary image of the same dimensions with skeletonized objects
 * @post Output values are strictly binary: 0 (background) or 1 (skeleton pixels)
 * 
 * @return std::vector<uint8_t> Skeletonized binary image as a 1D vector in row-major order (values: 0 or 1)
 */
std::vector<uint8_t> fast_skeletonize(std::vector<uint8_t> const & image, size_t height, size_t width);

/**
 * @brief Performs fast skeletonization of a binary image using morphological thinning.
 * 
 * This is the preferred interface that uses the Image struct to ensure consistency
 * in data layout and dimensions. The input image data must be in row-major order.
 * 
 * @param input_image Input binary image where non-zero values represent foreground pixels
 * 
 * @pre input_image.data.size() must equal input_image.size.width * input_image.size.height
 * @pre input_image.size.width and input_image.size.height must be greater than 0
 * @pre Image data should be binary (0 or non-zero values)
 * @pre Image data must be in row-major order as documented in Image struct
 * 
 * @post Returns a binary image of the same dimensions with skeletonized objects
 * @post Output image maintains row-major data layout
 * @post Output values are strictly binary: 0 (background) or 1 (skeleton pixels)
 * 
 * @return Image Skeletonized binary image with same dimensions as input (values: 0 or 1)
 */
Image fast_skeletonize(Image const & input_image);


#endif// SKELETONIZE_HPP
