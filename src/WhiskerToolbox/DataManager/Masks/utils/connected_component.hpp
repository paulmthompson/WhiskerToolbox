#ifndef WHISKERTOOLBOX_CONNECTED_COMPONENT_HPP
#define WHISKERTOOLBOX_CONNECTED_COMPONENT_HPP

#include "ImageSize/ImageSize.hpp"

#include <cstdint>
#include <vector>

/**
 * @brief Removes small connected components from a binary image using flood-fill algorithm.
 * 
 * This function performs connected component analysis on a binary image to identify
 * all connected regions (clusters) of foreground pixels. Connected components smaller
 * than the specified threshold are removed, while larger components are preserved.
 * Uses 8-connectivity (considers diagonal neighbors as connected).
 * 
 * @param image Input binary image as a 1D vector where non-zero values represent foreground pixels
 * @param image_size Dimensions of the input image (width and height)
 * @param threshold Minimum size (in pixels) for a connected component to be preserved
 * 
 * @pre image.size() must equal image_size.width * image_size.height
 * @pre image_size.width and image_size.height must be greater than 0
 * @pre threshold must be greater than 0
 * @pre Image data should be binary (0 or non-zero values)
 * 
 * @post Returns a binary image of the same dimensions with small clusters removed
 * @post Preserved clusters maintain their original shape and position
 * 
 * @return std::vector<uint8_t> Filtered binary image with small clusters removed (values 0 or 1)
 */
std::vector<uint8_t> remove_small_clusters(std::vector<uint8_t> const & image, ImageSize image_size, int threshold);


#endif//WHISKERTOOLBOX_CONNECTED_COMPONENT_HPP
