#include "Mask2DEncoder.hpp"

#include "torch/torch.h"

#include <algorithm>
#include <cmath>
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

    float const sx = static_cast<float>(ctx.width) / static_cast<float>(source_size.width);
    float const sy = static_cast<float>(ctx.height) / static_cast<float>(source_size.height);

    auto accessor = channel.accessor<float, 2>();

    for (auto const & point: mask) {
        int const px = std::clamp(
                static_cast<int>(std::round(static_cast<float>(point.x) * sx)),
                0, ctx.width - 1);
        int const py = std::clamp(
                static_cast<int>(std::round(static_cast<float>(point.y) * sy)),
                0, ctx.height - 1);
        accessor[py][px] = 1.0f;
    }
}

}// namespace dl
