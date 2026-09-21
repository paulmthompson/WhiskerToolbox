#include "Mask2DEncoder.hpp"

#include "spatial/SpatialResize.hpp"

#include <ATen/core/Tensor.h>// at::Tensor

#include <stdexcept>

namespace dl {

std::string Mask2DEncoder::name() const {
    return "Mask2DEncoder";
}

std::string Mask2DEncoder::inputTypeName() const {
    return "Mask2D";
}

void Mask2DEncoder::encode(Mask2D const & mask,
                           ImageSize const source_size,
                           at::Tensor & tensor,
                           EncoderContext const & ctx,
                           Mask2DEncoderParams const & params) {
    if (params.mode != RasterMode::Binary) {
        throw std::invalid_argument("Mask2DEncoder: only Binary mode is supported");
    }

    if (mask.empty()) {
        return;// nothing to encode
    }

    auto channel = tensor[ctx.batch_index][ctx.target_channel];

    auto accessor = channel.accessor<float, 2>();

    for (auto const & point: mask) {
        int const px = spatial::discrete_image_to_tensor(
                static_cast<int>(point.x), source_size.width, ctx.width);
        int const py = spatial::discrete_image_to_tensor(
                static_cast<int>(point.y), source_size.height, ctx.height);
        accessor[py][px] = 1.0f;
    }
}

}// namespace dl
