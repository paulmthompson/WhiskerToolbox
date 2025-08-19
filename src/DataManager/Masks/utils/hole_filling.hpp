#ifndef WHISKERTOOLBOX_HOLE_FILLING_HPP
#define WHISKERTOOLBOX_HOLE_FILLING_HPP

#include "CoreGeometry/Image.hpp"
#include "CoreGeometry/ImageSize.hpp"

#include <cstdint>
#include <vector>

/**
 * @brief Fills holes in a binary image using flood fill algorithm from boundaries.
 * 
 * This function identifies and fills holes (enclosed regions of background pixels)
 * within foreground regions of a binary image. The algorithm works by:
 * 1. Creating an inverted copy of the image
 * 2. Flood filling from image boundaries to identify pixels connected to the border
 * 3. Pixels that are background but NOT connected to the border are holes
 * 4. Filling these holes in the original image
 * 
 * @param image Input binary image as a 1D vector where non-zero values represent foreground pixels
 * @param image_size Dimensions of the input image (width and height)
 * 
 * @pre image.size() must equal image_size.width * image_size.height
 * @pre image_size.width and image_size.height must be greater than 0
 * @pre Image data should be binary (0 or non-zero values)
 * @pre Image data is expected in row-major order: index = row * width + col
 * 
 * @post Returns a binary image of the same dimensions with holes filled
 * @post Original foreground regions are preserved
 * @post Background regions connected to image boundaries remain unchanged
 * 
 * @return std::vector<uint8_t> Binary image with holes filled (values 0 or 1) in row-major order
 */
std::vector<uint8_t> fill_holes(std::vector<uint8_t> const & image, ImageSize image_size);

/**
 * @brief Fills holes in a binary image using flood fill algorithm from boundaries.
 * 
 * This is the preferred interface that uses the Image struct to ensure consistency
 * in data layout and dimensions.
 * 
 * @param input_image Input binary image where non-zero values represent foreground pixels
 * 
 * @pre input_image.data.size() must equal input_image.size.width * input_image.size.height
 * @pre input_image.size.width and input_image.size.height must be greater than 0
 * @pre Image data should be binary (0 or non-zero values)
 * @pre Image data must be in row-major order as documented in Image struct
 * 
 * @post Returns a binary image of the same dimensions with holes filled
 * @post Original foreground regions are preserved
 * @post Background regions connected to image boundaries remain unchanged
 * @post Output image maintains row-major data layout
 * 
 * @return Image Binary image with holes filled (values 0 or 1)
 */
Image fill_holes(Image const & input_image);

#endif//WHISKERTOOLBOX_HOLE_FILLING_HPP 