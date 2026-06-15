#ifndef WHISKERTOOLBOX_DATA_BANK_ENCODE_HPP
#define WHISKERTOOLBOX_DATA_BANK_ENCODE_HPP

/**
 * @file DataBankEncode.hpp
 * @brief Free functions for encoding DataBank sources into model-ready tensors.
 */

#include "channel_encoding/EncoderDispatch.hpp"// EncodingSourceVariant, EncoderParamsVariant
#include "models_v2/TensorSlotDescriptor.hpp"  // TensorSlotDescriptor

#include "CoreGeometry/ImageSize.hpp"

#include <ATen/core/Tensor.h>// at::Tensor

#include <cstdint>
#include <optional>
#include <vector>

namespace dl {

/**
 * @brief Compute the per-element shape for a sequence slot.
 *
 * For a slot with shape {4, 3, 256, 256} and sequence_dim=0, returns
 * {3, 256, 256}.
 *
 * @pre slot.hasSequenceDim() must be true
 */
[[nodiscard]] std::vector<int64_t> perElementShape(TensorSlotDescriptor const & slot);

/**
 * @brief Shape used when encoding a single bank entry for a slot.
 *
 * Excludes the sequence dimension when present; otherwise returns slot.shape.
 */
[[nodiscard]] std::vector<int64_t> encodingShapeForSlot(TensorSlotDescriptor const & slot);

/**
 * @brief Build an EncoderContext from a slot's encoding shape.
 */
[[nodiscard]] EncoderContext makeEncoderContext(
        TensorSlotDescriptor const & slot,
        int batch_index = 0);

/**
 * @brief Channel-encode a source into a new single-batch tensor.
 *
 * Allocates a tensor with shape {1, ...encoding_shape} and writes the encoded
 * values using the supplied encoder parameters.
 *
 * @pre encoding_shape must match encodingShapeForSlot(slot)
 * @return Encoded tensor on success, or nullopt on failure
 */
[[nodiscard]] std::optional<at::Tensor> encodeSourceToTensor(
        EncodingSourceVariant const & source,
        TensorSlotDescriptor const & slot,
        EncoderParamsVariant const & encoder_params,
        ImageSize source_image_size);

}// namespace dl

#endif// WHISKERTOOLBOX_DATA_BANK_ENCODE_HPP
