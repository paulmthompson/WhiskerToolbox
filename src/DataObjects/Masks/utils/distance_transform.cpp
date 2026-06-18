/**
 * @file distance_transform.cpp
 * @brief Euclidean distance transform for binary images.
 *
 * Algorithm from:
 *   Distance Transforms of Sampled Functions
 *   Pedro F. Felzenszwalb and Daniel P. Huttenlocher
 *   Cornell Computing and Information Science TR2004-1963
 *
 * Copyright (C) 2006 Pedro Felzenszwalb
 * License: GPL-2.0-or-later
 */

#include "distance_transform.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace {

constexpr float kDistanceTransformInf = 1.0e20F;

float square(float value) {
    return value * value;
}

std::vector<float> finalizeSquaredDistanceTransform(std::vector<float> distance) {
    for (float & value: distance) {
        if (value >= kDistanceTransformInf / 2.0F) {
            value = std::numeric_limits<float>::infinity();
        } else {
            value = std::sqrt(value);
        }
    }
    return distance;
}

/**
 * @brief Initialize squared DT seeds and run the 2D transform
 * @param binary Input mask
 * @param seed_is_foreground When true, foreground pixels are zero seeds (distance to foreground).
 *                           When false, background pixels are zero seeds (distance to background).
 */
std::vector<float> binaryDistanceTransformSquaredSeeded(
        std::span<uint8_t const> binary,
        int width,
        int height,
        uint8_t foreground_value,
        bool seed_is_foreground) {

    assert(width > 0 && height > 0 && "binaryDistanceTransformSquaredSeeded: dimensions must be positive");
    assert(binary.size() == static_cast<size_t>(width) * static_cast<size_t>(height) &&
           "binaryDistanceTransformSquaredSeeded: buffer size mismatch");

    std::vector<float> distance(binary.size());
    for (size_t i = 0; i < binary.size(); ++i) {
        bool const is_foreground = binary[i] == foreground_value;
        bool const is_seed = seed_is_foreground ? is_foreground : !is_foreground;
        distance[i] = is_seed ? 0.0F : kDistanceTransformInf;
    }

    distanceTransform2DSquaredInPlace(distance, width, height);
    return distance;
}

/**
 * @brief 1D squared distance transform of a sampled function
 * @pre count must be greater than 0
 */
std::vector<float> distanceTransform1DSquaredImpl(std::span<float const> samples) {
    assert(!samples.empty() && "distanceTransform1DSquaredImpl: samples must not be empty");

    int const count = static_cast<int>(samples.size());
    std::vector<float> distance(static_cast<size_t>(count));
    std::vector<int> vertices(static_cast<size_t>(count));
    std::vector<float> intersections(static_cast<size_t>(count) + 1U);

    int k = 0;
    vertices[0] = 0;
    intersections[0] = -kDistanceTransformInf;
    intersections[1] = kDistanceTransformInf;

    for (int q = 1; q < count; ++q) {
        float intersection = ((samples[static_cast<size_t>(q)] + square(static_cast<float>(q))) -
                              (samples[static_cast<size_t>(vertices[static_cast<size_t>(k)])] +
                               square(static_cast<float>(vertices[static_cast<size_t>(k)])))) /
                             (2.0F * static_cast<float>(q - vertices[static_cast<size_t>(k)]));

        while (intersection <= intersections[static_cast<size_t>(k)]) {
            --k;
            intersection = ((samples[static_cast<size_t>(q)] + square(static_cast<float>(q))) -
                            (samples[static_cast<size_t>(vertices[static_cast<size_t>(k)])] +
                             square(static_cast<float>(vertices[static_cast<size_t>(k)])))) /
                           (2.0F * static_cast<float>(q - vertices[static_cast<size_t>(k)]));
        }

        ++k;
        vertices[static_cast<size_t>(k)] = q;
        intersections[static_cast<size_t>(k)] = intersection;
        intersections[static_cast<size_t>(k) + 1U] = kDistanceTransformInf;
    }

    k = 0;
    for (int q = 0; q < count; ++q) {
        while (intersections[static_cast<size_t>(k) + 1U] < static_cast<float>(q)) {
            ++k;
        }

        int const vertex = vertices[static_cast<size_t>(k)];
        float const delta = static_cast<float>(q - vertex);
        distance[static_cast<size_t>(q)] =
                square(delta) + samples[static_cast<size_t>(vertex)];
    }

    return distance;
}

}// namespace

std::vector<float> distanceTransform1DSquared(std::span<float const> samples) {
    assert(!samples.empty() && "distanceTransform1DSquared: samples must not be empty");
    return distanceTransform1DSquaredImpl(samples);
}

void distanceTransform2DSquaredInPlace(std::span<float> image, int width, int height) {
    assert(width > 0 && height > 0 && "distanceTransform2DSquaredInPlace: dimensions must be positive");
    assert(static_cast<size_t>(width) * static_cast<size_t>(height) == image.size() &&
           "distanceTransform2DSquaredInPlace: buffer size mismatch");

    int const max_dim = std::max(width, height);
    std::vector<float> column_buffer(static_cast<size_t>(max_dim));

    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            column_buffer[static_cast<size_t>(y)] =
                    image[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)];
        }

        std::vector<float> const transformed = distanceTransform1DSquaredImpl(
                std::span(column_buffer).subspan(0, static_cast<size_t>(height)));

        for (int y = 0; y < height; ++y) {
            image[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] =
                    transformed[static_cast<size_t>(y)];
        }
    }

    for (int y = 0; y < height; ++y) {
        size_t const row_offset = static_cast<size_t>(y) * static_cast<size_t>(width);
        for (int x = 0; x < width; ++x) {
            column_buffer[static_cast<size_t>(x)] = image[row_offset + static_cast<size_t>(x)];
        }

        std::vector<float> const transformed = distanceTransform1DSquaredImpl(
                std::span(column_buffer).subspan(0, static_cast<size_t>(width)));

        for (int x = 0; x < width; ++x) {
            image[row_offset + static_cast<size_t>(x)] = transformed[static_cast<size_t>(x)];
        }
    }
}

std::vector<float> binaryDistanceTransformSquared(
        std::span<uint8_t const> binary,
        int width,
        int height,
        uint8_t foreground_value) {

    return binaryDistanceTransformSquaredSeeded(binary, width, height, foreground_value, true);
}

std::vector<float> binaryDistanceTransformEuclidean(
        std::span<uint8_t const> binary,
        int width,
        int height,
        uint8_t foreground_value) {

    return finalizeSquaredDistanceTransform(
            binaryDistanceTransformSquared(binary, width, height, foreground_value));
}

std::vector<float> binaryForegroundDistanceTransformEuclidean(
        std::span<uint8_t const> binary,
        int width,
        int height,
        uint8_t foreground_value) {

    return finalizeSquaredDistanceTransform(
            binaryDistanceTransformSquaredSeeded(binary, width, height, foreground_value, false));
}
