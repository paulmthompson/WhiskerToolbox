/**
 * @file distance_transform.test.cpp
 * @brief Unit tests for binary Euclidean distance transform.
 */

#include "distance_transform.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {

std::vector<uint8_t> filledRectangle(size_t height, size_t width) {
    return std::vector<uint8_t>(height * width, 1);
}

std::vector<uint8_t> singleForegroundPixel(size_t height, size_t width, size_t row, size_t col) {
    std::vector<uint8_t> image(height * width, 0);
    image[row * width + col] = 1;
    return image;
}

}// namespace

TEST_CASE("binaryDistanceTransformEuclidean - single foreground pixel",
          "[distance_transform][masks]") {

    size_t const height = 5;
    size_t const width = 7;
    auto const image = singleForegroundPixel(height, width, 2, 3);

    auto const distance = binaryDistanceTransformEuclidean(image, static_cast<int>(width), static_cast<int>(height));

    REQUIRE(distance.size() == height * width);
    REQUIRE_THAT(distance[2 * width + 3], WithinAbs(0.0F, 1.0e-4F));
    REQUIRE_THAT(distance[2 * width + 4], WithinAbs(1.0F, 1.0e-4F));
    REQUIRE_THAT(distance[1 * width + 3], WithinAbs(1.0F, 1.0e-4F));
    REQUIRE_THAT(distance[2 * width + 1], WithinAbs(2.0F, 1.0e-4F));
}

TEST_CASE("binaryDistanceTransformEuclidean - filled rectangle center is farthest from edge",
          "[distance_transform][masks]") {

    size_t const height = 7;
    size_t const width = 9;
    auto const image = filledRectangle(height, width);

    auto const distance = binaryDistanceTransformEuclidean(image, static_cast<int>(width), static_cast<int>(height));

    float const center = distance[(height / 2) * width + (width / 2)];
    float const corner = distance[0];

    REQUIRE(center > corner);
    REQUIRE_THAT(center, WithinAbs(3.0F, 0.25F));
}

TEST_CASE("distanceTransform1DSquared - three point line",
          "[distance_transform][masks]") {

    std::vector<float> const samples{0.0F, 0.0F, 100.0F};
    auto const transformed = distanceTransform1DSquared(samples);

    REQUIRE(transformed.size() == 3);
    REQUIRE_THAT(transformed[0], WithinAbs(0.0F, 1.0e-4F));
    REQUIRE_THAT(transformed[1], WithinAbs(0.0F, 1.0e-4F));
    REQUIRE_THAT(transformed[2], WithinAbs(0.0F, 1.0e-4F));
}

TEST_CASE("binaryForegroundDistanceTransformEuclidean - foreground distance to background",
          "[distance_transform][masks]") {

    size_t const height = 5;
    size_t const width = 5;
    std::vector<uint8_t> image(height * width, 0);
    for (size_t row = 1; row <= 3; ++row) {
        for (size_t col = 1; col <= 3; ++col) {
            image[row * width + col] = 1;
        }
    }

    auto const distance = binaryForegroundDistanceTransformEuclidean(
            image,
            static_cast<int>(width),
            static_cast<int>(height));

    REQUIRE_THAT(distance[2 * width + 2], WithinAbs(2.0F, 1.0e-4F));
    REQUIRE_THAT(distance[1 * width + 1], WithinAbs(1.0F, 1.0e-4F));
    REQUIRE_THAT(distance[0], WithinAbs(0.0F, 1.0e-4F));
}

TEST_CASE("binaryDistanceTransformEuclidean - empty background stays infinite",
          "[distance_transform][masks]") {

    std::vector<uint8_t> const image(4 * 4, 0);
    auto const distance = binaryDistanceTransformEuclidean(image, 4, 4);

    for (float const value: distance) {
        REQUIRE_FALSE(std::isfinite(value));
    }
}
