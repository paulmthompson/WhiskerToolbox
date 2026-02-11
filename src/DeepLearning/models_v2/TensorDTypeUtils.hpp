#ifndef WHISKERTOOLBOX_TENSOR_DTYPE_UTILS_HPP
#define WHISKERTOOLBOX_TENSOR_DTYPE_UTILS_HPP

#include "TensorSlotDescriptor.hpp"

#include <torch/torch.h>

namespace dl {

/// Convert TensorDType enum to torch::ScalarType.
inline torch::ScalarType toTorchDType(TensorDType dtype)
{
    switch (dtype) {
        case TensorDType::Float32: return torch::kFloat32;
        case TensorDType::Float64: return torch::kFloat64;
        case TensorDType::Byte:    return torch::kByte;
        case TensorDType::Int32:   return torch::kInt32;
        case TensorDType::Int64:   return torch::kInt64;
        default:                   return torch::kFloat32;
    }
}

} // namespace dl

#endif // WHISKERTOOLBOX_TENSOR_DTYPE_UTILS_HPP
