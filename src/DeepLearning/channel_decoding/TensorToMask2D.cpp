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

    if (!needs_scaling) {
        // No scaling needed - iterate over source and output directly
        Mask2D mask;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (accessor[y][x] > params.threshold) {
                    mask.push_back(Point2D<uint32_t>{
                        static_cast<uint32_t>(x),
                        static_cast<uint32_t>(y)});
                }
            }
        }
        return mask;
    }

    // Upscaling with proper nearest-neighbor interpolation:
    // Iterate over destination pixels and find corresponding source pixels.
    // This fills the entire region without gaps.
    int const dest_w = params.target_image_size.width;
    int const dest_h = params.target_image_size.height;

    // Scale factors: dest -> source mapping
    float const x_scale = static_cast<float>(w) / static_cast<float>(dest_w);
    float const y_scale = static_cast<float>(h) / static_cast<float>(dest_h);

    Mask2D mask;
    for (int dest_y = 0; dest_y < dest_h; ++dest_y) {
        // Find source y coordinate using nearest-neighbor mapping
        int const src_y = std::clamp(
            static_cast<int>((static_cast<float>(dest_y) + 0.5f) * y_scale),
            0, h - 1);

        for (int dest_x = 0; dest_x < dest_w; ++dest_x) {
            // Find source x coordinate using nearest-neighbor mapping
            int const src_x = std::clamp(
                static_cast<int>((static_cast<float>(dest_x) + 0.5f) * x_scale),
                0, w - 1);

            if (accessor[src_y][src_x] > params.threshold) {
                mask.push_back(Point2D<uint32_t>{
                    static_cast<uint32_t>(dest_x),
                    static_cast<uint32_t>(dest_y)});
            }
        }
    }

    return mask;
}

} // namespace dl
