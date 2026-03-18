#ifndef WHISKERTOOLBOX_MASK2D_ENCODER_HPP
#define WHISKERTOOLBOX_MASK2D_ENCODER_HPP

#include "ChannelEncoder.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/masks.hpp"

namespace at {
class Tensor;
}

namespace dl {

/// Encodes a Mask2D (set of pixel coordinates) into a tensor channel.
///
/// Supported modes:
///   - Binary: Sets 1.0 at each mask pixel (scaled to tensor dimensions)
///
/// Mask pixel coordinates are scaled from source ImageSize to (H, W).
class Mask2DEncoder : public ChannelEncoder {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string inputTypeName() const override;

    /// Encode mask pixel set into tensor[batch_index, target_channel, :, :]
    static void encode(Mask2D const & mask,
                ImageSize source_size,
                at::Tensor & tensor,
                EncoderContext const & ctx,
                Mask2DEncoderParams const & params) ;
};

}// namespace dl

#endif// WHISKERTOOLBOX_MASK2D_ENCODER_HPP
