#ifndef WHISKERTOOLBOX_MASK2D_ENCODER_HPP
#define WHISKERTOOLBOX_MASK2D_ENCODER_HPP

/** @file Mask2DEncoder.hpp
 *  @brief Encoder for Mask2D (set of pixel coordinates) into tensor channels.
 */

#include "ChannelEncoder.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/masks.hpp"

namespace at {
class Tensor;
}

namespace dl {

/**
 * @brief Encodes a Mask2D (set of pixel coordinates) into a tensor channel.
 *
 * Supported modes:
 *   - Binary: Sets 1.0 at each mask pixel (scaled to tensor dimensions)
 *
 * Mask pixel coordinates are scaled from source ImageSize to (H, W).
 */
class Mask2DEncoder : public ChannelEncoder {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string inputTypeName() const override;

    /**
     * @brief Encode mask pixel set into tensor[batch_index, target_channel, :, :]
     *
     * An empty mask produces no output (no-op early return).
     * Mask point coordinates outside the source image bounds are clamped to
     * the tensor edges after scaling.
     *
     * @param mask        Set of pixel coordinates in source image coordinate space
     * @param source_size Spatial dimensions of the source image used to scale
     *                    mask coordinates into tensor space
     * @param tensor      Pre-allocated [B, C, H, W] float32 tensor to write into
     * @param ctx         Encoder context (batch_index, target_channel, height, width)
     * @param params      User-configurable encoding parameters (mode)
     *
     * @pre params.mode == Binary (enforcement: exception)
     * @pre source_size.width > 0 and source_size.height > 0; zero causes
     *      floating-point division by zero producing a +Inf scale factor, which
     *      makes static_cast<int> of the resulting Inf undefined behaviour
     *      (enforcement: none) [CRITICAL]
     * @pre source_size.width > 0 and source_size.height > 0; negative values
     *      (e.g. the ImageSize default -1) produce a negative scale factor,
     *      mapping all mask pixels to coordinate 0 instead of their true position
     *      (enforcement: none) [IMPORTANT]
     * @pre tensor is a valid initialized [B, C, H, W] float32 tensor with at least
     *      4 dimensions; fewer dimensions cause an out-of-bounds tensor access when
     *      indexing tensor[batch_index][target_channel] (enforcement: none) [CRITICAL]
     * @pre ctx.batch_index >= 0 and ctx.batch_index < tensor.size(0); an out-of-range
     *      value causes an out-of-bounds tensor access (enforcement: none) [CRITICAL]
     * @pre ctx.target_channel >= 0 and ctx.target_channel < tensor.size(1); an
     *      out-of-range value causes an out-of-bounds tensor access (enforcement: none) [IMPORTANT]
     * @pre ctx.height == tensor.size(2) and ctx.width == tensor.size(3); if either
     *      ctx dimension exceeds the actual tensor spatial dimension the accessor
     *      write is out-of-bounds (enforcement: none) [CRITICAL]
     * @pre ctx.height > 0 and ctx.width > 0; zero causes std::clamp to receive an
     *      upper bound of -1 which is less than the lower bound of 0, invoking
     *      undefined behaviour (enforcement: none) [IMPORTANT]
     */
    static void encode(Mask2D const & mask,
                       ImageSize source_size,
                       at::Tensor & tensor,
                       EncoderContext const & ctx,
                       Mask2DEncoderParams const & params);
};

}// namespace dl

#endif// WHISKERTOOLBOX_MASK2D_ENCODER_HPP
