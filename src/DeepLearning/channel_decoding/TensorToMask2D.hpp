#ifndef WHISKERTOOLBOX_TENSOR_TO_MASK2D_HPP
#define WHISKERTOOLBOX_TENSOR_TO_MASK2D_HPP

/**
 * @file TensorToMask2D.hpp
 * @brief Decoder that thresholds a spatial tensor channel into a Mask2D.
 */

#include "ChannelDecoder.hpp"

#include "CoreGeometry/masks.hpp"

namespace at {
class Tensor;
}

namespace dl {

/**
 * @brief Decodes a tensor channel into a Mask2D by thresholding.
 *
 * All pixels with activation above @p params.threshold are collected as mask pixels.
 * Output pixel coordinates are scaled from tensor (H, W) back to target_image_size.
 * If target_image_size is {0, 0}, coordinates are returned in tensor space.
 *
 * Expects a 4D input tensor laid out as [B, C, H, W].
 */
class TensorToMask2D : public ChannelDecoder {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string outputTypeName() const override;

    /**
     * @brief Decode a tensor channel to a mask by thresholding.
     *
     * @param tensor 4D activation tensor [B, C, H, W].
     * @param ctx Decoder indexing context; height and width must match tensor dims 2 and 3.
     * @param params User threshold for mask generation.
     *
     * @pre tensor is defined
     * @pre tensor.dim() == 4 (layout [B, C, H, W])
     * @pre 0 <= ctx.batch_index < tensor.size(0)
     * @pre 0 <= ctx.source_channel < tensor.size(1)
     * @pre ctx.height > 0 and ctx.width > 0
     * @pre tensor.size(2) == ctx.height and tensor.size(3) == ctx.width
     *
     * @post Returned mask contains only pixels with activation > params.threshold.
     */
    [[nodiscard]] static Mask2D decode(at::Tensor const & tensor,
                                       DecoderContext const & ctx,
                                       MaskDecoderParams const & params);
};

}// namespace dl

#endif// WHISKERTOOLBOX_TENSOR_TO_MASK2D_HPP
