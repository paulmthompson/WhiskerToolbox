/**
 * @file medial_axis_skeletonize.hpp
 * @brief Medial-axis skeletonization via distance transform and single-pass thinning.
 */

#ifndef MEDIAL_AXIS_SKELETONIZE_HPP
#define MEDIAL_AXIS_SKELETONIZE_HPP

#include "CoreGeometry/Image.hpp"

#include <cstdint>
#include <vector>

/**
 * @brief Reduce binary foreground objects to a medial-axis skeleton
 *
 * Uses the Euclidean distance transform (Felzenszwalb) and a single-pass connectivity-preserving
 * thinning loop adapted from scikit-image's `medial_axis`.
 *
 * @param image Input binary image where non-zero values represent foreground
 * @pre image.size.width and image.size.height must be greater than 0
 * @post Returned image has the same dimensions with values 0 (background) or 1 (skeleton)
 * @return Skeletonized binary image
 */
Image medial_axis_skeletonize(Image const & image);

/**
 * @brief Reduce a row-major binary buffer to a medial-axis skeleton
 *
 * @param image Input binary image buffer in row-major order
 * @param height Image height in pixels
 * @param width Image width in pixels
 * @pre image.size() must equal height * width
 * @pre height and width must be greater than 0
 * @return Skeletonized binary buffer (values 0 or 1)
 */
std::vector<uint8_t> medial_axis_skeletonize(
        std::vector<uint8_t> const & image,
        size_t height,
        size_t width);

#endif// MEDIAL_AXIS_SKELETONIZE_HPP
