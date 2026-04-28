/**
 * @file GlobalAvgPoolModule.cpp
 * @brief Implementation of the global average pooling post-encoder module.
 */

#include "GlobalAvgPoolModule.hpp"

#include <torch/torch.h>

#include <cassert>
#include <stdexcept>

namespace dl {

std::string GlobalAvgPoolModule::name() const {
    return "global_avg_pool";
}

at::Tensor GlobalAvgPoolModule::apply(at::Tensor const & features) const {
    assert(features.defined() && "GlobalAvgPoolModule: input tensor must be defined");
    if (features.dim() != 4) {
        throw std::invalid_argument(
                "GlobalAvgPoolModule: expected 4D tensor [B, C, H, W], got dim=" +
                std::to_string(features.dim()));
    }
    // adaptive_avg_pool2d → [B, C, 1, 1], then squeeze spatial dims
    return torch::adaptive_avg_pool2d(features, {1, 1}).squeeze(-1).squeeze(-1);
}

std::vector<int64_t>
GlobalAvgPoolModule::outputShape(std::vector<int64_t> const & input_shape) const {
    assert(!input_shape.empty() && "GlobalAvgPoolModule: input_shape must not be empty");
    // input_shape is [C, H, W] (excluding batch dim); output is [C]
    return {input_shape[0]};
}

}// namespace dl
