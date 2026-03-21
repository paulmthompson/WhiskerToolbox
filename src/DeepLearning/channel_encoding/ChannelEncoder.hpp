#ifndef WHISKERTOOLBOX_CHANNEL_ENCODER_HPP
#define WHISKERTOOLBOX_CHANNEL_ENCODER_HPP

/// @file ChannelEncoder.hpp
/// @brief Base class for channel encoders and per-encoder parameter structs.

#include <string>

namespace dl {

/// How a geometry primitive is rasterized onto a tensor channel.
enum class RasterMode {
    Binary,  ///< 1.0 at occupied pixels, 0.0 elsewhere
    Heatmap, ///< Gaussian blob (requires sigma parameter)
    Distance,///< Distance transform from geometry
    Raw      ///< Direct pixel copy (images)
};

/// Fields set by SlotAssembler from model metadata and runtime state.
/// Not user-configurable — describes how to index into the input tensor.
struct EncoderContext {
    int target_channel = 0;///< Which channel to write into
    int batch_index = 0;   ///< Which batch index to write into
    int height = 256;      ///< Tensor spatial height
    int width = 256;       ///< Tensor spatial width
};

/// User-configurable params for ImageEncoder.
struct ImageEncoderParams {
    bool normalize = true;///< Normalize output to [0, 1]
};

/// User-configurable params for Point2DEncoder.
struct Point2DEncoderParams {
    RasterMode mode = RasterMode::Binary;///< Binary or Heatmap
    float gaussian_sigma = 2.0f;         ///< Sigma for Gaussian heatmap (Heatmap mode only)
    bool normalize = true;               ///< Normalize output to [0, 1]
};

/// User-configurable params for Mask2DEncoder.
struct Mask2DEncoderParams {
    RasterMode mode = RasterMode::Binary;///< Binary only
    bool normalize = true;               ///< Normalize output to [0, 1]
};

/// User-configurable params for Line2DEncoder.
struct Line2DEncoderParams {
    RasterMode mode = RasterMode::Binary;///< Binary or Heatmap
    float gaussian_sigma = 2.0f;         ///< Sigma for Gaussian heatmap (Heatmap mode only)
    bool normalize = true;               ///< Normalize output to [0, 1]
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

}// namespace dl

#endif// WHISKERTOOLBOX_CHANNEL_ENCODER_HPP
