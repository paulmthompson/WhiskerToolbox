/**
 * @file GlobalAvgPoolModule.cpp
 * @brief Implementation of the global average pooling post-encoder module.
 */

#include "GlobalAvgPoolModule.hpp"

#include <ATen/Functions.h>  // at::adaptive_avg_pool2d
#include <ATen/core/Tensor.h>// at::Tensor
#include <spdlog/spdlog.h>

#include <cassert>
#include <stdexcept>
#include <string>

namespace dl {

namespace {

/**
 * @brief Validate input tensor shape for global average pooling.
 *
 * @pre features is defined (enforced via assert)
 */
void validateGlobalAvgPoolInput(at::Tensor const & features) {
    assert(features.defined() && "GlobalAvgPoolModule: input tensor must be defined");

    if (features.dim() != 4) {
        auto const message =
                "GlobalAvgPoolModule: expected 4D tensor [B, C, H, W], got dim=" +
                std::to_string(features.dim());
        spdlog::debug("[GlobalAvgPoolModule] {}", message);
        throw std::invalid_argument(message);
    }

    auto const batch_size = features.size(0);
    auto const channel_count = features.size(1);
    auto const height = features.size(2);
    auto const width = features.size(3);

    if (batch_size < 1) {
        auto const message = "GlobalAvgPoolModule: batch dimension must be >= 1, got " +
                             std::to_string(batch_size);
        spdlog::debug("[GlobalAvgPoolModule] {}", message);
        throw std::invalid_argument(message);
    }

    if (channel_count < 1) {
        auto const message = "GlobalAvgPoolModule: channel dimension must be >= 1, got " +
                             std::to_string(channel_count);
        spdlog::debug("[GlobalAvgPoolModule] {}", message);
        throw std::invalid_argument(message);
    }

    if (height < 1 || width < 1) {
        auto const message = "GlobalAvgPoolModule: spatial dimensions must be >= 1, got H=" +
                             std::to_string(height) + ", W=" + std::to_string(width);
        spdlog::debug("[GlobalAvgPoolModule] {}", message);
        throw std::invalid_argument(message);
    }
}

/**
 * @brief Validate encoder output shape metadata for global average pooling.
 *
 * @pre input_shape is not empty (enforced via assert)
 */
void validateGlobalAvgPoolOutputShapeInput(std::vector<int64_t> const & input_shape) {
    assert(!input_shape.empty() && "GlobalAvgPoolModule: input_shape must not be empty");

    if (input_shape[0] < 1) {
        auto const message =
                "GlobalAvgPoolModule: channel dimension must be >= 1, got " +
                std::to_string(input_shape[0]);
        spdlog::debug("[GlobalAvgPoolModule] {}", message);
        throw std::invalid_argument(message);
    }
}

}// namespace

std::string GlobalAvgPoolModule::name() const {
    return "global_avg_pool";
}

at::Tensor GlobalAvgPoolModule::apply(at::Tensor const & features) const {
    validateGlobalAvgPoolInput(features);
    return at::adaptive_avg_pool2d(features, {1, 1}).squeeze(-1).squeeze(-1);
}

std::vector<int64_t>
GlobalAvgPoolModule::outputShape(std::vector<int64_t> const & input_shape) const {
    validateGlobalAvgPoolOutputShapeInput(input_shape);
    return {input_shape[0]};
}

}// namespace dl
