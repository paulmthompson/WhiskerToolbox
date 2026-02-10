#include "Mask2DEncoder.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dl {

std::string Mask2DEncoder::name() const
{
    return "Mask2DEncoder";
}

std::string Mask2DEncoder::inputTypeName() const
{
    return "Mask2D";
}

void Mask2DEncoder::encode(Mask2D const & mask,
                           ImageSize const source_size,
                           torch::Tensor & tensor,
                           EncoderParams const & params) const
{
    if (params.mode != RasterMode::Binary) {
        throw std::invalid_argument("Mask2DEncoder: only Binary mode is supported");
    }

    if (mask.empty()) {
        return; // nothing to encode
    }

    auto channel = tensor[params.batch_index][params.target_channel];

    float const sx = static_cast<float>(params.width) / static_cast<float>(source_size.width);
    float const sy = static_cast<float>(params.height) / static_cast<float>(source_size.height);

    auto accessor = channel.accessor<float, 2>();

    for (auto const & point : mask) {
        int const px = std::clamp(
            static_cast<int>(std::round(static_cast<float>(point.x) * sx)),
            0, params.width - 1);
        int const py = std::clamp(
            static_cast<int>(std::round(static_cast<float>(point.y) * sy)),
            0, params.height - 1);
        accessor[py][px] = 1.0f;
    }
}

} // namespace dl
