#ifndef WHISKERTOOLBOX_POINT2D_ENCODER_HPP
#define WHISKERTOOLBOX_POINT2D_ENCODER_HPP

#include "ChannelEncoder.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/points.hpp"

#include <vector>

namespace dl {

/// Encodes Point2D<float> data into a tensor channel.
///
/// Supported modes:
///   - Binary:  Sets 1.0 at the nearest pixel for each point
///   - Heatmap: Places a 2D Gaussian centered on each point
///
/// Points are scaled from source ImageSize to the tensor's (H, W).
class Point2DEncoder : public ChannelEncoder {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string inputTypeName() const override;

    /// Encode a single point
    void encode(Point2D<float> point,
                ImageSize source_size,
                torch::Tensor & tensor,
                EncoderParams const & params) const;

    /// Encode multiple points
    void encode(std::vector<Point2D<float>> const & points,
                ImageSize source_size,
                torch::Tensor & tensor,
                EncoderParams const & params) const;
};

} // namespace dl

#endif // WHISKERTOOLBOX_POINT2D_ENCODER_HPP
