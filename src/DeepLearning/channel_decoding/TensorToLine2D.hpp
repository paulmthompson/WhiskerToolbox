#ifndef WHISKERTOOLBOX_TENSOR_TO_LINE2D_HPP
#define WHISKERTOOLBOX_TENSOR_TO_LINE2D_HPP

/**
 * @file TensorToLine2D.hpp
 * @brief Decoder that extracts an ordered polyline from a spatial tensor channel.
 */

#include "ChannelDecoder.hpp"

#include "CoreGeometry/lines.hpp"

namespace at {
class Tensor;
}

namespace dl {

/**
 * @brief Decodes a tensor channel into a Line2D by thresholding, skeletonization, and ordering.
 *
 * Strategy:
 *   1. Threshold the channel to produce a binary mask
 *   2. Skeletonize (thin to 1-pixel width) using Zhang-Suen thinning
 *   3. Order skeleton pixels into a connected polyline by tracing connectivity
 *
 * Output coordinates are scaled from tensor (H, W) back to target_image_size.
 * If target_image_size is {0, 0}, coordinates are returned in tensor space.
 *
 * Expects a 4D input tensor laid out as [B, C, H, W].
 */
class TensorToLine2D : public ChannelDecoder {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string outputTypeName() const override;

    /**
     * @brief Decode a tensor channel to an ordered polyline.
     *
     * @param tensor 4D activation tensor [B, C, H, W].
     * @param ctx Decoder indexing context; height and width must match tensor dims 2 and 3.
     * @param params User threshold for line extraction.
     *
     * @pre tensor is defined
     * @pre tensor.dim() == 4 (layout [B, C, H, W])
     * @pre 0 <= ctx.batch_index < tensor.size(0)
     * @pre 0 <= ctx.source_channel < tensor.size(1)
     * @pre ctx.height > 0 and ctx.width > 0
     * @pre tensor.size(2) == ctx.height and tensor.size(3) == ctx.width
     *
     * @post Returned line is empty when no pixels exceed params.threshold.
     */
    [[nodiscard]] static Line2D decode(at::Tensor const & tensor,
                                       DecoderContext const & ctx,
                                       LineDecoderParams const & params);
};

}// namespace dl

#endif// WHISKERTOOLBOX_TENSOR_TO_LINE2D_HPP
