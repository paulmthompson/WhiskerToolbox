#ifndef WHISKERTOOLBOX_TENSOR_TO_POINT2D_HPP
#define WHISKERTOOLBOX_TENSOR_TO_POINT2D_HPP

#include "ChannelDecoder.hpp"

#include "CoreGeometry/points.hpp"

#include <vector>

namespace dl {

/// Decodes a tensor channel into Point2D<float> by finding the maximum activation.
///
/// Strategies:
///   - Argmax: finds the pixel with highest value
///   - Subpixel: refines argmax with parabolic fit around the maximum
///
/// Output coordinates are scaled from tensor (H, W) back to target_image_size.
/// If target_image_size is {0, 0}, coordinates are returned in tensor space.
class TensorToPoint2D : public ChannelDecoder {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string outputTypeName() const override;

    /// Decode the channel with the highest activation into a single point.
    /// Returns {0, 0} if the tensor channel is entirely zero.
    [[nodiscard]] Point2D<float> decode(torch::Tensor const & tensor,
                                        DecoderParams const & params) const;

    /// Decode all local maxima above threshold into multiple points.
    /// A local maximum is a pixel whose value is greater than all 8 neighbors.
    [[nodiscard]] std::vector<Point2D<float>> decodeMultiple(
        torch::Tensor const & tensor,
        DecoderParams const & params) const;
};

} // namespace dl

#endif // WHISKERTOOLBOX_TENSOR_TO_POINT2D_HPP
