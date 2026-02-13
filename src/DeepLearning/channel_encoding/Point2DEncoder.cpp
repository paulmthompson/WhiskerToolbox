#include "Point2DEncoder.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dl {

std::string Point2DEncoder::name() const
{
    return "Point2DEncoder";
}

std::string Point2DEncoder::inputTypeName() const
{
    return "Point2D<float>";
}

namespace {

/// Scale a point from source image coordinates to tensor coordinates
Point2D<float> scale_point(Point2D<float> const point,
                           ImageSize const source_size,
                           int const target_h,
                           int const target_w)
{
    float const sx = static_cast<float>(target_w) / static_cast<float>(source_size.width);
    float const sy = static_cast<float>(target_h) / static_cast<float>(source_size.height);
    return {point.x * sx, point.y * sy};
}

/// Place a single point with Binary mode (set 1.0 at nearest pixel)
void encode_binary(Point2D<float> const scaled_point,
                   torch::Tensor & channel,
                   int const h,
                   int const w)
{
    int const px = std::clamp(static_cast<int>(std::round(scaled_point.x)), 0, w - 1);
    int const py = std::clamp(static_cast<int>(std::round(scaled_point.y)), 0, h - 1);
    channel[py][px] = 1.0f;
}

/// Place a 2D Gaussian centered on a point (Heatmap mode)
void encode_heatmap(Point2D<float> const scaled_point,
                    torch::Tensor & channel,
                    int const h,
                    int const w,
                    float const sigma)
{
    // Compute extent: 3*sigma is enough to capture >99% of the Gaussian
    float const extent = 3.0f * sigma;
    int const y_min = std::max(0, static_cast<int>(std::floor(scaled_point.y - extent)));
    int const y_max = std::min(h - 1, static_cast<int>(std::ceil(scaled_point.y + extent)));
    int const x_min = std::max(0, static_cast<int>(std::floor(scaled_point.x - extent)));
    int const x_max = std::min(w - 1, static_cast<int>(std::ceil(scaled_point.x + extent)));

    float const inv_2sigma2 = 1.0f / (2.0f * sigma * sigma);
    auto accessor = channel.accessor<float, 2>();

    for (int y = y_min; y <= y_max; ++y) {
        for (int x = x_min; x <= x_max; ++x) {
            float const dx = static_cast<float>(x) - scaled_point.x;
            float const dy = static_cast<float>(y) - scaled_point.y;
            float const val = std::exp(-(dx * dx + dy * dy) * inv_2sigma2);
            // Use max to handle overlapping points
            accessor[y][x] = std::max(accessor[y][x], val);
        }
    }
}

} // anonymous namespace

void Point2DEncoder::encode(Point2D<float> const point,
                            ImageSize const source_size,
                            torch::Tensor & tensor,
                            EncoderParams const & params) const
{
    if (params.mode != RasterMode::Binary && params.mode != RasterMode::Heatmap) {
        throw std::invalid_argument("Point2DEncoder: only Binary and Heatmap modes are supported");
    }

    auto channel = tensor[params.batch_index][params.target_channel];
    auto const scaled = scale_point(point, source_size, params.height, params.width);

    if (params.mode == RasterMode::Binary) {
        encode_binary(scaled, channel, params.height, params.width);
    } else {
        encode_heatmap(scaled, channel, params.height, params.width, params.gaussian_sigma);
    }
}

void Point2DEncoder::encode(std::vector<Point2D<float>> const & points,
                            ImageSize const source_size,
                            torch::Tensor & tensor,
                            EncoderParams const & params) const
{
    if (params.mode != RasterMode::Binary && params.mode != RasterMode::Heatmap) {
        throw std::invalid_argument("Point2DEncoder: only Binary and Heatmap modes are supported");
    }

    auto channel = tensor[params.batch_index][params.target_channel];

    for (auto const & pt : points) {
        auto const scaled = scale_point(pt, source_size, params.height, params.width);

        if (params.mode == RasterMode::Binary) {
            encode_binary(scaled, channel, params.height, params.width);
        } else {
            encode_heatmap(scaled, channel, params.height, params.width, params.gaussian_sigma);
        }
    }
}

} // namespace dl
