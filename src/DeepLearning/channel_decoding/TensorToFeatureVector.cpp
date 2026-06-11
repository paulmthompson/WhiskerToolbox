/**
 * @file TensorToFeatureVector.cpp
 * @brief Implementation of the feature vector decoder.
 */

#include "TensorToFeatureVector.hpp"

#include <spdlog/spdlog.h>
#include <torch/types.h>// kCPU, kFloat32, at::Tensor

#include <cassert>
#include <cstring>
#include <stdexcept>
#include <string>

namespace dl {

namespace {

/**
 * @brief Validate tensor shape and decoder context for feature vector decoding.
 *
 * @pre tensor is defined (enforced via assert)
 */
void validateTensorToFeatureVectorInput(at::Tensor const & tensor, DecoderContext const & ctx) {
    assert(tensor.defined() && "TensorToFeatureVector: tensor must be defined");

    if (tensor.dim() == 0) {
        auto const message = "TensorToFeatureVector: expected at least 1D tensor";
        spdlog::debug("[TensorToFeatureVector] {}", message);
        throw std::invalid_argument(message);
    }

    if (tensor.dim() > 2) {
        auto const message =
                "TensorToFeatureVector: expected 1D [C] or 2D [B, C] tensor, got dim=" +
                std::to_string(tensor.dim());
        spdlog::debug("[TensorToFeatureVector] {}", message);
        throw std::invalid_argument(message);
    }

    if (tensor.dim() == 2) {
        auto const batch_size = tensor.size(0);
        if (ctx.batch_index < 0 || static_cast<int64_t>(ctx.batch_index) >= batch_size) {
            auto const message = "TensorToFeatureVector: batch_index " +
                                 std::to_string(ctx.batch_index) + " out of range [0, " +
                                 std::to_string(batch_size) + ")";
            spdlog::debug("[TensorToFeatureVector] {}", message);
            throw std::out_of_range(message);
        }
    }
}

}// namespace

std::string TensorToFeatureVector::name() const {
    return "TensorToFeatureVector";
}

std::string TensorToFeatureVector::outputTypeName() const {
    return "std::vector<float>";
}

std::vector<float> TensorToFeatureVector::decode(
        at::Tensor const & tensor,
        DecoderContext const & ctx,
        FeatureVectorDecoderParams const & /*params*/) {
    validateTensorToFeatureVectorInput(tensor, ctx);

    auto const cpu_tensor = tensor.to(torch::kCPU).to(torch::kFloat32).contiguous();

    at::Tensor row;
    if (cpu_tensor.dim() == 1) {
        row = cpu_tensor;
    } else {
        row = cpu_tensor[ctx.batch_index];
    }

    auto const feature_count = row.numel();
    std::vector<float> result(static_cast<std::size_t>(feature_count));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    std::memcpy(result.data(),
                row.data_ptr<float>(),
                static_cast<std::size_t>(feature_count) * sizeof(float));
    return result;
}

}// namespace dl
