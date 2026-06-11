/**
 * @file TensorToPoint2D.cpp
 * @brief Implementation of the spatial point decoder.
 */

#include "TensorToPoint2D.hpp"

#include <ATen/core/Tensor.h>        // at::Tensor
#include <ATen/core/TensorAccessor.h>// at::TensorAccessor
#include <spdlog/spdlog.h>
#include <torch/types.h>// kCPU, kFloat32

#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <string>

namespace dl {

namespace {

/**
 * @brief Validate tensor shape and decoder context for spatial point decoding.
 *
 * @pre tensor is defined (enforced via assert)
 */
void validateTensorToPoint2DInput(at::Tensor const & tensor, DecoderContext const & ctx) {
    assert(tensor.defined() && "TensorToPoint2D: tensor must be defined");

    if (tensor.dim() != 4) {
        auto const message =
                "TensorToPoint2D: expected 4D tensor [B, C, H, W], got dim=" +
                std::to_string(tensor.dim());
        spdlog::debug("[TensorToPoint2D] {}", message);
        throw std::invalid_argument(message);
    }

    if (ctx.height <= 0 || ctx.width <= 0) {
        auto const message = "TensorToPoint2D: ctx.height and ctx.width must be > 0";
        spdlog::debug("[TensorToPoint2D] {}", message);
        throw std::invalid_argument(message);
    }

    auto const batch_size = tensor.size(0);
    auto const channel_count = tensor.size(1);
    auto const tensor_height = tensor.size(2);
    auto const tensor_width = tensor.size(3);

    if (ctx.batch_index < 0 || static_cast<int64_t>(ctx.batch_index) >= batch_size) {
        auto const message = "TensorToPoint2D: batch_index " +
                             std::to_string(ctx.batch_index) + " out of range [0, " +
                             std::to_string(batch_size) + ")";
        spdlog::debug("[TensorToPoint2D] {}", message);
        throw std::out_of_range(message);
    }

    if (ctx.source_channel < 0 ||
        static_cast<int64_t>(ctx.source_channel) >= channel_count) {
        auto const message = "TensorToPoint2D: source_channel " +
                             std::to_string(ctx.source_channel) + " out of range [0, " +
                             std::to_string(channel_count) + ")";
        spdlog::debug("[TensorToPoint2D] {}", message);
        throw std::out_of_range(message);
    }

    if (tensor_height != ctx.height || tensor_width != ctx.width) {
        auto const message = "TensorToPoint2D: tensor spatial dims [" +
                             std::to_string(tensor_height) + ", " +
                             std::to_string(tensor_width) + "] do not match DecoderContext [" +
                             std::to_string(ctx.height) + ", " + std::to_string(ctx.width) + "]";
        spdlog::debug("[TensorToPoint2D] {}", message);
        throw std::invalid_argument(message);
    }
}

/**
 * @brief Scale a point from tensor coordinates to target image coordinates.
 *
 * If target_image_size is {0, 0}, returns the point unchanged.
 */
Point2D<float> scale_to_target(Point2D<float> const point,
                               int const tensor_h,
                               int const tensor_w,
                               ImageSize const target) {
    if (target.width <= 0 || target.height <= 0) {
        return point;
    }
    float const sx = static_cast<float>(target.width) / static_cast<float>(tensor_w);
    float const sy = static_cast<float>(target.height) / static_cast<float>(tensor_h);
    return {point.x * sx, point.y * sy};
}

/**
 * @brief Parabolic subpixel refinement around a peak at (px, py).
 *
 * Fits a 1D parabola along each axis through the peak and its two neighbors.
 */
Point2D<float> refine_subpixel(at::TensorAccessor<float, 2> const & accessor,
                               int const px, int const py,
                               int const h, int const w) {
    auto refined_x = static_cast<float>(px);
    auto refined_y = static_cast<float>(py);

    if (px > 0 && px < w - 1) {
        float const left = accessor[py][px - 1];
        float const center = accessor[py][px];
        float const right = accessor[py][px + 1];
        float const denom = 2.0f * (2.0f * center - left - right);
        if (std::abs(denom) > 1e-7f) {
            refined_x += (left - right) / denom;
        }
    }

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

/**
 * @brief Check if pixel (px, py) is a local maximum (greater than all 8 neighbors).
 */
bool is_local_maximum(at::TensorAccessor<float, 2> const & accessor,
                      int const px, int const py,
                      int const h, int const w) {
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

at::Tensor extractChannel(at::Tensor const & tensor, DecoderContext const & ctx) {
    return tensor[ctx.batch_index][ctx.source_channel]
            .to(torch::kCPU)
            .to(torch::kFloat32)
            .contiguous();
}

}// namespace

std::string TensorToPoint2D::name() const {
    return "TensorToPoint2D";
}

std::string TensorToPoint2D::outputTypeName() const {
    return "Point2D<float>";
}

Point2D<float> TensorToPoint2D::decode(at::Tensor const & tensor,
                                       DecoderContext const & ctx,
                                       PointDecoderParams const & params) {
    validateTensorToPoint2DInput(tensor, ctx);

    auto channel = extractChannel(tensor, ctx);
    auto const h = ctx.height;
    auto const w = ctx.width;

    auto const flat_idx = channel.argmax().item<int64_t>();
    int const py = static_cast<int>(flat_idx / w);
    int const px = static_cast<int>(flat_idx % w);

    auto accessor = channel.accessor<float, 2>();
    if (accessor[py][px] <= 0.0f) {
        return scale_to_target({0.0f, 0.0f}, h, w, ctx.target_image_size);
    }

    Point2D<float> result;
    if (params.subpixel) {
        result = refine_subpixel(accessor, px, py, h, w);
    } else {
        result = {static_cast<float>(px), static_cast<float>(py)};
    }

    return scale_to_target(result, h, w, ctx.target_image_size);
}

std::vector<Point2D<float>> TensorToPoint2D::decodeMultiple(
        at::Tensor const & tensor,
        DecoderContext const & ctx,
        PointDecoderParams const & params) {
    validateTensorToPoint2DInput(tensor, ctx);

    auto channel = extractChannel(tensor, ctx);
    auto const h = ctx.height;
    auto const w = ctx.width;
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
                points.push_back(scale_to_target(pt, h, w, ctx.target_image_size));
            }
        }
    }

    return points;
}

}// namespace dl
