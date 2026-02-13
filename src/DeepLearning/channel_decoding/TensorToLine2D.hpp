#ifndef WHISKERTOOLBOX_TENSOR_TO_LINE2D_HPP
#define WHISKERTOOLBOX_TENSOR_TO_LINE2D_HPP

#include "ChannelDecoder.hpp"

#include "CoreGeometry/lines.hpp"

namespace dl {

/// Decodes a tensor channel into a Line2D by thresholding, skeletonization, and ordering.
///
/// Strategy:
///   1. Threshold the channel to produce a binary mask
///   2. Skeletonize (thin to 1-pixel width) using Zhang-Suen thinning
///   3. Order skeleton pixels into a connected polyline by tracing connectivity
///
/// Output coordinates are scaled from tensor (H, W) back to target_image_size.
/// If target_image_size is {0, 0}, coordinates are returned in tensor space.
class TensorToLine2D : public ChannelDecoder {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string outputTypeName() const override;

    /// Decode tensor channel to ordered polyline
    [[nodiscard]] Line2D decode(torch::Tensor const & tensor,
                                DecoderParams const & params) const;
};

} // namespace dl

#endif // WHISKERTOOLBOX_TENSOR_TO_LINE2D_HPP
