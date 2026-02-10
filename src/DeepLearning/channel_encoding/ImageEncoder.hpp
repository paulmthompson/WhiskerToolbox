#ifndef WHISKERTOOLBOX_IMAGE_ENCODER_HPP
#define WHISKERTOOLBOX_IMAGE_ENCODER_HPP

#include "ChannelEncoder.hpp"

#include "CoreGeometry/ImageSize.hpp"

#include <cstdint>
#include <vector>

namespace dl {

/// Encodes image pixel data (grayscale or RGB) into one or more tensor channels.
///
/// Supports both uint8 [0,255] and float [0,1] source data.
/// For 1-channel (grayscale) source with a 3-channel target, the single channel
/// is replicated across the 3 output channels.
///
/// Only RasterMode::Raw is supported.
class ImageEncoder : public ChannelEncoder {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string inputTypeName() const override;

    /// Encode 8-bit image data into tensor[batch_index, target_channel:target_channel+channels, :, :]
    ///
    /// @param image_data  Raw pixel bytes in row-major order (H*W for grayscale, H*W*3 for RGB)
    /// @param source_size Dimensions of the source image
    /// @param num_channels Number of channels in source (1=grayscale, 3=RGB)
    /// @param tensor      Pre-allocated [B, C, H, W] tensor to write into
    /// @param params      Encoding parameters (target_channel, batch_index, height, width, normalize)
    void encode(std::vector<uint8_t> const & image_data,
                ImageSize source_size,
                int num_channels,
                torch::Tensor & tensor,
                EncoderParams const & params) const;

    /// Encode 32-bit float image data into tensor channels.
    /// Float data is assumed to already be in [0,1] range if normalize=false.
    void encode(std::vector<float> const & image_data,
                ImageSize source_size,
                int num_channels,
                torch::Tensor & tensor,
                EncoderParams const & params) const;
};

} // namespace dl

#endif // WHISKERTOOLBOX_IMAGE_ENCODER_HPP
