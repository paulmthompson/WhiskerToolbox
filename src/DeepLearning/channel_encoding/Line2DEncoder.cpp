#include "Line2DEncoder.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dl {

std::string Line2DEncoder::name() const
{
    return "Line2DEncoder";
}

std::string Line2DEncoder::inputTypeName() const
{
    return "Line2D";
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

/// Bresenham-style line rasterization between two pixel coordinates.
/// Sets all visited pixels to 1.0 in the channel.
void rasterize_segment_binary(int x0, int y0, int x1, int y1,
                              torch::TensorAccessor<float, 2> & accessor,
                              int const h, int const w)
{
    // Clamp endpoints
    x0 = std::clamp(x0, 0, w - 1);
    y0 = std::clamp(y0, 0, h - 1);
    x1 = std::clamp(x1, 0, w - 1);
    y1 = std::clamp(y1, 0, h - 1);

    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (true) {
        accessor[y0][x0] = 1.0f;
        if (x0 == x1 && y0 == y1) break;
        int const e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/// Rasterize a line segment with Gaussian spread.
/// For each pixel, compute the minimum distance to the line segment and
/// apply a Gaussian weight.
void rasterize_segment_heatmap(Point2D<float> const p0, Point2D<float> const p1,
                               torch::TensorAccessor<float, 2> & accessor,
                               int const h, int const w,
                               float const sigma)
{
    float const extent = 3.0f * sigma;
    float const inv_2sigma2 = 1.0f / (2.0f * sigma * sigma);

    // Compute bounding box of the segment + extent
    float const min_x = std::min(p0.x, p1.x) - extent;
    float const max_x = std::max(p0.x, p1.x) + extent;
    float const min_y = std::min(p0.y, p1.y) - extent;
    float const max_y = std::max(p0.y, p1.y) + extent;

    int const y_start = std::max(0, static_cast<int>(std::floor(min_y)));
    int const y_end = std::min(h - 1, static_cast<int>(std::ceil(max_y)));
    int const x_start = std::max(0, static_cast<int>(std::floor(min_x)));
    int const x_end = std::min(w - 1, static_cast<int>(std::ceil(max_x)));

    // Line segment vector
    float const dx = p1.x - p0.x;
    float const dy = p1.y - p0.y;
    float const seg_len_sq = dx * dx + dy * dy;

    for (int y = y_start; y <= y_end; ++y) {
        for (int x = x_start; x <= x_end; ++x) {
            // Project pixel onto line segment to find nearest point
            float dist_sq;
            if (seg_len_sq < 1e-12f) {
                // Degenerate segment (zero length)
                float const px = static_cast<float>(x) - p0.x;
                float const py = static_cast<float>(y) - p0.y;
                dist_sq = px * px + py * py;
            } else {
                float const t = std::clamp(
                    ((static_cast<float>(x) - p0.x) * dx +
                     (static_cast<float>(y) - p0.y) * dy) / seg_len_sq,
                    0.0f, 1.0f);
                float const proj_x = p0.x + t * dx;
                float const proj_y = p0.y + t * dy;
                float const px = static_cast<float>(x) - proj_x;
                float const py = static_cast<float>(y) - proj_y;
                dist_sq = px * px + py * py;
            }

            float const val = std::exp(-dist_sq * inv_2sigma2);
            accessor[y][x] = std::max(accessor[y][x], val);
        }
    }
}

} // anonymous namespace

void Line2DEncoder::encode(Line2D const & line,
                           ImageSize const source_size,
                           torch::Tensor & tensor,
                           EncoderParams const & params) const
{
    if (params.mode != RasterMode::Binary && params.mode != RasterMode::Heatmap) {
        throw std::invalid_argument("Line2DEncoder: only Binary and Heatmap modes are supported");
    }

    if (line.size() < 2) {
        // A line with 0 or 1 points has nothing to rasterize
        return;
    }

    auto channel = tensor[params.batch_index][params.target_channel];
    auto accessor = channel.accessor<float, 2>();

    for (size_t i = 0; i + 1 < line.size(); ++i) {
        auto const p0 = scale_point(line[i], source_size, params.height, params.width);
        auto const p1 = scale_point(line[i + 1], source_size, params.height, params.width);

        if (params.mode == RasterMode::Binary) {
            rasterize_segment_binary(
                static_cast<int>(std::round(p0.x)),
                static_cast<int>(std::round(p0.y)),
                static_cast<int>(std::round(p1.x)),
                static_cast<int>(std::round(p1.y)),
                accessor, params.height, params.width);
        } else {
            rasterize_segment_heatmap(p0, p1, accessor,
                                      params.height, params.width,
                                      params.gaussian_sigma);
        }
    }
}

} // namespace dl
