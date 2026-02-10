#ifndef WHISKERTOOLBOX_CHANNEL_DECODER_HPP
#define WHISKERTOOLBOX_CHANNEL_DECODER_HPP

#include "CoreGeometry/ImageSize.hpp"

#include <string>
#include <torch/torch.h>

namespace dl {

struct DecoderParams {
    int source_channel = 0;
    int batch_index = 0;       ///< Which batch index to read from
    int height = 256;
    int width = 256;
    float threshold = 0.5f;    ///< For mask binarization / line extraction
    bool subpixel = true;      ///< Subpixel localization for points
    ImageSize target_image_size{}; ///< Scale output back to original coords
};

class ChannelDecoder {
public:
    virtual ~ChannelDecoder() = default;
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual std::string outputTypeName() const = 0;
};

} // namespace dl

#endif // WHISKERTOOLBOX_CHANNEL_DECODER_HPP
