/**
 * @file PostEncoderOutputTransform.hpp
 * @brief Free functions for applying post-encoder modules to model output tensors.
 */

#ifndef WHISKERTOOLBOX_POST_ENCODER_OUTPUT_TRANSFORM_HPP
#define WHISKERTOOLBOX_POST_ENCODER_OUTPUT_TRANSFORM_HPP

#include "models_v2/TensorSlotDescriptor.hpp"

#include <ATen/core/Tensor.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace dl {

class PostEncoderModule;

/**
 * @brief Apply a post-encoder module to the first model output slot tensor.
 *
 * @param outputs Raw model forward outputs (slot name → tensor).
 * @param raw_slots Model output slot descriptors in declaration order.
 * @param module Post-encoder module to apply; pass-through when null.
 * @return Output map with the first slot tensor transformed when @p module is set.
 *
 * @pre raw_slots must not be empty when @p module is non-null.
 */
[[nodiscard]] std::unordered_map<std::string, at::Tensor>
applyPostEncoderToFirstOutputSlot(
        std::unordered_map<std::string, at::Tensor> outputs,
        std::vector<TensorSlotDescriptor> const & raw_slots,
        PostEncoderModule const * module);

/**
 * @brief Compute output slot descriptors after post-encoder shape propagation.
 *
 * @param raw_slots Model output slot descriptors (raw encoder shapes).
 * @param module Post-encoder module; returns @p raw_slots unchanged when null.
 * @return Slot descriptors with the first slot shape updated when @p module is set.
 *
 * @pre raw_slots must not be empty when @p module is non-null.
 */
[[nodiscard]] std::vector<TensorSlotDescriptor>
effectiveOutputSlots(
        std::vector<TensorSlotDescriptor> const & raw_slots,
        PostEncoderModule const * module);

}// namespace dl

#endif// WHISKERTOOLBOX_POST_ENCODER_OUTPUT_TRANSFORM_HPP
