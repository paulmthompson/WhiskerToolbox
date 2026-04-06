#ifndef WHISKERTOOLBOX_POINT2D_ENCODER_HPP
#define WHISKERTOOLBOX_POINT2D_ENCODER_HPP

/** @file Point2DEncoder.hpp
 *  @brief Encoder for Point2D<float> data into tensor channels.
 */

#include "ChannelEncoder.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/points.hpp"

#include <vector>

namespace at {
class Tensor;
}

namespace dl {

/**
 * @brief Encodes Point2D<float> data into a tensor channel.
 *
 * Supported modes:
 *   - Binary:  Sets 1.0 at the nearest pixel for each point
 *   - Heatmap: Places a 2D Gaussian centered on each point
 *
 * Points are scaled from source ImageSize to the tensor's (H, W).
 */
class Point2DEncoder : public ChannelEncoder {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string inputTypeName() const override;

    /**
     * @brief Encode a single point into tensor[batch_index, target_channel, :, :]
     *
     * @param point       Point in source image coordinate space
     * @param source_size Spatial dimensions of the source image used to scale
     *                    the point into tensor space
     * @param tensor      Pre-allocated [B, C, H, W] float32 tensor to write into
     * @param ctx         Encoder context (batch_index, target_channel, height, width)
     * @param params      User-configurable encoding parameters (mode, gaussian_sigma)
     *
     * @pre params.mode == Binary or params.mode == Heatmap (enforcement: exception)
     * @pre source_size.width > 0 and source_size.height > 0; zero causes
     *      floating-point division by zero producing a +Inf scale factor, which
     *      makes static_cast<int> of the resulting Inf undefined behaviour
     *      (enforcement: none) [CRITICAL]
     * @pre source_size.width > 0 and source_size.height > 0; negative values
     *      (e.g. the ImageSize default -1) produce a negative scale factor,
     *      mapping points to mirrored coordinates (enforcement: none) [IMPORTANT]
     * @pre tensor is a valid initialized [B, C, H, W] float32 tensor with at least
     *      4 dimensions; fewer dimensions cause an out-of-bounds tensor access when
     *      indexing tensor[batch_index][target_channel] (enforcement: none) [CRITICAL]
     * @pre ctx.batch_index >= 0 and ctx.batch_index < tensor.size(0); an out-of-range
     *      value causes an out-of-bounds tensor access (enforcement: none) [CRITICAL]
     * @pre ctx.target_channel >= 0 and ctx.target_channel < tensor.size(1); an
     *      out-of-range value causes an out-of-bounds tensor access (enforcement: none) [IMPORTANT]
     * @pre ctx.height == tensor.size(2) and ctx.width == tensor.size(3); if either
     *      ctx dimension exceeds the actual tensor spatial dimension the channel
     *      write is out-of-bounds (enforcement: none) [CRITICAL]
     * @pre ctx.height > 0 and ctx.width > 0; zero causes std::clamp to receive an
     *      upper bound of -1 which is less than the lower bound of 0, invoking
     *      undefined behaviour (enforcement: none) [IMPORTANT]
     * @pre params.gaussian_sigma > 0 when mode == Heatmap; zero causes
     *      floating-point division by zero in the Gaussian weight computation,
     *      producing NaN at the centre pixel (enforcement: none) [IMPORTANT]
     * @pre point.x and point.y are finite; NaN coordinates cause round() to return
     *      an unspecified value and static_cast<int> of NaN is undefined behaviour
     *      in Binary mode; in Heatmap mode the bounding-box calculations also
     *      invoke UB (enforcement: none) [IMPORTANT]
     */
    static void encode(Point2D<float> point,
                       ImageSize source_size,
                       at::Tensor & tensor,
                       EncoderContext const & ctx,
                       Point2DEncoderParams const & params);

    /**
     * @brief Encode multiple points into tensor[batch_index, target_channel, :, :]
     *
     * An empty points vector is a safe no-op. When multiple points map to
     * the same pixel in Binary mode the pixel is simply set to 1.0; in
     * Heatmap mode the maximum Gaussian value across all points is retained.
     *
     * @param points      Points in source image coordinate space
     * @param source_size Spatial dimensions of the source image used to scale
     *                    points into tensor space
     * @param tensor      Pre-allocated [B, C, H, W] float32 tensor to write into
     * @param ctx         Encoder context (batch_index, target_channel, height, width)
     * @param params      User-configurable encoding parameters (mode, gaussian_sigma)
     *
     * @pre params.mode == Binary or params.mode == Heatmap (enforcement: exception)
     * @pre source_size.width > 0 and source_size.height > 0; zero causes
     *      floating-point division by zero producing a +Inf scale factor, which
     *      makes static_cast<int> of the resulting Inf undefined behaviour
     *      (enforcement: none) [CRITICAL]
     * @pre source_size.width > 0 and source_size.height > 0; negative values
     *      (e.g. the ImageSize default -1) produce a negative scale factor,
     *      mapping points to mirrored coordinates (enforcement: none) [IMPORTANT]
     * @pre tensor is a valid initialized [B, C, H, W] float32 tensor with at least
     *      4 dimensions; fewer dimensions cause an out-of-bounds tensor access when
     *      indexing tensor[batch_index][target_channel] (enforcement: none) [CRITICAL]
     * @pre ctx.batch_index >= 0 and ctx.batch_index < tensor.size(0); an out-of-range
     *      value causes an out-of-bounds tensor access (enforcement: none) [CRITICAL]
     * @pre ctx.target_channel >= 0 and ctx.target_channel < tensor.size(1); an
     *      out-of-range value causes an out-of-bounds tensor access (enforcement: none) [IMPORTANT]
     * @pre ctx.height == tensor.size(2) and ctx.width == tensor.size(3); if either
     *      ctx dimension exceeds the actual tensor spatial dimension the channel
     *      write is out-of-bounds (enforcement: none) [CRITICAL]
     * @pre ctx.height > 0 and ctx.width > 0; zero causes std::clamp to receive an
     *      upper bound of -1 which is less than the lower bound of 0, invoking
     *      undefined behaviour (enforcement: none) [IMPORTANT]
     * @pre params.gaussian_sigma > 0 when mode == Heatmap; zero causes
     *      floating-point division by zero in the Gaussian weight computation,
     *      producing NaN at centre pixels (enforcement: none) [IMPORTANT]
     * @pre each element of points has finite x and y; NaN coordinates invoke
     *      undefined behaviour in the pixel-coordinate computation for both
     *      Binary and Heatmap modes (enforcement: none) [IMPORTANT]
     */
    static void encode(std::vector<Point2D<float>> const & points,
                       ImageSize source_size,
                       at::Tensor & tensor,
                       EncoderContext const & ctx,
                       Point2DEncoderParams const & params);
};

}// namespace dl

#endif// WHISKERTOOLBOX_POINT2D_ENCODER_HPP
