/// @file TensorToFeatureVector.cpp
/// @brief Implementation of the feature vector decoder.

#include "TensorToFeatureVector.hpp"

#include <torch/torch.h>

#include <cassert>
#include <stdexcept>

namespace dl {

std::string TensorToFeatureVector::name() const {
    return "TensorToFeatureVector";
}

std::string TensorToFeatureVector::outputTypeName() const {
    return "std::vector<float>";
}

std::vector<float> TensorToFeatureVector::decode(
        at::Tensor const & tensor,
        DecoderParams const & params) const {
    assert(tensor.defined() && "TensorToFeatureVector: tensor must be defined");

    if (tensor.dim() == 0) {
        throw std::invalid_argument(
                "TensorToFeatureVector: expected at least 1D tensor");
    }

    // Move to CPU and ensure float32 for copying
    auto const cpu_tensor = tensor.to(torch::kCPU).to(torch::kFloat32).contiguous();

    at::Tensor row;
    if (cpu_tensor.dim() == 1) {
        // Unbatched [C]
        row = cpu_tensor;
    } else if (cpu_tensor.dim() == 2) {
        // Batched [B, C] — select the requested batch index
        auto const B = cpu_tensor.size(0);
        auto const idx = static_cast<int64_t>(params.batch_index);
        if (idx < 0 || idx >= B) {
            throw std::out_of_range(
                    "TensorToFeatureVector: batch_index " +
                    std::to_string(params.batch_index) +
                    " out of range [0, " + std::to_string(B) + ")");
        }
        row = cpu_tensor[idx];
    } else {
        throw std::invalid_argument(
                "TensorToFeatureVector: expected 1D [C] or 2D [B, C] tensor, "
                "got dim=" + std::to_string(cpu_tensor.dim()));
    }

    // Copy data pointer to std::vector
    auto const C = row.numel();
    std::vector<float> result(static_cast<std::size_t>(C));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    std::memcpy(result.data(),
                row.data_ptr<float>(),
                static_cast<std::size_t>(C) * sizeof(float));
    return result;
}

}// namespace dl
