#ifndef WHISKERTOOLBOX_LINE2D_ENCODER_HPP
#define WHISKERTOOLBOX_LINE2D_ENCODER_HPP

/** @file Line2DEncoder.hpp
 *  @brief Encoder for ordered polyline (Line2D) data into tensor channels.
 */

#include "ChannelEncoder.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"

namespace at {
class Tensor;
}

namespace dl {

/**
 * @brief Encodes a Line2D (ordered polyline) into a tensor channel.
 *
 * Supported modes:
 *   - Binary:  Rasterizes the polyline using Bresenham-style line drawing
 *   - Heatmap: Rasterizes the polyline with Gaussian blur along the line
 *
 * Line point coordinates are scaled from source ImageSize to (H, W).
 */
class Line2DEncoder : public ChannelEncoder {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string inputTypeName() const override;

    /**
     * @brief Encode a polyline into tensor[batch_index, target_channel, :, :]
     *
     * Lines with fewer than 2 points produce no output (no-op early return).
     * Point coordinates outside the source image bounds are clamped to the
     * tensor edges in Binary mode; in Heatmap mode they are handled naturally
     * by the bounding-box loop and clamping.
     *
     * @param line        Ordered polyline in source image coordinate space
     * @param source_size Spatial dimensions of the source image used to scale
     *                    line coordinates into tensor space
     * @param tensor      Pre-allocated [B, C, H, W] float32 tensor to write into
     * @param ctx         Encoder context (batch_index, target_channel, height, width)
     * @param params      User-configurable encoding parameters (mode, gaussian_sigma)
     *
     * @pre params.mode == Binary or params.mode == Heatmap (enforcement: exception)
     * @pre source_size.width > 0 and source_size.height > 0; zero causes floating-point
     *      division by zero in coordinate scaling producing +Inf scaled coordinates
     *      (enforcement: none) [CRITICAL]
     * @pre source_size.width > 0 and source_size.height > 0; negative values (e.g. the
     *      ImageSize default -1) invert the scale factor, producing garbage coordinates
     *      (enforcement: none) [IMPORTANT]
     * @pre tensor is a valid initialized [B, C, H, W] float32 tensor with at least
     *      4 dimensions; fewer dimensions cause an out-of-bounds tensor access when
     *      indexing tensor[batch_index][target_channel] (enforcement: none) [CRITICAL]
     * @pre ctx.batch_index >= 0 and ctx.batch_index < tensor.size(0); an out-of-range
     *      value causes an out-of-bounds tensor access (enforcement: none) [CRITICAL]
     * @pre ctx.target_channel >= 0 and ctx.target_channel < tensor.size(1); an
     *      out-of-range value causes an out-of-bounds tensor access (enforcement: none) [IMPORTANT]
     * @pre ctx.height == tensor.size(2) and ctx.width == tensor.size(3); if ctx.height
     *      or ctx.width exceeds the actual tensor spatial dimension the Bresenham clamp
     *      limit exceeds the accessor bound, causing an out-of-bounds accessor write
     *      (enforcement: none) [CRITICAL]
     * @pre ctx.height > 0 and ctx.width > 0; zero causes std::clamp to receive an
     *      upper bound of -1 which is less than the lower bound of 0, invoking
     *      undefined behaviour (enforcement: none) [IMPORTANT]
     * @pre params.gaussian_sigma > 0 when mode == Heatmap; zero sigma causes
     *      floating-point division by zero in the Gaussian weight computation,
     *      producing +Inf weights and NaN output for pixels exactly on the line
     *      (enforcement: none) [IMPORTANT]
     */
    static void encode(Line2D const & line,
                       ImageSize source_size,
                       at::Tensor & tensor,
                       EncoderContext const & ctx,
                       Line2DEncoderParams const & params);
};

}// namespace dl

#endif// WHISKERTOOLBOX_LINE2D_ENCODER_HPP
