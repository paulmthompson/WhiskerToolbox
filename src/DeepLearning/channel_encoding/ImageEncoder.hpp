#ifndef WHISKERTOOLBOX_IMAGE_ENCODER_HPP
#define WHISKERTOOLBOX_IMAGE_ENCODER_HPP

/** @file ImageEncoder.hpp
 *  @brief Encoder for image pixel data (grayscale or RGB) into tensor channels.
 */

#include "ChannelEncoder.hpp"

#include "CoreGeometry/ImageSize.hpp"

#include <cstdint>
#include <vector>

namespace at {
class Tensor;
}

namespace dl {

/**
 * @brief Encodes image pixel data (grayscale or RGB) into one or more tensor channels.
 *
 * Supports both uint8 [0,255] and float [0,1] source data.
 * For 1-channel (grayscale) source with a 3-channel target, the single channel
 * is replicated across the 3 output channels.
 *
 * Only RasterMode::Raw is supported.
 */
class ImageEncoder : public ChannelEncoder {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string inputTypeName() const override;

    /**
     * @brief Encode 8-bit image data into tensor[batch_index, target_channel:target_channel+channels, :, :]
     *
     * @param image_data   Raw pixel bytes in row-major order (H*W for grayscale, H*W*3 for RGB)
     * @param source_size  Dimensions of the source image
     * @param num_channels Number of channels in source (1=grayscale, 3=RGB)
     * @param tensor       Pre-allocated [B, C, H, W] tensor to write into
     * @param ctx          Encoder context (target_channel, batch_index, height, width)
     * @param params       User-configurable encoding parameters (normalize)
     *
     * @pre num_channels == 1 or num_channels == 3 (enforcement: exception)
     * @pre image_data.size() == source_size.height * source_size.width * num_channels
     *      (enforcement: exception)
     * @pre source_size.height > 0 and source_size.width > 0; zero dimensions produce an
     *      empty tensor that causes libtorch interpolation to fail (enforcement: none) [IMPORTANT]
     * @pre tensor is a valid initialized [B, C, H, W] tensor with at least 4 dimensions;
     *      missing dimensions cause an out-of-bounds tensor access (enforcement: none) [CRITICAL]
     * @pre ctx.batch_index >= 0 and ctx.batch_index < tensor.size(0); a negative or
     *      out-of-range batch index causes an out-of-bounds tensor access (enforcement: none) [CRITICAL]
     * @pre ctx.target_channel >= 0; a negative value causes an out-of-bounds channel
     *      access inside the tensor write loop (enforcement: none) [IMPORTANT]
     * @pre ctx.height > 0 and ctx.width > 0; zero spatial target dimensions cause
     *      libtorch interpolation to fail (enforcement: none) [IMPORTANT]
     * @pre tensor.size(2) == ctx.height and tensor.size(3) == ctx.width; a spatial
     *      dimension mismatch causes a libtorch shape error on channel assignment
     *      (enforcement: none) [IMPORTANT]
     */
    static void encode(std::vector<uint8_t> const & image_data,
                       ImageSize source_size,
                       int num_channels,
                       at::Tensor & tensor,
                       EncoderContext const & ctx,
                       ImageEncoderParams const & params);

    /**
     * @brief Encode 32-bit float image data into tensor channels.
     *
     * Float data is assumed to already be in [0,1] range if normalize=false.
     * When normalize=true, divides by the maximum pixel value only if that
     * maximum exceeds 1.0; if the maximum is exactly 0.0 the division is
     * skipped to avoid division by zero.
     *
     * @param image_data   Raw float pixel values in row-major order (H*W for grayscale, H*W*3 for RGB)
     * @param source_size  Dimensions of the source image
     * @param num_channels Number of channels in source (1=grayscale, 3=RGB)
     * @param tensor       Pre-allocated [B, C, H, W] tensor to write into
     * @param ctx          Encoder context (target_channel, batch_index, height, width)
     * @param params       User-configurable encoding parameters (normalize)
     *
     * @pre num_channels == 1 or num_channels == 3 (enforcement: exception)
     * @pre image_data.size() == source_size.height * source_size.width * num_channels
     *      (enforcement: exception)
     * @pre source_size.height > 0 and source_size.width > 0; zero dimensions produce an
     *      empty tensor that causes libtorch interpolation to fail (enforcement: none) [IMPORTANT]
     * @pre tensor is a valid initialized [B, C, H, W] tensor with at least 4 dimensions;
     *      missing dimensions cause an out-of-bounds tensor access (enforcement: none) [CRITICAL]
     * @pre ctx.batch_index >= 0 and ctx.batch_index < tensor.size(0); a negative or
     *      out-of-range batch index causes an out-of-bounds tensor access (enforcement: none) [CRITICAL]
     * @pre ctx.target_channel >= 0; a negative value causes an out-of-bounds channel
     *      access inside the tensor write loop (enforcement: none) [IMPORTANT]
     * @pre ctx.height > 0 and ctx.width > 0; zero spatial target dimensions cause
     *      libtorch interpolation to fail (enforcement: none) [IMPORTANT]
     * @pre tensor.size(2) == ctx.height and tensor.size(3) == ctx.width; a spatial
     *      dimension mismatch causes a libtorch shape error on channel assignment
     *      (enforcement: none) [IMPORTANT]
     * @pre image_data values are finite (not NaN or Inf); NaN causes src_tensor.max()
     *      to return NaN, silently skipping normalization and propagating NaN into the
     *      output tensor (enforcement: none) [IMPORTANT]
     */
    static void encode(std::vector<float> const & image_data,
                       ImageSize source_size,
                       int num_channels,
                       at::Tensor & tensor,
                       EncoderContext const & ctx,
                       ImageEncoderParams const & params);
};

}// namespace dl

#endif// WHISKERTOOLBOX_IMAGE_ENCODER_HPP
