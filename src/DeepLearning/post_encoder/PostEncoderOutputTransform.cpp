/**
 * @file PostEncoderOutputTransform.cpp
 * @brief Implementation of post-encoder output transform helpers.
 */

#include "PostEncoderOutputTransform.hpp"

#include "PostEncoderModule.hpp"

#include <cassert>
#include <utility>

namespace dl {

std::unordered_map<std::string, at::Tensor>
applyPostEncoderToFirstOutputSlot(
        std::unordered_map<std::string, at::Tensor> outputs,
        std::vector<TensorSlotDescriptor> const & raw_slots,
        PostEncoderModule const * module) {
    if (!module || raw_slots.empty()) {
        return outputs;
    }

    auto const & first_slot = raw_slots.front();
    auto it = outputs.find(first_slot.name);
    if (it == outputs.end()) {
        return outputs;
    }

    it->second = module->apply(it->second);
    return outputs;
}

std::vector<TensorSlotDescriptor>
effectiveOutputSlots(
        std::vector<TensorSlotDescriptor> const & raw_slots,
        PostEncoderModule const * module) {
    if (!module || raw_slots.empty()) {
        return raw_slots;
    }

    auto slots = raw_slots;
    slots.front().shape = module->outputShape(raw_slots.front().shape);
    return slots;
}

}// namespace dl
