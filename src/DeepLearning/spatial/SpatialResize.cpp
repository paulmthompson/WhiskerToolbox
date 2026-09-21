/**
 * @file SpatialResize.cpp
 * @brief Implementation of pixel-center spatial mapping for DeepLearning.
 */

#include "SpatialResize.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace dl::spatial {

namespace {

[[nodiscard]] int clamp_index(int index, int size) {
    return std::clamp(index, 0, size - 1);
}

[[nodiscard]] double pixel_center_scale(int source_size, int dest_size) {
    return static_cast<double>(source_size) / static_cast<double>(dest_size);
}

}// namespace

float continuous_image_to_tensor(float image_coord, int image_size, int tensor_size) {
    assert(image_size > 0 && "continuous_image_to_tensor: image_size must be > 0");
    assert(tensor_size > 0 && "continuous_image_to_tensor: tensor_size must be > 0");

    return static_cast<float>(
            (static_cast<double>(image_coord) + 0.5) * pixel_center_scale(tensor_size, image_size) -
            0.5);
}

float continuous_tensor_to_image(float tensor_coord, int tensor_size, int image_size) {
    assert(tensor_size > 0 && "continuous_tensor_to_image: tensor_size must be > 0");
    assert(image_size > 0 && "continuous_tensor_to_image: image_size must be > 0");

    return static_cast<float>(
            (static_cast<double>(tensor_coord) + 0.5) * pixel_center_scale(image_size, tensor_size) -
            0.5);
}

int discrete_image_to_tensor(int image_coord, int image_size, int tensor_size) {
    assert(image_size > 0 && "discrete_image_to_tensor: image_size must be > 0");
    assert(tensor_size > 0 && "discrete_image_to_tensor: tensor_size must be > 0");

    if (tensor_size == 1) {
        return 0;
    }

    double const continuous = (static_cast<double>(image_coord) + 0.5) *
                                      pixel_center_scale(tensor_size, image_size) -
                              0.5;
    return clamp_index(static_cast<int>(std::lround(continuous)), tensor_size);
}

int discrete_dest_to_source(int dest_coord, int source_size, int dest_size) {
    assert(source_size > 0 && "discrete_dest_to_source: source_size must be > 0");
    assert(dest_size > 0 && "discrete_dest_to_source: dest_size must be > 0");

    if (source_size == 1) {
        return 0;
    }

    double const continuous =
            (static_cast<double>(dest_coord) + 0.5) * pixel_center_scale(source_size, dest_size) -
            0.5;
    return clamp_index(static_cast<int>(std::lround(continuous)), source_size);
}

int discrete_tensor_nearest(float tensor_coord, int tensor_size) {
    assert(tensor_size > 0 && "discrete_tensor_nearest: tensor_size must be > 0");

    if (tensor_size == 1) {
        return 0;
    }

    return clamp_index(static_cast<int>(std::lround(tensor_coord)), tensor_size);
}

}// namespace dl::spatial
