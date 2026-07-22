#ifndef NEURALYZER_TENSOR_TO_POINT2D_HPP
#define NEURALYZER_TENSOR_TO_POINT2D_HPP

/**
 * @file TensorToPoint2D.hpp
 * @brief Decoder that extracts point coordinates from a spatial tensor channel.
 */

#include "ChannelDecoder.hpp"

#include "CoreGeometry/points.hpp"

#include <vector>

namespace at {
class Tensor;
}

namespace dl {

/**
 * @brief Decodes a tensor channel into Point2D<float> by finding peak activations.
 *
 * Strategies:
 *   - Argmax: finds the pixel with highest value
 *   - Subpixel: refines argmax with parabolic fit around the maximum
 *
 * Output coordinates are scaled from tensor (H, W) back to target_image_size.
 * If target_image_size is {0, 0}, coordinates are returned in tensor space.
 *
 * Expects a 4D input tensor laid out as [B, C, H, W].
 */
class TensorToPoint2D : public ChannelDecoder {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string outputTypeName() const override;

    /**
     * @brief Decode the channel with the highest activation into a single point.
     *
     * @param tensor 4D activation tensor [B, C, H, W].
     * @param ctx Decoder indexing context; height and width must match tensor dims 2 and 3.
     * @param params Subpixel refinement and threshold settings.
     * @return Peak location, or {0, 0} if the channel is entirely non-positive.
     *
     * @pre tensor is defined
     * @pre tensor.dim() == 4 (layout [B, C, H, W])
     * @pre 0 <= ctx.batch_index < tensor.size(0)
     * @pre 0 <= ctx.source_channel < tensor.size(1)
     * @pre ctx.height > 0 and ctx.width > 0
     * @pre tensor.size(2) == ctx.height and tensor.size(3) == ctx.width
     */
    [[nodiscard]] static Point2D<float> decode(at::Tensor const & tensor,
                                               DecoderContext const & ctx,
                                               PointDecoderParams const & params);

    /**
     * @brief Decode all local maxima above threshold into multiple points.
     *
     * A local maximum is a pixel whose value is greater than all 8 neighbors.
     *
     * @param tensor 4D activation tensor [B, C, H, W].
     * @param ctx Decoder indexing context; height and width must match tensor dims 2 and 3.
     * @param params Subpixel refinement and minimum activation threshold.
     *
     * @pre tensor is defined
     * @pre tensor.dim() == 4 (layout [B, C, H, W])
     * @pre 0 <= ctx.batch_index < tensor.size(0)
     * @pre 0 <= ctx.source_channel < tensor.size(1)
     * @pre ctx.height > 0 and ctx.width > 0
     * @pre tensor.size(2) == ctx.height and tensor.size(3) == ctx.width
     */
    [[nodiscard]] static std::vector<Point2D<float>> decodeMultiple(
            at::Tensor const & tensor,
            DecoderContext const & ctx,
            PointDecoderParams const & params);
};

}// namespace dl

#endif// NEURALYZER_TENSOR_TO_POINT2D_HPP
