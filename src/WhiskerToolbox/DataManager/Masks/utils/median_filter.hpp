#ifndef WHISKERTOOLBOX_MEDIAN_FILTER_HPP
#define WHISKERTOOLBOX_MEDIAN_FILTER_HPP

#include "Image.hpp"
#include "ImageSize/ImageSize.hpp"

#include <cstdint>
#include <vector>

/**
 * @brief Applies median filtering to a binary image using a square window.
 * 
 * Median filtering replaces each pixel with the median value of pixels in its neighborhood.
 * For binary images, this effectively removes small isolated foreground pixels (noise)
 * and fills small gaps in objects. The median is computed over a square window of 
 * specified size centered on each pixel.
 * 
 * @param image Input binary image as a 1D vector where non-zero values represent foreground pixels
 * @param image_size Dimensions of the input image (width and height)
 * @param window_size Size of the square window (must be odd and >= 1)
 * 
 * @pre image.size() must equal image_size.width * image_size.height
 * @pre image_size.width and image_size.height must be greater than 0
 * @pre window_size must be odd and >= 1
 * @pre Image data should be binary (0 or non-zero values)
 * @pre Image data is expected in row-major order: index = row * width + col
 * 
 * @post Returns a binary image of the same dimensions with median filtering applied
 * @post Output values are normalized to 0 or 1
 * @post Boundary pixels are handled using reflection padding
 * 
 * @return std::vector<uint8_t> Filtered binary image (values 0 or 1) in row-major order
 */
std::vector<uint8_t> median_filter(std::vector<uint8_t> const & image, ImageSize image_size, int window_size);

/**
 * @brief Applies median filtering to a binary image using a square window.
 * 
 * This is the preferred interface that uses the Image struct to ensure consistency
 * in data layout and dimensions. Uses reflection padding at image boundaries.
 * 
 * @param input_image Input binary image where non-zero values represent foreground pixels
 * @param window_size Size of the square window (must be odd and >= 1)
 * 
 * @pre input_image.data.size() must equal input_image.size.width * input_image.size.height
 * @pre input_image.size.width and input_image.size.height must be greater than 0
 * @pre window_size must be odd and >= 1
 * @pre Image data should be binary (0 or non-zero values)
 * @pre Image data must be in row-major order as documented in Image struct
 * 
 * @post Returns a binary image of the same dimensions with median filtering applied
 * @post Output values are normalized to 0 or 1
 * @post Output image maintains row-major data layout
 * @post Boundary pixels are handled using reflection padding
 * 
 * @return Image Filtered binary image (values 0 or 1)
 */
Image median_filter(Image const & input_image, int window_size);

#endif//WHISKERTOOLBOX_MEDIAN_FILTER_HPP 