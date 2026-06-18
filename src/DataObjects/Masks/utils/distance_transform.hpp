/**
 * @file distance_transform.hpp
 * @brief Euclidean distance transform for binary images (Felzenszwalb & Huttenlocher).
 */

#ifndef DISTANCE_TRANSFORM_HPP
#define DISTANCE_TRANSFORM_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

/**
 * @brief Compute the squared Euclidean distance transform of a 1D sampled function
 *
 * Ported from Felzenszwalb & Huttenlocher (GPL-2.0-or-later). See `distance_transform.cpp`.
 *
 * @param samples Input function samples (length @p count)
 * @pre samples must not be empty
 * @post Returned vector has the same length as @p samples
 * @return Squared distance transform of @p samples
 */
std::vector<float> distanceTransform1DSquared(std::span<float const> samples);

/**
 * @brief Compute the squared Euclidean distance transform of a 2D image in place
 *
 * @param image Row-major image buffer of size @p width * @p height (modified in place)
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @pre width and height must be greater than 0
 * @pre image.size() must equal width * height
 * @post Each pixel holds its squared distance to the nearest zero-valued pixel
 */
void distanceTransform2DSquaredInPlace(std::span<float> image, int width, int height);

/**
 * @brief Compute the squared Euclidean distance transform of a binary image
 *
 * Foreground pixels (@p foreground_value) are seeds at distance 0; background pixels start at
 * infinity and receive the squared distance to the nearest foreground pixel.
 *
 * @param binary Row-major binary image (0 = background seed region, non-zero = foreground)
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param foreground_value Value treated as foreground (distance 0)
 * @pre width and height must be greater than 0
 * @pre binary.size() must equal width * height
 * @return Squared distance transform
 */
std::vector<float> binaryDistanceTransformSquared(
        std::span<uint8_t const> binary,
        int width,
        int height,
        uint8_t foreground_value = 1);

/**
 * @brief Compute the Euclidean distance transform of a binary image
 *
 * Each pixel receives the distance to the nearest foreground (@p foreground_value) pixel.
 * Foreground pixels are seeds at distance 0.
 *
 * @param binary Row-major binary image (0 = background, non-zero = foreground)
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param foreground_value Value treated as foreground (distance 0)
 * @pre width and height must be greater than 0
 * @pre binary.size() must equal width * height
 * @return Euclidean distance transform (same layout as input)
 */
std::vector<float> binaryDistanceTransformEuclidean(
        std::span<uint8_t const> binary,
        int width,
        int height,
        uint8_t foreground_value = 1);

/**
 * @brief Euclidean distance from each foreground pixel to the nearest background pixel
 *
 * Matches `scipy.ndimage.distance_transform_edt` on a boolean mask where `True` is foreground.
 * Used by medial-axis skeletonization to order pixels by distance to the boundary.
 *
 * @param binary Row-major binary image (0 = background, non-zero = foreground)
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param foreground_value Value treated as foreground
 * @pre width and height must be greater than 0
 * @pre binary.size() must equal width * height
 * @return Per-pixel distance to the nearest background pixel (background pixels are 0)
 */
std::vector<float> binaryForegroundDistanceTransformEuclidean(
        std::span<uint8_t const> binary,
        int width,
        int height,
        uint8_t foreground_value = 1);

#endif// DISTANCE_TRANSFORM_HPP
