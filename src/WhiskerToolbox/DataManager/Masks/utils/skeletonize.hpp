#ifndef SKELETONIZE_HPP
#define SKELETONIZE_HPP

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
 * 
 * @post Returns a binary image of the same dimensions with skeletonized objects
 * 
 * @return std::vector<uint8_t> Skeletonized binary image as a 1D vector
 */
std::vector<uint8_t> fast_skeletonize(std::vector<uint8_t> const & image, size_t height, size_t width);


#endif// SKELETONIZE_HPP
