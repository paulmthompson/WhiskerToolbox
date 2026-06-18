/**
 * @file medial_axis_skeletonize.cpp
 * @brief Medial-axis skeletonization via distance transform and single-pass thinning.
 *
 * Medial-axis thinning loop adapted from scikit-image morphology/_skeletonize.py and
 * morphology/_skeletonize_various_cy.pyx (_skeletonize_loop).
 *
 * Copyright: Massachusetts Institute of Technology, Broad Institute, scikit-image contributors
 * License: BSD-3-Clause
 *
 * Distance transform: Felzenszwalb & Huttenlocher (GPL-2.0-or-later), see distance_transform.cpp
 */

#include "medial_axis_skeletonize.hpp"

#include "distance_transform.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <span>
#include <utility>
#include <vector>

namespace {

// Lookup table: keep (1) or remove (0) each 3x3 neighborhood configuration.
// Generated from scikit-image medial_axis connectivity rules.
constexpr std::array<uint8_t, 512> kMedialAxisKeepTable = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// Cornerness score: 9 minus foreground count in the 3x3 neighborhood.
constexpr std::array<uint8_t, 512> kCornernessTable = {
        9, 8, 8, 7, 8, 7, 7, 6, 8, 7, 7, 6, 7, 6, 6, 5,
        8, 7, 7, 6, 7, 6, 6, 5, 7, 6, 6, 5, 6, 5, 5, 4,
        8, 7, 7, 6, 7, 6, 6, 5, 7, 6, 6, 5, 6, 5, 5, 4,
        7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
        8, 7, 7, 6, 7, 6, 6, 5, 7, 6, 6, 5, 6, 5, 5, 4,
        7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
        7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
        6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
        8, 7, 7, 6, 7, 6, 6, 5, 7, 6, 6, 5, 6, 5, 5, 4,
        7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
        7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
        6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
        7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
        6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
        6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
        5, 4, 4, 3, 4, 3, 3, 2, 4, 3, 3, 2, 3, 2, 2, 1,
        8, 7, 7, 6, 7, 6, 6, 5, 7, 6, 6, 5, 6, 5, 5, 4,
        7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
        7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
        6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
        7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
        6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
        6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
        5, 4, 4, 3, 4, 3, 3, 2, 4, 3, 3, 2, 3, 2, 2, 1,
        7, 6, 6, 5, 6, 5, 5, 4, 6, 5, 5, 4, 5, 4, 4, 3,
        6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
        6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
        5, 4, 4, 3, 4, 3, 3, 2, 4, 3, 3, 2, 3, 2, 2, 1,
        6, 5, 5, 4, 5, 4, 4, 3, 5, 4, 4, 3, 4, 3, 3, 2,
        5, 4, 4, 3, 4, 3, 3, 2, 4, 3, 3, 2, 3, 2, 2, 1,
        5, 4, 4, 3, 4, 3, 3, 2, 4, 3, 3, 2, 3, 2, 2, 1,
        4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0,
};

struct ForegroundPixel {
    int row = 0;
    int col = 0;
    float distance = 0.0F;
    uint8_t cornerness = 0;
};

/**
 * @brief Compute the 9-neighbor configuration index at (@p col, @p row)
 *
 * Pixel numbering matches scikit-image `_skeletonize_loop`:
 * @code
 *  1   2   4
 *  8  16  32
 * 64 128 256
 * @endcode
 */
int computeNeighborhoodIndex(
        std::span<uint8_t const> image,
        int width,
        int height,
        int col,
        int row) {

    int accumulator = 16;

    if (row > 0) {
        size_t const north = static_cast<size_t>(row - 1) * static_cast<size_t>(width) +
                             static_cast<size_t>(col);
        if (col > 0 && image[north - 1U] > 0) {
            accumulator += 1;
        }
        if (image[north] > 0) {
            accumulator += 2;
        }
        if (col < width - 1 && image[north + 1U] > 0) {
            accumulator += 4;
        }
    }

    size_t const center = static_cast<size_t>(row) * static_cast<size_t>(width) +
                          static_cast<size_t>(col);
    if (col > 0 && image[center - 1U] > 0) {
        accumulator += 8;
    }
    if (col < width - 1 && image[center + 1U] > 0) {
        accumulator += 32;
    }

    if (row < height - 1) {
        size_t const south = static_cast<size_t>(row + 1) * static_cast<size_t>(width) +
                             static_cast<size_t>(col);
        if (col > 0 && image[south - 1U] > 0) {
            accumulator += 64;
        }
        if (image[south] > 0) {
            accumulator += 128;
        }
        if (col < width - 1 && image[south + 1U] > 0) {
            accumulator += 256;
        }
    }

    return accumulator;
}

/**
 * @brief Single-pass medial-axis thinning loop (scikit-image `_skeletonize_loop`)
 */
void medialAxisSkeletonizeLoop(
        std::span<uint8_t> result,
        int width,
        int height,
        std::span<int const> rows,
        std::span<int const> cols,
        std::span<int const> order) {

    assert(static_cast<size_t>(width) * static_cast<size_t>(height) == result.size());

    for (size_t order_index = 0; order_index < order.size(); ++order_index) {
        int const pixel_index = order[order_index];
        int const row = rows[static_cast<size_t>(pixel_index)];
        int const col = cols[static_cast<size_t>(pixel_index)];

        int const neighborhood_index =
                computeNeighborhoodIndex(result, width, height, col, row);
        result[static_cast<size_t>(row) * static_cast<size_t>(width) + static_cast<size_t>(col)] =
                kMedialAxisKeepTable[static_cast<size_t>(neighborhood_index)];
    }
}

std::vector<uint8_t> normalizeBinaryImage(
        std::vector<uint8_t> const & image,
        size_t height,
        size_t width) {

    std::vector<uint8_t> normalized(height * width);
    for (size_t i = 0; i < image.size(); ++i) {
        normalized[i] = image[i] > 0 ? 1 : 0;
    }
    return normalized;
}

std::vector<uint8_t> medialAxisSkeletonizeNormalized(
        std::vector<uint8_t> const & binary_image,
        int width,
        int height) {

    if (width <= 0 || height <= 0 || binary_image.empty()) {
        return {};
    }

    if (width < 3 || height < 3) {
        return binary_image;
    }

    std::vector<float> const distance = binaryForegroundDistanceTransformEuclidean(
            binary_image,
            width,
            height,
            1);

    std::vector<ForegroundPixel> foreground_pixels;
    foreground_pixels.reserve(binary_image.size());

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            size_t const index =
                    static_cast<size_t>(row) * static_cast<size_t>(width) + static_cast<size_t>(col);
            if (binary_image[index] == 0) {
                continue;
            }

            int const neighborhood_index =
                    computeNeighborhoodIndex(binary_image, width, height, col, row);

            foreground_pixels.push_back(ForegroundPixel{
                    .row = row,
                    .col = col,
                    .distance = distance[index],
                    .cornerness = kCornernessTable[static_cast<size_t>(neighborhood_index)],
            });
        }
    }

    if (foreground_pixels.empty()) {
        return std::vector<uint8_t>(binary_image.size(), 0);
    }

    std::vector<int> order(foreground_pixels.size());
    std::iota(order.begin(), order.end(), 0);

    std::vector<int> tiebreakers(foreground_pixels.size());
    std::iota(tiebreakers.begin(), tiebreakers.end(), 0);
    std::mt19937 rng(0);
    std::shuffle(tiebreakers.begin(), tiebreakers.end(), rng);

    std::sort(order.begin(), order.end(), [&](int lhs, int rhs) {
        ForegroundPixel const & a = foreground_pixels[static_cast<size_t>(lhs)];
        ForegroundPixel const & b = foreground_pixels[static_cast<size_t>(rhs)];
        if (a.distance != b.distance) {
            return a.distance < b.distance;
        }
        if (a.cornerness != b.cornerness) {
            return a.cornerness < b.cornerness;
        }
        return tiebreakers[static_cast<size_t>(lhs)] < tiebreakers[static_cast<size_t>(rhs)];
    });

    std::vector<int> rows(foreground_pixels.size());
    std::vector<int> cols(foreground_pixels.size());
    for (size_t i = 0; i < foreground_pixels.size(); ++i) {
        rows[i] = foreground_pixels[i].row;
        cols[i] = foreground_pixels[i].col;
    }

    std::vector<uint8_t> result = binary_image;
    medialAxisSkeletonizeLoop(result, width, height, rows, cols, order);
    return result;
}

}// namespace

std::vector<uint8_t> medial_axis_skeletonize(
        std::vector<uint8_t> const & image,
        size_t height,
        size_t width) {

    assert(image.size() == height * width &&
           "medial_axis_skeletonize: image buffer size mismatch");
    assert(height > 0 && width > 0 && "medial_axis_skeletonize: dimensions must be positive");

    std::vector<uint8_t> const normalized = normalizeBinaryImage(image, height, width);
    return medialAxisSkeletonizeNormalized(
            normalized,
            static_cast<int>(width),
            static_cast<int>(height));
}

Image medial_axis_skeletonize(Image const & image) {
    auto const result_data = medial_axis_skeletonize(
            image.data,
            static_cast<size_t>(image.size.height),
            static_cast<size_t>(image.size.width));

    return {std::move(result_data), image.size};
}
