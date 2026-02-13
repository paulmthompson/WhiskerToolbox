#include "TensorToPoint2D.hpp"

#include <algorithm>
#include <cmath>

namespace dl {

std::string TensorToPoint2D::name() const
{
    return "TensorToPoint2D";
}

std::string TensorToPoint2D::outputTypeName() const
{
    return "Point2D<float>";
}

namespace {

/// Scale a point from tensor coordinates to target image coordinates.
/// If target_image_size is {0, 0}, returns the point unchanged.
Point2D<float> scale_to_target(Point2D<float> const point,
                               int const tensor_h,
                               int const tensor_w,
                               ImageSize const target)
{
    if (target.width <= 0 || target.height <= 0) {
        return point;
    }
    float const sx = static_cast<float>(target.width) / static_cast<float>(tensor_w);
    float const sy = static_cast<float>(target.height) / static_cast<float>(tensor_h);
    return {point.x * sx, point.y * sy};
}

/// Parabolic subpixel refinement around a peak at (px, py).
/// Fits a 1D parabola along each axis through the peak and its two neighbors.
/// Returns the refined floating-point coordinate.
Point2D<float> refine_subpixel(torch::TensorAccessor<float, 2> const & accessor,
                               int const px, int const py,
                               int const h, int const w)
{
    float refined_x = static_cast<float>(px);
    float refined_y = static_cast<float>(py);

    // Refine x: fit parabola through (px-1, px, px+1)
    if (px > 0 && px < w - 1) {
        float const left = accessor[py][px - 1];
        float const center = accessor[py][px];
        float const right = accessor[py][px + 1];
        float const denom = 2.0f * (2.0f * center - left - right);
        if (std::abs(denom) > 1e-7f) {
            refined_x += (left - right) / denom;
        }
    }

    // Refine y: fit parabola through (py-1, py, py+1)
    if (py > 0 && py < h - 1) {
        float const top = accessor[py - 1][px];
        float const center = accessor[py][px];
        float const bottom = accessor[py + 1][px];
        float const denom = 2.0f * (2.0f * center - top - bottom);
        if (std::abs(denom) > 1e-7f) {
            refined_y += (top - bottom) / denom;
        }
    }

    return {refined_x, refined_y};
}

/// Check if pixel (px, py) is a local maximum (greater than all 8 neighbors)
bool is_local_maximum(torch::TensorAccessor<float, 2> const & accessor,
                      int const px, int const py,
                      int const h, int const w)
{
    float const val = accessor[py][px];

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int const nx = px + dx;
            int const ny = py + dy;
            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                if (accessor[ny][nx] >= val) {
                    return false;
                }
            }
        }
    }
    return true;
}

} // anonymous namespace

Point2D<float> TensorToPoint2D::decode(torch::Tensor const & tensor,
                                       DecoderParams const & params) const
{
    auto channel = tensor[params.batch_index][params.source_channel];
    auto const h = params.height;
    auto const w = params.width;

    // Find global argmax
    auto const flat_idx = channel.argmax().item<int64_t>();
    int const py = static_cast<int>(flat_idx / w);
    int const px = static_cast<int>(flat_idx % w);

    // Check if the channel is all zeros (no detection)
    auto accessor = channel.accessor<float, 2>();
    if (accessor[py][px] <= 0.0f) {
        return scale_to_target({0.0f, 0.0f}, h, w, params.target_image_size);
    }

    Point2D<float> result;
    if (params.subpixel) {
        result = refine_subpixel(accessor, px, py, h, w);
    } else {
        result = {static_cast<float>(px), static_cast<float>(py)};
    }

    return scale_to_target(result, h, w, params.target_image_size);
}

std::vector<Point2D<float>> TensorToPoint2D::decodeMultiple(
    torch::Tensor const & tensor,
    DecoderParams const & params) const
{
    auto channel = tensor[params.batch_index][params.source_channel];
    auto const h = params.height;
    auto const w = params.width;
    auto accessor = channel.accessor<float, 2>();

    std::vector<Point2D<float>> points;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (accessor[y][x] > params.threshold && is_local_maximum(accessor, x, y, h, w)) {
                Point2D<float> pt;
                if (params.subpixel) {
                    pt = refine_subpixel(accessor, x, y, h, w);
                } else {
                    pt = {static_cast<float>(x), static_cast<float>(y)};
                }
                points.push_back(scale_to_target(pt, h, w, params.target_image_size));
            }
        }
    }

    return points;
}

} // namespace dl
