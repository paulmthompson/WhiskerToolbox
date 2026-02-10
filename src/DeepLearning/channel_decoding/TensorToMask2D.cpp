#include "TensorToMask2D.hpp"

#include <algorithm>
#include <cmath>

namespace dl {

std::string TensorToMask2D::name() const
{
    return "TensorToMask2D";
}

std::string TensorToMask2D::outputTypeName() const
{
    return "Mask2D";
}

Mask2D TensorToMask2D::decode(torch::Tensor const & tensor,
                              DecoderParams const & params) const
{
    auto channel = tensor[params.batch_index][params.source_channel];
    auto const h = params.height;
    auto const w = params.width;
    auto accessor = channel.accessor<float, 2>();

    bool const needs_scaling =
        params.target_image_size.width > 0 && params.target_image_size.height > 0;

    float const sx = needs_scaling
        ? static_cast<float>(params.target_image_size.width) / static_cast<float>(w)
        : 1.0f;
    float const sy = needs_scaling
        ? static_cast<float>(params.target_image_size.height) / static_cast<float>(h)
        : 1.0f;

    Mask2D mask;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (accessor[y][x] > params.threshold) {
                auto const px = static_cast<uint32_t>(
                    std::clamp(static_cast<int>(std::round(static_cast<float>(x) * sx)),
                               0,
                               needs_scaling ? params.target_image_size.width - 1 : w - 1));
                auto const py = static_cast<uint32_t>(
                    std::clamp(static_cast<int>(std::round(static_cast<float>(y) * sy)),
                               0,
                               needs_scaling ? params.target_image_size.height - 1 : h - 1));
                mask.push_back(Point2D<uint32_t>{px, py});
            }
        }
    }

    return mask;
}

} // namespace dl
