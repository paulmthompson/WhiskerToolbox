#ifndef WHISKERTOOLBOX_TENSOR_TO_MASK2D_HPP
#define WHISKERTOOLBOX_TENSOR_TO_MASK2D_HPP

#include "ChannelDecoder.hpp"

#include "CoreGeometry/masks.hpp"

namespace dl {

/// Decodes a tensor channel into a Mask2D by thresholding.
///
/// All pixels with activation above `threshold` are collected as mask pixels.
/// Output pixel coordinates are scaled from tensor (H, W) back to target_image_size.
/// If target_image_size is {0, 0}, coordinates are returned in tensor space.
class TensorToMask2D : public ChannelDecoder {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string outputTypeName() const override;

    /// Decode tensor channel to mask by thresholding
    [[nodiscard]] Mask2D decode(torch::Tensor const & tensor,
                                DecoderParams const & params) const;
};

} // namespace dl

#endif // WHISKERTOOLBOX_TENSOR_TO_MASK2D_HPP
