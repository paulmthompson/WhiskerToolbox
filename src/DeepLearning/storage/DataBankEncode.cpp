/**
 * @file DataBankEncode.cpp
 * @brief Implementation of DataBank encoding helpers.
 */

#include "DataBankEncode.hpp"

#include "models_v2/TensorDTypeUtils.hpp"

#include <ATen/Functions.h>

#include <cassert>
#include <cstdint>
#include <vector>

namespace dl {

std::vector<int64_t> perElementShape(TensorSlotDescriptor const & slot) {
    assert(slot.hasSequenceDim() && "perElementShape: slot must have a sequence dim");
    std::vector<int64_t> shape;
    shape.reserve(slot.shape.size() - 1);
    for (std::size_t i = 0; i < slot.shape.size(); ++i) {
        if (static_cast<int>(i) != slot.sequence_dim) {
            shape.push_back(slot.shape[i]);
        }
    }
    return shape;
}

std::vector<int64_t> encodingShapeForSlot(TensorSlotDescriptor const & slot) {
    if (slot.hasSequenceDim()) {
        return perElementShape(slot);
    }
    return slot.shape;
}

EncoderContext makeEncoderContext(TensorSlotDescriptor const & slot, int batch_index) {
    auto const encode_shape = encodingShapeForSlot(slot);

    EncoderContext ctx;
    ctx.target_channel = 0;
    ctx.batch_index = batch_index;

    if (encode_shape.size() >= 2) {
        ctx.height = static_cast<int>(encode_shape[encode_shape.size() - 2]);
        ctx.width = static_cast<int>(encode_shape[encode_shape.size() - 1]);
    } else if (encode_shape.size() == 1) {
        ctx.height = 1;
        ctx.width = static_cast<int>(encode_shape[0]);
    }

    return ctx;
}

std::optional<at::Tensor> encodeSourceToTensor(
        EncodingSourceVariant const & source,
        TensorSlotDescriptor const & slot,
        EncoderVariant const & encoder_params,
        ImageSize source_image_size) {

    auto const encode_shape = encodingShapeForSlot(slot);

    std::vector<int64_t> tensor_shape = {1};
    tensor_shape.insert(tensor_shape.end(), encode_shape.begin(), encode_shape.end());

    auto tensor = at::zeros(tensor_shape, toTorchDType(slot.dtype));

    TensorSlotDescriptor encode_slot = slot;
    encode_slot.shape = encode_shape;
    encode_slot.sequence_dim = -1;

    auto const ctx = makeEncoderContext(encode_slot, /*batch_index=*/0);
    encodeToTensor(source, tensor, ctx, source_image_size, encoder_params);

    return tensor;
}

}// namespace dl
