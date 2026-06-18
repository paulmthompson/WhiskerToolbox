/**
 * @file medial_axis_skeletonize.test.cpp
 * @brief Unit tests for medial-axis skeletonization.
 */

#include "medial_axis_skeletonize.hpp"
#include "skeletonize.hpp"

#include "CoreGeometry/Image.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <vector>

namespace {

std::vector<uint8_t> horizontalBar(size_t height, size_t width, size_t bar_height) {
    std::vector<uint8_t> image(height * width, 0);
    size_t const row_start = (height - bar_height) / 2;
    for (size_t row = row_start; row < row_start + bar_height; ++row) {
        std::fill(image.begin() + row * width, image.begin() + (row + 1) * width, 1);
    }
    return image;
}

size_t countForeground(std::vector<uint8_t> const & image) {
    return static_cast<size_t>(std::accumulate(image.begin(), image.end(), 0));
}

float meanSkeletonRow(std::vector<uint8_t> const & skeleton, size_t width, size_t height) {
    float sum = 0.0F;
    size_t count = 0;
    for (size_t row = 0; row < height; ++row) {
        for (size_t col = 0; col < width; ++col) {
            if (skeleton[row * width + col] > 0) {
                sum += static_cast<float>(row);
                ++count;
            }
        }
    }
    return count > 0 ? sum / static_cast<float>(count) : -1.0F;
}

}// namespace

TEST_CASE("medial_axis_skeletonize - horizontal bar centers on middle row",
          "[medial_axis][skeletonize][masks]") {

    size_t const height = 9;
    size_t const width = 25;
    size_t const bar_height = 5;
    auto const image = horizontalBar(height, width, bar_height);

    auto const zhang = fast_skeletonize(image, height, width);
    auto const medial = medial_axis_skeletonize(image, height, width);

    float const zhang_mean = meanSkeletonRow(zhang, width, height);
    float const medial_mean = meanSkeletonRow(medial, width, height);
    float const geometric_center = static_cast<float>(height) / 2.0F;

    REQUIRE(countForeground(medial) > 0);
    REQUIRE(std::abs(medial_mean - geometric_center) <= std::abs(zhang_mean - geometric_center));
}

TEST_CASE("medial_axis_skeletonize - staggered bars prefer geometric center over top edge",
          "[medial_axis][skeletonize][masks]") {

    size_t const height = 20;
    size_t const width = 80;
    std::vector<uint8_t> image(height * width, 0);

    auto fillBar = [&](size_t x0, size_t y0, size_t w, size_t h) {
        for (size_t y = y0; y < y0 + h; ++y) {
            for (size_t x = x0; x < x0 + w; ++x) {
                image[y * width + x] = 1;
            }
        }
    };

    size_t const bar_height = 5;
    fillBar(5, 5, 20, bar_height);
    fillBar(20, 9, 20, bar_height);
    fillBar(35, 13, 20, bar_height);

    auto const zhang = fast_skeletonize(image, height, width);
    auto const medial = medial_axis_skeletonize(image, height, width);

    float const zhang_mean = meanSkeletonRow(zhang, width, height);
    float const medial_mean = meanSkeletonRow(medial, width, height);

    REQUIRE(countForeground(zhang) > 0);
    REQUIRE(countForeground(medial) > 0);
    // Medial axis is centered on the bars; Zhang–Suen is slightly top-biased.
    REQUIRE(medial_mean <= zhang_mean);
}

TEST_CASE("medial_axis_skeletonize - matches scikit-image 5x5 padded square pattern",
          "[medial_axis][skeletonize][masks][scikit_image]") {

    size_t const height = 5;
    size_t const width = 5;
    std::vector<uint8_t> image(height * width, 0);
    for (size_t row = 1; row <= 3; ++row) {
        for (size_t col = 1; col <= 3; ++col) {
            image[row * width + col] = 1;
        }
    }

    auto const skeleton = medial_axis_skeletonize(image, height, width);

    auto at = [&](size_t row, size_t col) { return skeleton[row * width + col] > 0; };

    // Reference: skimage.medial_axis(..., rng=0) on the same 5x5 canvas
    REQUIRE(at(1, 1));
    REQUIRE_FALSE(at(1, 2));
    REQUIRE(at(1, 3));
    REQUIRE_FALSE(at(2, 1));
    REQUIRE(at(2, 2));
    REQUIRE_FALSE(at(2, 3));
    REQUIRE(at(3, 1));
    REQUIRE_FALSE(at(3, 2));
    REQUIRE(at(3, 3));
    REQUIRE(countForeground(skeleton) == 5);
}

TEST_CASE("medial_axis_skeletonize - Image wrapper preserves dimensions",
          "[medial_axis][skeletonize][masks]") {

    Image input{{1, 1, 1, 1, 1, 1, 1, 1, 1}, {3, 3}};
    Image const output = medial_axis_skeletonize(input);

    REQUIRE(output.size.width == 3);
    REQUIRE(output.size.height == 3);
    REQUIRE(output.data.size() == 9);
}
