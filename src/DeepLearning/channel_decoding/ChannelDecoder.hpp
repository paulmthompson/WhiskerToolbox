#ifndef WHISKERTOOLBOX_CHANNEL_DECODER_HPP
#define WHISKERTOOLBOX_CHANNEL_DECODER_HPP

/// @file ChannelDecoder.hpp
/// @brief Base class for channel decoders and per-decoder parameter structs.

#include "CoreGeometry/ImageSize.hpp"

#include <string>

namespace dl {

/// Fields set by SlotAssembler from model metadata and runtime state.
/// Not user-configurable — describes how to index into the output tensor.
struct DecoderContext {
    int source_channel = 0;       ///< Which channel to decode
    int batch_index = 0;          ///< Which batch index to read from
    int height = 256;             ///< Tensor spatial height
    int width = 256;              ///< Tensor spatial width
    ImageSize target_image_size{};///< Scale output back to original coords
};

/// User-configurable params for TensorToMask2D.
struct MaskDecoderParams {
    float threshold = 0.5f;///< Binary threshold for mask generation
};

/// User-configurable params for TensorToPoint2D.
struct PointDecoderParams {
    bool subpixel = true;  ///< Enable parabolic subpixel refinement
    float threshold = 0.5f;///< For decodeMultiple: minimum activation for local maxima
};

/// User-configurable params for TensorToLine2D.
struct LineDecoderParams {
    float threshold = 0.5f;///< Binary threshold for line extraction
};

/// TensorToFeatureVector has no user-configurable params.
struct FeatureVectorDecoderParams {};

class ChannelDecoder {
public:
    virtual ~ChannelDecoder() = default;
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual std::string outputTypeName() const = 0;
};

}// namespace dl

#endif// WHISKERTOOLBOX_CHANNEL_DECODER_HPP
