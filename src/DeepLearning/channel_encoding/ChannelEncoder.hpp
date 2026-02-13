#ifndef WHISKERTOOLBOX_CHANNEL_ENCODER_HPP
#define WHISKERTOOLBOX_CHANNEL_ENCODER_HPP

#include <string>

#include <torch/torch.h>

namespace dl {

/// How a geometry primitive is rasterized onto a tensor channel
enum class RasterMode {
    Binary,   ///< 1.0 at occupied pixels, 0.0 elsewhere
    Heatmap,  ///< Gaussian blob (requires sigma parameter)
    Distance, ///< Distance transform from geometry
    Raw       ///< Direct pixel copy (images)
};

/// Parameters controlling how data is encoded into a tensor channel
struct EncoderParams {
    int target_channel = 0;            ///< which channel in the output tensor
    int batch_index = 0;               ///< which batch index to write into
    int height = 256;                  ///< spatial H of the tensor
    int width = 256;                   ///< spatial W of the tensor
    RasterMode mode = RasterMode::Binary;
    float gaussian_sigma = 2.0f;       ///< only used when mode == Heatmap
    bool normalize = true;             ///< normalize output to [0, 1]
};

/// Abstract base class for encoding geometry/image data into a tensor channel.
///
/// Each encoder writes into a specified channel index of a pre-allocated
/// B x C x H x W float tensor. The caller controls B and which batch_index
/// to write into.
class ChannelEncoder {
public:
    virtual ~ChannelEncoder() = default;

    /// Human-readable name for UI / registry
    [[nodiscard]] virtual std::string name() const = 0;

    /// Which geometry type this encoder expects (e.g. "Point2D<float>", "Mask2D")
    [[nodiscard]] virtual std::string inputTypeName() const = 0;
};

} // namespace dl

#endif // WHISKERTOOLBOX_CHANNEL_ENCODER_HPP
