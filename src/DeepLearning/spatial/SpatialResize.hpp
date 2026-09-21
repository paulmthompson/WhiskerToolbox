#ifndef NEURALYZER_SPATIAL_RESIZE_HPP
#define NEURALYZER_SPATIAL_RESIZE_HPP

/**
 * @file SpatialResize.hpp
 * @brief Pixel-center spatial coordinate mapping for DeepLearning encode/decode.
 *
 * @par Module boundary
 * This API is **internal to DeepLearning**. Mask raster resize for non-DL callers
 * remains in `mask_utils.hpp`; both use the same pixel-center convention.
 *
 * @par Convention
 * Integer pixel indices are treated as **pixel centers** at `(i + 0.5)` in
 * continuous space (`align_corners=false`, PyTorch / OpenCV sampling grid).
 *
 * @see EncoderFactory
 * @see DecoderFactory
 */

namespace dl::spatial {

/**
 * @brief Map a continuous image-space coordinate to tensor space.
 *
 * @param image_coord Coordinate along one axis in image space (may be fractional).
 * @param image_size Image extent along that axis (must be > 0).
 * @param tensor_size Tensor extent along that axis (must be > 0).
 * @return Continuous tensor coordinate.
 *
 * @pre image_size > 0 and tensor_size > 0
 */
[[nodiscard]] float continuous_image_to_tensor(
        float image_coord, int image_size, int tensor_size);

/**
 * @brief Map a continuous tensor-space coordinate to image space.
 *
 * @param tensor_coord Coordinate along one axis in tensor space (may be fractional).
 * @param tensor_size Tensor extent along that axis (must be > 0).
 * @param image_size Image extent along that axis (must be > 0).
 * @return Continuous image coordinate.
 *
 * @pre tensor_size > 0 and image_size > 0
 */
[[nodiscard]] float continuous_tensor_to_image(
        float tensor_coord, int tensor_size, int image_size);

/**
 * @brief Nearest tensor index for an integer image pixel (mask encode / downsample).
 *
 * @param image_coord Integer image pixel index along one axis.
 * @param image_size Image extent along that axis (must be > 0).
 * @param tensor_size Tensor extent along that axis (must be > 0).
 * @return Clamped tensor index in `[0, tensor_size - 1]`.
 *
 * @pre image_size > 0 and tensor_size > 0
 */
[[nodiscard]] int discrete_image_to_tensor(
        int image_coord, int image_size, int tensor_size);

/**
 * @brief Nearest tensor index sampled when upsampling to a destination pixel (mask decode).
 *
 * Equivalent to `map_dest_to_source` in `mask_utils.hpp` with `std::lround`.
 *
 * @param dest_coord Integer destination pixel index along one axis.
 * @param source_size Source (tensor) extent along that axis (must be > 0).
 * @param dest_size Destination (image) extent along that axis (must be > 0).
 * @return Clamped source index in `[0, source_size - 1]`.
 *
 * @pre source_size > 0 and dest_size > 0
 */
[[nodiscard]] int discrete_dest_to_source(
        int dest_coord, int source_size, int dest_size);

/**
 * @brief Round a continuous tensor coordinate to the nearest discrete tensor index.
 *
 * @param tensor_coord Continuous tensor coordinate along one axis.
 * @param tensor_size Tensor extent along that axis (must be > 0).
 * @return Clamped index in `[0, tensor_size - 1]`.
 *
 * @pre tensor_size > 0
 */
[[nodiscard]] int discrete_tensor_nearest(float tensor_coord, int tensor_size);

}// namespace dl::spatial

#endif// NEURALYZER_SPATIAL_RESIZE_HPP
