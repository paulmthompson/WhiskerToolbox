/**
 * @file TensorToMask2D.cpp
 * @brief Implementation of the spatial mask decoder.
 */

#include "TensorToMask2D.hpp"

#include <ATen/core/Tensor.h>// at::Tensor
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
 * @brief Validate tensor shape and decoder context for spatial mask decoding.
 *
 * @pre tensor is defined (enforced via assert)
 */
void validateTensorToMask2DInput(at::Tensor const & tensor, DecoderContext const & ctx) {
    assert(tensor.defined() && "TensorToMask2D: tensor must be defined");

    if (tensor.dim() != 4) {
        auto const message =
                "TensorToMask2D: expected 4D tensor [B, C, H, W], got dim=" +
                std::to_string(tensor.dim());
        spdlog::debug("[TensorToMask2D] {}", message);
        throw std::invalid_argument(message);
    }

    if (ctx.height <= 0 || ctx.width <= 0) {
        auto const message = "TensorToMask2D: ctx.height and ctx.width must be > 0";
        spdlog::debug("[TensorToMask2D] {}", message);
        throw std::invalid_argument(message);
    }

    auto const batch_size = tensor.size(0);
    auto const channel_count = tensor.size(1);
    auto const tensor_height = tensor.size(2);
    auto const tensor_width = tensor.size(3);

    if (ctx.batch_index < 0 || static_cast<int64_t>(ctx.batch_index) >= batch_size) {
        auto const message = "TensorToMask2D: batch_index " +
                             std::to_string(ctx.batch_index) + " out of range [0, " +
                             std::to_string(batch_size) + ")";
        spdlog::debug("[TensorToMask2D] {}", message);
        throw std::out_of_range(message);
    }

    if (ctx.source_channel < 0 ||
        static_cast<int64_t>(ctx.source_channel) >= channel_count) {
        auto const message = "TensorToMask2D: source_channel " +
                             std::to_string(ctx.source_channel) + " out of range [0, " +
                             std::to_string(channel_count) + ")";
        spdlog::debug("[TensorToMask2D] {}", message);
        throw std::out_of_range(message);
    }

    if (tensor_height != ctx.height || tensor_width != ctx.width) {
        auto const message = "TensorToMask2D: tensor spatial dims [" +
                             std::to_string(tensor_height) + ", " +
                             std::to_string(tensor_width) + "] do not match DecoderContext [" +
                             std::to_string(ctx.height) + ", " + std::to_string(ctx.width) + "]";
        spdlog::debug("[TensorToMask2D] {}", message);
        throw std::invalid_argument(message);
    }
}

}// namespace

std::string TensorToMask2D::name() const {
    return "TensorToMask2D";
}

std::string TensorToMask2D::outputTypeName() const {
    return "Mask2D";
}

Mask2D TensorToMask2D::decode(at::Tensor const & tensor,
                              DecoderContext const & ctx,
                              MaskDecoderParams const & params) {
    validateTensorToMask2DInput(tensor, ctx);

    auto channel = tensor[ctx.batch_index][ctx.source_channel]
                           .to(torch::kCPU)
                           .to(torch::kFloat32)
                           .contiguous();
    auto const h = ctx.height;
    auto const w = ctx.width;
    auto accessor = channel.accessor<float, 2>();

    bool const needs_scaling =
            ctx.target_image_size.width > 0 && ctx.target_image_size.height > 0;

    if (!needs_scaling) {
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

    int const dest_w = ctx.target_image_size.width;
    int const dest_h = ctx.target_image_size.height;

    float const x_scale = static_cast<float>(w) / static_cast<float>(dest_w);
    float const y_scale = static_cast<float>(h) / static_cast<float>(dest_h);

    Mask2D mask;
    for (int dest_y = 0; dest_y < dest_h; ++dest_y) {
        int const src_y = std::clamp(
                static_cast<int>((static_cast<float>(dest_y) + 0.5f) * y_scale),
                0, h - 1);

        for (int dest_x = 0; dest_x < dest_w; ++dest_x) {
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

}// namespace dl
