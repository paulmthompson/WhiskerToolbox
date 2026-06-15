#include "SlotAssembler.hpp"

#include "DeepLearning/bindings/DeepLearningBindingData.hpp"
#include "DeepLearningParamSchemas.hpp"
#include "Inference/BatchInferenceResult.hpp"

#include "DataManager/DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Media/Media_Data.hpp"
#include "Points/Point_Data.hpp"
#include "Tensors/TensorData.hpp"

#include "channel_encoding/ChannelEncoder.hpp"
#include "channel_encoding/EncoderDispatch.hpp"
#include "channel_encoding/EncoderFactory.hpp"

#include "channel_decoding/ChannelDecoder.hpp"
#include "channel_decoding/DecoderDispatch.hpp"
#include "channel_decoding/DecoderFactory.hpp"

#include "models_v2/ModelBase.hpp"
#include "models_v2/TensorDTypeUtils.hpp"
#include "models_v2/TensorSlotDescriptor.hpp"
#include "models_v2/general_encoder/GeneralEncoderModel.hpp"
#include "post_encoder/PostEncoderModule.hpp"
#include "post_encoder/PostEncoderModuleRegistry.hpp"
#include "post_encoder/PostEncoderOutputTransform.hpp"
#include "post_encoder/SpatialPointExtractModule.hpp"
#include "registry/ModelRegistry.hpp"
#include "storage/DataBank.hpp"
#include "storage/DataBankEncode.hpp"

#include "device/DeviceManager.hpp"

#include <spdlog/spdlog.h>

#include <ATen/core/Tensor.h>             // at::Tensor
#include <c10/core/Device.h>              // Device, kCPU, kCUDA
#include <torch/csrc/autograd/grad_mode.h>// torch::NoGradGuard

#include <algorithm>
#include <atomic>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

// ════════════════════════════════════════════════════════════════════════════
// PIMPL
// ════════════════════════════════════════════════════════════════════════════

struct SlotAssembler::Impl {
    std::unique_ptr<dl::ModelBase> model;
    std::string model_id;
    std::unique_ptr<dl::PostEncoderModule> post_encoder_module;

    /// Cached tensors for recurrent (feedback) inputs.
    /// Key: "recurrent:input_slot_name" (from recurrentCacheKey()).
    /// Stores the output tensor from the previous frame that will be
    /// injected into the input slot on the next frame.
    std::unordered_map<std::string, at::Tensor> recurrent_cache;

    /// Named library of geometry sources and channel-encoded tensors.
    std::unique_ptr<dl::DataBank> data_bank;
};

// ════════════════════════════════════════════════════════════════════════════
// Anonymous helpers
// ════════════════════════════════════════════════════════════════════════════

namespace {

[[nodiscard]] std::vector<dl::TensorSlotDescriptor>
effectiveOutputSlotsFor(
        dl::ModelBase const * model,
        dl::PostEncoderModule const * post_encoder) {
    if (!model) {
        return {};
    }
    return dl::effectiveOutputSlots(model->outputSlots(), post_encoder);
}

[[nodiscard]] std::unordered_map<std::string, at::Tensor>
runModelAndPostEncoder(
        dl::ModelBase & model,
        dl::PostEncoderModule const * post_encoder,
        std::unordered_map<std::string, at::Tensor> const & inputs) {
    auto outputs = model.forward(inputs);
    return dl::applyPostEncoderToFirstOutputSlot(
            std::move(outputs),
            model.outputSlots(),
            post_encoder);
}

[[nodiscard]] bool requiresSingleFrameBatch(
        dl::PostEncoderModule const * post_encoder) {
    if (!post_encoder) {
        return false;
    }
    return dynamic_cast<dl::SpatialPointExtractModule const *>(post_encoder) !=
           nullptr;
}

dl::TensorSlotDescriptor const *
findSlot(std::vector<dl::TensorSlotDescriptor> const & slot_vec,
         std::string const & name) {
    for (auto const & s: slot_vec) {
        if (s.name == name) return &s;
    }
    return nullptr;
}

/// Get the sequence length from a slot's shape.
///
/// @pre slot.hasSequenceDim() must be true; without it, slot.sequence_dim is
///      negative and the cast to std::size_t wraps to a huge index, causing
///      out-of-bounds access on slot.shape
///      (enforcement: assert) [CRITICAL]
int64_t sequenceLength(dl::TensorSlotDescriptor const & slot) {
    assert(slot.hasSequenceDim() && "sequenceLength: slot must have a sequence dim");
    return slot.shape[static_cast<std::size_t>(slot.sequence_dim)];
}

dl::EncoderContext makeDynamicSlotEncoderContext(
        dl::TensorSlotDescriptor const & slot,
        int batch_index) {

    dl::EncoderContext ctx;
    ctx.target_channel = 0;
    ctx.batch_index = batch_index;

    if (slot.shape.size() >= 2) {
        ctx.height = static_cast<int>(slot.shape[slot.shape.size() - 2]);
        ctx.width = static_cast<int>(slot.shape[slot.shape.size() - 1]);
    } else if (slot.shape.size() == 1) {
        ctx.height = 1;
        ctx.width = static_cast<int>(slot.shape[0]);
    }

    return ctx;
}

/// Expand a batch-1 tensor along dimension 0 when batch_size > 1.
[[nodiscard]] at::Tensor expandLeadingBatchDim(
        at::Tensor tensor, int batch_size) {
    if (batch_size > 1 && tensor.size(0) == 1) {
        std::vector<int64_t> expand_sizes(
                static_cast<std::size_t>(tensor.dim()), -1);
        expand_sizes[0] = batch_size;
        return tensor.expand(expand_sizes);
    }
    return tensor;
}

/// Fetch raw encoding source from DataManager without writing a tensor.
[[nodiscard]] std::optional<dl::EncodingSourceVariant> fetchEncodingSource(
        DataManager & dm,
        SlotBindingData const & binding,
        int frame,
        MediaOverrides const * media_overrides = nullptr) {

    return binding.encoder.visit(
            [&](auto const & params) -> std::optional<dl::EncodingSourceVariant> {
                using ParamsT = std::decay_t<decltype(params)>;

                if constexpr (std::is_same_v<ParamsT, dl::ImageEncoderParams>) {
                    std::shared_ptr<MediaData> media;
                    if (media_overrides) {
                        auto ov_it = media_overrides->find(binding.data_key);
                        if (ov_it != media_overrides->end()) {
                            media = ov_it->second;
                        }
                    }
                    if (!media) {
                        media = dm.getData<MediaData>(binding.data_key);
                    }
                    if (!media) {
                        spdlog::error(
                                "SlotAssembler: MediaData not found for key '{}'",
                                binding.data_key);
                        return std::nullopt;
                    }
                    media->LoadFrame(frame);

                    int const channels =
                            (media->getFormat() == MediaData::DisplayFormat::Color)
                                    ? 3
                                    : 1;

                    dl::ImageEncodingSource source;
                    source.size = media->getImageSize();
                    source.channels = channels;
                    if (media->is8Bit()) {
                        source.is_8bit = true;
                        source.data_u8 = media->getRawData8(frame);
                    } else {
                        source.is_8bit = false;
                        source.data_f32 = media->getRawData32(frame);
                    }
                    return source;

                } else if constexpr (std::is_same_v<ParamsT,
                                                    dl::Point2DEncoderParams>) {
                    auto point_data = dm.getData<PointData>(binding.data_key);
                    if (!point_data) {
                        return std::nullopt;
                    }

                    auto points_at_frame =
                            point_data->getAtTime(TimeFrameIndex(frame));
                    std::vector<Point2D<float>> points_vec;
                    points_vec.reserve(
                            static_cast<std::size_t>(points_at_frame.size()));
                    for (auto const & pt: points_at_frame) {
                        points_vec.push_back(pt);
                    }
                    return points_vec;

                } else if constexpr (std::is_same_v<ParamsT,
                                                    dl::Mask2DEncoderParams>) {
                    auto mask_data = dm.getData<MaskData>(binding.data_key);
                    if (!mask_data) {
                        return std::nullopt;
                    }

                    auto masks_at_frame =
                            mask_data->getAtTime(TimeFrameIndex(frame));
                    Mask2D mask_to_encode;
                    for (auto const & m: masks_at_frame) {
                        mask_to_encode = m;
                        break;
                    }
                    return mask_to_encode;

                } else if constexpr (std::is_same_v<ParamsT,
                                                    dl::Line2DEncoderParams>) {
                    auto line_data = dm.getData<LineData>(binding.data_key);
                    if (!line_data) {
                        return std::nullopt;
                    }

                    auto lines_at_frame =
                            line_data->getAtTime(TimeFrameIndex(frame));
                    Line2D line_to_encode;
                    for (auto const & l: lines_at_frame) {
                        line_to_encode = l;
                        break;
                    }
                    return line_to_encode;
                } else {
                    return std::nullopt;
                }
            });
}

/// Find a static input entry matching slot name and memory index.
[[nodiscard]] StaticInputData const * findStaticInputEntry(
        std::vector<StaticInputData> const & static_inputs,
        std::string const & slot_name,
        int memory_index) {
    for (auto const & si: static_inputs) {
        if (si.slot_name == slot_name && si.memory_index == memory_index) {
            return &si;
        }
    }
    return nullptr;
}

/// Resolve a pre-encoded tensor from the DataBank.
[[nodiscard]] std::optional<at::Tensor> getBankCachedTensor(
        StaticInputData const & entry,
        dl::DataBank const & data_bank,
        int batch_size) {

    auto const bank_id = entry.resolvedBankEntryId();
    if (bank_id.empty()) {
        return std::nullopt;
    }

    auto const cached = data_bank.getEncoded(bank_id);
    if (!cached) {
        spdlog::error(
                "SlotAssembler: bank entry '{}' has no encoded tensor for slot '{}'",
                bank_id,
                entry.slot_name);
        return std::nullopt;
    }

    return expandLeadingBatchDim(*cached, batch_size);
}

void encodeDynamicSlot(
        DataManager & dm,
        SlotBindingData const & binding,
        dl::TensorSlotDescriptor const & slot,
        at::Tensor & tensor,
        int frame,
        int batch_index,
        ImageSize const & source_image_size,
        MediaOverrides const * media_overrides);

/// Encode a static entry into a single-batch temp tensor (DataManager or DataBank source).
[[nodiscard]] bool encodeStaticEntryToTensor(
        DataManager & dm,
        StaticInputData const & entry,
        dl::TensorSlotDescriptor const & encode_slot,
        dl::DataBank const & data_bank,
        at::Tensor & temp,
        int frame,
        ImageSize source_image_size,
        MediaOverrides const * media_overrides = nullptr) {

    if (entry.sourceType() == StaticInputSource::DataBank) {
        auto const bank_id = entry.resolvedBankEntryId();
        auto const source = data_bank.getSource(bank_id);
        if (!source) {
            spdlog::error(
                    "SlotAssembler: bank entry '{}' has no source for slot '{}'",
                    bank_id,
                    entry.slot_name);
            return false;
        }

        auto const encoder_params =
                dl::encoderParamsFromFactoryName(encode_slot.recommended_encoder);
        if (!encoder_params) {
            spdlog::error(
                    "SlotAssembler: unknown encoder '{}' for slot '{}'",
                    encode_slot.recommended_encoder,
                    entry.slot_name);
            return false;
        }

        ImageSize encode_image_size = source_image_size;
        if (auto const bank_entry = data_bank.get(bank_id)) {
            encode_image_size = bank_entry->metadata.source_image_size;
        }

        auto encoded = dl::encodeSourceToTensor(
                *source, encode_slot, *encoder_params, encode_image_size);
        if (!encoded) {
            spdlog::error(
                    "SlotAssembler: failed to encode bank entry '{}' for slot '{}'",
                    bank_id,
                    entry.slot_name);
            return false;
        }
        at::Tensor write_target =
                temp.dim() > 0 && temp.size(0) > 1 ? temp.narrow(0, 0, 1) : temp;
        write_target.copy_(*encoded);
        return true;
    }

    if (entry.data_key.empty()) {
        return false;
    }

    SlotBindingData temp_binding;
    temp_binding.slot_name = encode_slot.name;
    temp_binding.data_key = entry.data_key;
    dl::widget::assignEncoderFromFactoryName(
            temp_binding.encoder, encode_slot.recommended_encoder);
    encodeDynamicSlot(
            dm, temp_binding, encode_slot, temp, frame, 0,
            source_image_size, media_overrides);
    return true;
}

/// Encode a dynamic (per-frame) input slot into the tensor.
///
/// @param dm DataManager to fetch data from
/// @param binding Slot binding configuration
/// @param slot Tensor slot descriptor
/// @param tensor Output tensor to write to
/// @param frame Frame number to encode
/// @param batch_index Batch index in the tensor
/// @param source_image_size Original image dimensions (for coordinate scaling of masks/points/lines)
/// @param media_overrides Optional map of data_key -> cloned MediaData for
///        thread-safe frame reading (nullptr = use DataManager only)
///
/// @pre frame >= 0; for ImageEncoder, negative frames are passed directly to
///      MediaData::LoadFrame() whose behavior is implementation-defined and
///      potentially unsafe; for other encoders a negative TimeFrameIndex
///      returns empty data silently; all callers enforce via computeEncodingFrame()
///      clamping (enforcement: none) [IMPORTANT]
/// @pre batch_index >= 0; it is used inside encoder implementations to index
///      the batch dimension of tensor, causing out-of-bounds tensor access if
///      negative; all callers pass a loop variable b >= 0 or hardcoded 0
///      (enforcement: none) [CRITICAL]
void encodeDynamicSlot(
        DataManager & dm,
        SlotBindingData const & binding,
        dl::TensorSlotDescriptor const & slot,
        at::Tensor & tensor,
        int frame,
        int batch_index,
        ImageSize const & source_image_size,
        MediaOverrides const * media_overrides = nullptr) {

    auto const source = fetchEncodingSource(
            dm, binding, frame, media_overrides);
    if (!source) {
        return;
    }

    auto const ctx = makeDynamicSlotEncoderContext(slot, batch_index);
    binding.encoder.visit([&](auto const & params) {
        dl::encodeToTensor(*source, tensor, ctx, source_image_size, params);
    });
}

/// Build a set of recurrent-claimed positions for each sequence slot.
///
/// Returns a map from slot_name -> set of memory_index values that are
/// claimed by a recurrent binding (i.e., should NOT be filled by static
/// assembly because the recurrent injection will fill them).
std::unordered_map<std::string, std::set<int>>
recurrentClaimedPositions(
        std::vector<RecurrentBindingData> const & recurrent_bindings) {
    std::unordered_map<std::string, std::set<int>> result;
    for (auto const & rb: recurrent_bindings) {
        if (rb.hasTargetMemoryIndex()) {
            result[rb.input_slot_name].insert(rb.target_memory_index);
        }
    }
    return result;
}

/// Build the full input tensor map for a model forward pass.
///
/// @pre current_frame >= 0; negative values are silently clamped to 0 by
///      every internal computeEncodingFrame() call, producing incorrect
///      temporal alignment (enforcement: none) [IMPORTANT]
/// @pre batch_size >= 1; batch_size=0 produces empty tensors that cause a
///      downstream error in model forward(); batch_size<0 throws from
///      LibTorch when creating tensors with a negative leading dimension;
///      all callers clamp to >= 1 before calling (enforcement: none) [IMPORTANT]
std::unordered_map<std::string, at::Tensor>
assembleInputs(
        DataManager & dm,
        dl::ModelBase const & model,
        std::vector<SlotBindingData> const & input_bindings,
        std::vector<StaticInputData> const & static_inputs,
        dl::DataBank const & data_bank,
        std::vector<RecurrentBindingData> const & recurrent_bindings,
        int current_frame,
        int batch_size,
        MediaOverrides const * media_overrides = nullptr) {

    std::unordered_map<std::string, at::Tensor> result;
    auto const input_slot_vec = model.inputSlots();

    // ── Detect source image size from ImageEncoder binding ──
    // Masks, points, and lines are stored in original image coordinates,
    // so we need to know the original image dimensions for proper scaling.
    ImageSize source_image_size{0, 0};
    for (auto const & binding: input_bindings) {
        if (dl::widget::isImageEncoder(binding.encoder) && !binding.data_key.empty()) {
            // Check overrides first
            std::shared_ptr<MediaData> media;
            if (media_overrides) {
                auto ov_it = media_overrides->find(binding.data_key);
                if (ov_it != media_overrides->end()) media = ov_it->second;
            }
            if (!media) media = dm.getData<MediaData>(binding.data_key);
            if (media) {
                source_image_size = media->getImageSize();
                break;
            }
        }
    }

    // ── Determine max valid frame for clamping ──
    int max_frame = std::numeric_limits<int>::max();
    for (auto const & binding: input_bindings) {
        if (dl::widget::isImageEncoder(binding.encoder) && !binding.data_key.empty()) {
            std::shared_ptr<MediaData> media;
            if (media_overrides) {
                auto ov_it = media_overrides->find(binding.data_key);
                if (ov_it != media_overrides->end()) media = ov_it->second;
            }
            if (!media) media = dm.getData<MediaData>(binding.data_key);
            if (media) {
                int const total = media->getTotalFrameCount();
                if (total > 0) {
                    max_frame = total - 1;
                }
                break;
            }
        }
    }

    // ── Dynamic (per-frame) inputs ──
    for (auto const & binding: input_bindings) {
        auto const * slot = findSlot(input_slot_vec, binding.slot_name);
        if (!slot || binding.data_key.empty()) continue;

        std::vector<int64_t> tensor_shape;
        tensor_shape.push_back(batch_size);
        tensor_shape.insert(tensor_shape.end(),
                            slot->shape.begin(), slot->shape.end());

        // Use dtype from slot descriptor (model specifies expected dtype)
        auto tensor = at::zeros(tensor_shape, toTorchDType(slot->dtype));
        for (int b = 0; b < batch_size; ++b) {
            int const frame = computeEncodingFrame(
                    current_frame, b, binding.time_offset, max_frame);
            encodeDynamicSlot(dm, binding, *slot, tensor,
                              frame, b, source_image_size,
                              media_overrides);
        }
        result[binding.slot_name] = std::move(tensor);
    }

    // ── Static (memory) inputs ──
    std::unordered_map<std::string, std::vector<StaticInputData const *>> grouped;
    for (auto const & si: static_inputs) {
        grouped[si.slot_name].push_back(&si);
    }

    // ── Build recurrent-claimed position map for hybrid mode ──
    auto const claimed = recurrentClaimedPositions(recurrent_bindings);

    // Process all static/memory input slots (including boolean masks)
    for (auto const & slot: input_slot_vec) {
        if (!slot.is_static) continue;// skip dynamic inputs

        auto const & entries = grouped[slot.name];// may be empty

        // Positions in this slot claimed by recurrent bindings
        std::set<int> const * slot_claimed = nullptr;
        {
            auto it = claimed.find(slot.name);
            if (it != claimed.end()) slot_claimed = &it->second;
        }

        if (slot.is_boolean_mask) {
            // Boolean mask: always create, even if no entries
            std::vector<int64_t> shape = {batch_size};
            for (auto d: slot.shape) shape.push_back(d);
            auto tensor = at::zeros(shape, toTorchDType(slot.dtype));

            // Determine which dimension the memory_index indexes into.
            // For boolean masks with sequence_dim, use sequence_dim + 1
            // (accounting for the batch dim). Otherwise, use dim 1 (first
            // shape dimension).
            int const flag_dim = slot.hasSequenceDim()
                                         ? slot.sequence_dim + 1
                                         : 1;
            int64_t const flag_extent = slot.shape[slot.hasSequenceDim()
                                                           ? static_cast<std::size_t>(slot.sequence_dim)
                                                           : 0];

            for (auto const * entry: entries) {
                if (entry->active &&
                    entry->memory_index < static_cast<int>(flag_extent)) {
                    // Set the flag for all batch elements
                    for (int b = 0; b < batch_size; ++b) {
                        tensor[b].select(flag_dim - 1, entry->memory_index) = 1.0f;
                    }
                }
            }
            result[slot.name] = std::move(tensor);
        } else if (!entries.empty()) {
            // Memory frame slots: pull from DataManager at time offsets
            // Only create if user has configured at least one entry
            std::vector<int64_t> tensor_shape = {batch_size};
            tensor_shape.insert(tensor_shape.end(),
                                slot.shape.begin(), slot.shape.end());
            // Use dtype from slot descriptor
            auto tensor = at::zeros(tensor_shape, toTorchDType(slot.dtype));

            if (slot.hasSequenceDim()) {
                // ── Sequence slot: place each entry at its memory_index ──
                // along the sequence axis. The full tensor dimension
                // for the sequence is sequence_dim + 1 (batch is dim 0).
                int const full_seq_dim = slot.sequence_dim + 1;
                auto const seq_len = sequenceLength(slot);
                auto const elem_shape = perElementShape(slot);

                // Build a temporary descriptor with per-element shape for encoding
                dl::TensorSlotDescriptor elem_slot = slot;
                elem_slot.shape = elem_shape;
                elem_slot.sequence_dim = -1;

                // Track which positions have been filled for gap warnings
                int filled_count = 0;

                // Count recurrent-claimed positions as "will be filled"
                if (slot_claimed) {
                    filled_count += static_cast<int>(slot_claimed->size());
                }

                for (auto const * entry: entries) {
                    if (!entry->hasActiveSource()) {
                        continue;
                    }
                    if (entry->memory_index >= static_cast<int>(seq_len)) {
                        std::cerr << "SlotAssembler: memory_index "
                                  << entry->memory_index
                                  << " exceeds sequence length "
                                  << seq_len << " for slot '"
                                  << slot.name << "'\n";
                        continue;
                    }

                    // Skip positions claimed by recurrent bindings
                    if (slot_claimed &&
                        slot_claimed->contains(entry->memory_index)) {
                        std::cerr << "SlotAssembler: position "
                                  << entry->memory_index
                                  << " in slot '" << slot.name
                                  << "' is claimed by both static and "
                                     "recurrent sources; skipping static\n";
                        continue;
                    }

                    // Get the target slice: tensor[:, ..., memory_index, ..., :]
                    // where memory_index is along full_seq_dim
                    auto target_slice = tensor.select(full_seq_dim, entry->memory_index);

                    if (entry->sourceType() == StaticInputSource::DataBank) {
                        if (auto cached = getBankCachedTensor(
                                    *entry, data_bank, batch_size)) {
                            target_slice.copy_(*cached);
                            ++filled_count;
                            continue;
                        }
                        continue;
                    }

                    std::vector<int64_t> temp_shape = {1};
                    temp_shape.insert(temp_shape.end(),
                                      elem_shape.begin(), elem_shape.end());
                    auto temp = at::zeros(temp_shape, toTorchDType(slot.dtype));

                    int const frame = computeEncodingFrame(
                            current_frame, 0, entry->time_offset, max_frame);
                    if (!encodeStaticEntryToTensor(
                                dm, *entry, elem_slot, data_bank, temp, frame,
                                source_image_size, media_overrides)) {
                        continue;
                    }

                    target_slice.copy_(expandLeadingBatchDim(temp, batch_size));
                    ++filled_count;
                }

                // Warn if not all sequence positions are filled
                if (filled_count < static_cast<int>(seq_len)) {
                    std::cerr << "SlotAssembler: slot '" << slot.name
                              << "' has " << filled_count << " of "
                              << seq_len
                              << " sequence positions filled; unfilled "
                                 "positions are zero-padded\n";
                }
                // For hybrid slots: also create the tensor when there are
                // only recurrent-claimed positions but no static entries.
            } else if (entries.empty() && slot_claimed && !slot_claimed->empty()) {
                // Hybrid-only: create a zero tensor for the slot so that
                // recurrent injection has a target tensor to fill into.
                std::vector<int64_t> hybrid_tensor_shape = {batch_size};
                hybrid_tensor_shape.insert(hybrid_tensor_shape.end(),
                                           slot.shape.begin(), slot.shape.end());
                result[slot.name] = at::zeros(hybrid_tensor_shape,
                                              toTorchDType(slot.dtype));
            } else {
                // ── Non-sequence static slot (original behavior) ──
                for (auto const * entry: entries) {
                    if (!entry->hasActiveSource()) {
                        continue;
                    }

                    if (entry->sourceType() == StaticInputSource::DataBank) {
                        if (auto cached = getBankCachedTensor(
                                    *entry, data_bank, batch_size)) {
                            tensor.copy_(*cached);
                            continue;
                        }
                        continue;
                    }

                    int const frame = computeEncodingFrame(
                            current_frame, 0, entry->time_offset, max_frame);
                    if (!encodeStaticEntryToTensor(
                                dm, *entry, slot, data_bank, tensor, frame,
                                source_image_size, media_overrides)) {
                        continue;
                    }
                }
            }
            result[slot.name] = std::move(tensor);
        }
    }

    return result;
}

/// @brief Build decoder context from slot metadata and runtime state.
[[nodiscard]] dl::DecoderContext makeDecoderContext(
        dl::TensorSlotDescriptor const & slot,
        ImageSize source_image_size,
        int batch_index) {
    dl::DecoderContext ctx;
    ctx.source_channel = 0;
    ctx.batch_index = batch_index;
    ctx.target_image_size = source_image_size;

    if (slot.shape.size() >= 2) {
        ctx.height = static_cast<int>(slot.shape[slot.shape.size() - 2]);
        ctx.width = static_cast<int>(slot.shape[slot.shape.size() - 1]);
    }
    return ctx;
}

/**
 * @brief Decode one output binding when spatial rank requirements are satisfied.
 *
 * @returns Decoded geometry, or nullopt when skipped due to rank mismatch.
 */
[[nodiscard]] std::optional<dl::DecodedGeometryVariant> decodeBindingGeometry(
        OutputBindingData const & binding,
        at::Tensor const & tensor,
        dl::DecoderContext const & ctx,
        char const * log_prefix) {
    std::optional<dl::DecodedGeometryVariant> decoded;
    bool spatial_skip = false;

    binding.decoder.visit([&](auto const & params) {
        using ParamsT = std::decay_t<decltype(params)>;
        if constexpr (dl::isSpatialDecoderParams<ParamsT>()) {
            if (tensor.dim() < 4) {
                spatial_skip = true;
                std::cerr << log_prefix << ": spatial decoder '"
                          << dl::decoderFactoryName<ParamsT>()
                          << "' requires 4D tensor [B,C,H,W], got dim="
                          << tensor.dim()
                          << " (post-encoder reduces rank). Skipping.\n";
                return;
            }
        }
        decoded = dl::decodeToGeometry(tensor, ctx, params);
    });

    if (spatial_skip) {
        return std::nullopt;
    }
    return decoded;
}

/// @brief Verify the output data key is configured and already present in DataManager.
void assertOutputDataKeyReady(
        DataManager & dm,
        std::string const & data_key) {
    assert(!data_key.empty() &&
           "writeDecodedGeometryToDataManager: data_key must be set before inference");
    assert(dm.getDataVariant(data_key).has_value() &&
           "writeDecodedGeometryToDataManager: data_key must exist in DataManager");
}

/**
 * @brief Write decoded geometry into DataManager at the given frame.
 *
 * @pre data_key is non-empty and refers to an existing object of the matching type.
 */
void writeDecodedGeometryToDataManager(
        DataManager & dm,
        std::string const & data_key,
        TimeFrameIndex const frame_idx,
        dl::DecodedGeometryVariant decoded) {
    assertOutputDataKeyReady(dm, data_key);

    std::visit(
            [&](auto && geometry) {
                using GeometryT = std::decay_t<decltype(geometry)>;
                if constexpr (std::is_same_v<GeometryT, Mask2D>) {
                    if (geometry.empty()) {
                        return;
                    }
                    dm.getData<MaskData>(data_key)->addAtTime(
                            frame_idx,
                            std::forward<decltype(geometry)>(geometry),
                            NotifyObservers::Yes);
                } else if constexpr (std::is_same_v<GeometryT, Point2D<float>>) {
                    dm.getData<PointData>(data_key)->addAtTime(
                            frame_idx,
                            std::forward<decltype(geometry)>(geometry),
                            NotifyObservers::Yes);
                } else if constexpr (std::is_same_v<GeometryT, Line2D>) {
                    if (geometry.empty()) {
                        return;
                    }
                    dm.getData<LineData>(data_key)->addAtTime(
                            frame_idx,
                            std::forward<decltype(geometry)>(geometry),
                            NotifyObservers::Yes);
                } else if constexpr (std::is_same_v<GeometryT, std::vector<float>>) {
                    if (geometry.empty()) {
                        return;
                    }
                    auto time_frame = dm.getTime();
                    std::vector<std::pair<int, std::vector<float>>> rows;
                    rows.emplace_back(
                            static_cast<int>(frame_idx.getValue()),
                            std::forward<decltype(geometry)>(geometry));
                    dm.getData<TensorData>(data_key)->upsertRows(
                            rows, std::move(time_frame));
                }
            },
            std::move(decoded));
}

/// @brief Convert decoded geometry to a FrameResult when non-empty.
[[nodiscard]] std::optional<FrameResult> toFrameResult(
        int current_frame,
        std::string const & data_key,
        dl::DecodedGeometryVariant decoded) {
    return std::visit(
            [&](auto && geometry) -> std::optional<FrameResult> {
                using GeometryT = std::decay_t<decltype(geometry)>;
                if constexpr (std::is_same_v<GeometryT, Point2D<float>>) {
                    return FrameResult{
                            current_frame,
                            geometry,
                            data_key};
                } else {
                    if (geometry.empty()) {
                        return std::nullopt;
                    }
                    return FrameResult{
                            current_frame,
                            std::forward<decltype(geometry)>(geometry),
                            data_key};
                }
            },
            std::move(decoded));
}

/// Decode model output tensors and write results into DataManager.
///
/// @pre current_frame >= 0; a negative value is stored as a negative-indexed
///      key in the data objects, producing silently mislabeled output frames;
///      all callers derive current_frame from start_frame >= 0
///      (enforcement: none) [IMPORTANT]
/// @pre batch_index >= 0; it is passed to decoder internals as a tensor
///      batch-dimension index, causing out-of-bounds tensor access if negative;
///      all callers pass a loop variable b >= 0 or the default 0
///      (enforcement: none) [CRITICAL]
void decodeOutputs(
        DataManager & dm,
        std::unordered_map<std::string, at::Tensor> const & outputs,
        std::vector<OutputBindingData> const & output_bindings,
        std::vector<dl::TensorSlotDescriptor> const & output_slots,
        int current_frame,
        ImageSize source_image_size,
        int batch_index = 0) {

    TimeFrameIndex const frame_idx(current_frame);

    for (auto const & binding: output_bindings) {
        if (binding.data_key.empty()) continue;
        auto it = outputs.find(binding.slot_name);
        if (it == outputs.end()) continue;

        auto const * slot = findSlot(output_slots, binding.slot_name);
        if (!slot) continue;

        auto const ctx =
                makeDecoderContext(*slot, source_image_size, batch_index);
        auto decoded = decodeBindingGeometry(
                binding, it->second, ctx, "SlotAssembler");
        if (!decoded) continue;

        writeDecodedGeometryToDataManager(
                dm, binding.data_key, frame_idx, std::move(*decoded));
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Offline (worker-thread) helpers
// ────────────────────────────────────────────────────────────────────────────

/// Decode model outputs into FrameResult entries instead of writing to
/// DataManager. Returns a vector of decoded results for the given frame.
///
/// @pre current_frame >= 0; a negative value is stored directly in
///      FrameResult.frame, silently mislabeling the result frame;
///      the sole caller derives current_frame from chunk_start >= 0
///      (enforcement: none) [IMPORTANT]
/// @pre batch_index >= 0; it is passed to decoder internals as a tensor
///      batch-dimension index, causing out-of-bounds tensor access if negative;
///      the sole caller passes loop variable b >= 0
///      (enforcement: none) [CRITICAL]
std::vector<FrameResult> decodeOutputsToBuffer(
        std::unordered_map<std::string, at::Tensor> const & outputs,
        std::vector<OutputBindingData> const & output_bindings,
        std::vector<dl::TensorSlotDescriptor> const & output_slots,
        int current_frame,
        ImageSize source_image_size,
        int batch_index = 0) {

    std::vector<FrameResult> frame_results;

    for (auto const & binding: output_bindings) {
        if (binding.data_key.empty()) continue;
        auto it = outputs.find(binding.slot_name);
        if (it == outputs.end()) continue;

        auto const * slot = findSlot(output_slots, binding.slot_name);
        if (!slot) continue;

        auto const ctx =
                makeDecoderContext(*slot, source_image_size, batch_index);
        auto decoded = decodeBindingGeometry(
                binding, it->second, ctx, "SlotAssembler(offline)");
        if (!decoded) continue;

        if (auto frame_result =
                    toFrameResult(current_frame, binding.data_key, std::move(*decoded))) {
            frame_results.push_back(std::move(*frame_result));
        }
    }

    return frame_results;
}

}// anonymous namespace

// ════════════════════════════════════════════════════════════════════════════
// Construction / destruction / move
// ════════════════════════════════════════════════════════════════════════════

SlotAssembler::SlotAssembler()
    : _impl(std::make_unique<Impl>()) {
    _impl->data_bank = std::make_unique<dl::DataBank>();
}

SlotAssembler::~SlotAssembler() = default;

SlotAssembler::SlotAssembler(SlotAssembler &&) noexcept = default;
SlotAssembler & SlotAssembler::operator=(SlotAssembler &&) noexcept = default;

// ════════════════════════════════════════════════════════════════════════════
// Instance: model lifecycle
// ════════════════════════════════════════════════════════════════════════════

bool SlotAssembler::loadModel(std::string const & model_id) {
    _impl->model.reset();
    _impl->model_id.clear();
    _impl->post_encoder_module.reset();

    if (model_id.empty()) return false;

    auto model = dl::ModelRegistry::instance().create(model_id);
    if (!model) return false;

    _impl->model = std::move(model);
    _impl->model_id = model_id;
    return true;
}

bool SlotAssembler::loadWeights(std::string const & weights_path) {
    if (!_impl->model) return false;
    if (weights_path.empty()) return false;

    std::filesystem::path const p(weights_path);
    if (!std::filesystem::exists(p)) return false;

    try {
        _impl->model->loadWeights(p);
        return true;
    } catch (std::exception const & e) {
        std::cerr << "SlotAssembler::loadWeights: " << e.what() << '\n';
        return false;
    }
}

std::string SlotAssembler::validateWeights() {
    if (!isModelReady()) {
        return "Model not loaded or weights not applied";
    }

    auto const input_slots = _impl->model->inputSlots();
    std::unordered_map<std::string, at::Tensor> dummy_inputs;
    dummy_inputs.reserve(input_slots.size());

    for (auto const & slot: input_slots) {
        std::vector<int64_t> shape;
        shape.reserve(slot.shape.size() + 1);
        shape.push_back(1);// batch_size = 1
        shape.insert(shape.end(), slot.shape.begin(), slot.shape.end());
        dummy_inputs[slot.name] =
                at::zeros(shape, dl::toTorchDType(slot.dtype));
    }

    try {
        torch::NoGradGuard const no_grad;
        auto const outputs = runModelAndPostEncoder(
                *_impl->model,
                _impl->post_encoder_module.get(),
                dummy_inputs);
        auto const effective_slots = effectiveOutputSlotsFor(
                _impl->model.get(),
                _impl->post_encoder_module.get());

        // Verify all expected output slots are present and non-empty.
        for (auto const & expected: effective_slots) {
            auto it = outputs.find(expected.name);
            if (it == outputs.end()) {
                return "Missing output slot '" + expected.name + "'";
            }
            if (it->second.numel() == 0) {
                return "Output slot '" + expected.name + "' returned empty tensor";
            }
        }
        return {};// success
    } catch (std::exception const & e) {
        return e.what();
    } catch (...) {
        return "Unknown error during weight validation";
    }
}

bool SlotAssembler::isModelReady() const {
    return _impl->model && _impl->model->isReady();
}

std::string const & SlotAssembler::currentModelId() const {
    return _impl->model_id;
}

void SlotAssembler::resetModel() {
    _impl->model.reset();
    _impl->model_id.clear();
    _impl->post_encoder_module.reset();
    _impl->recurrent_cache.clear();
}

dl::DataBank & SlotAssembler::dataBank() {
    return *_impl->data_bank;
}

dl::DataBank const & SlotAssembler::dataBank() const {
    return *_impl->data_bank;
}

void SlotAssembler::clearDataBank() {
    _impl->data_bank->clear();
}

// ════════════════════════════════════════════════════════════════════════════
// Device context
// ════════════════════════════════════════════════════════════════════════════

bool SlotAssembler::isCudaAvailable() {
    return dl::DeviceManager::cudaAvailable();
}

std::string SlotAssembler::currentDeviceName() {
    auto const & dev = dl::DeviceManager::instance().device();
    if (dev.type() == at::kCUDA) {
        return "GPU (CUDA)";
    }
    return "CPU";
}

void SlotAssembler::setDeviceByName(std::string const & name) {
    if (name == "cuda") {
        dl::DeviceManager::instance().setDevice(at::Device(at::kCUDA));
    } else {
        dl::DeviceManager::instance().setDevice(at::Device(at::kCPU));
    }
}

void SlotAssembler::initDeviceForCurrentThread() {
    auto const & dev = dl::DeviceManager::instance().device();
    if (dev.type() != at::kCPU) {
        // Allocating a small tensor on the target device forces the CUDA
        // runtime to initialize its per-thread context.
        (void) at::zeros({1}, at::TensorOptions().device(dev));
    }
}

bool SlotAssembler::captureToBank(
        DataManager & dm,
        std::string const & bank_entry_id,
        StaticInputData const & entry,
        int frame,
        ImageSize source_image_size) {

    if (!_impl->model) {
        return false;
    }
    if (entry.data_key.empty()) {
        return false;
    }
    if (auto const err = dl::validateEntryId(bank_entry_id)) {
        spdlog::error("SlotAssembler::captureToBank: {}", *err);
        return false;
    }

    auto const input_slot_vec = _impl->model->inputSlots();
    auto const * slot = findSlot(input_slot_vec, entry.slot_name);
    if (!slot || !slot->is_static) {
        return false;
    }

    std::vector<int64_t> encode_shape;
    if (slot->hasSequenceDim()) {
        encode_shape = perElementShape(*slot);
    } else {
        encode_shape = slot->shape;
    }

    dl::TensorSlotDescriptor encode_slot = *slot;
    encode_slot.shape = encode_shape;
    encode_slot.sequence_dim = -1;

    SlotBindingData temp_binding;
    temp_binding.slot_name = slot->name;
    temp_binding.data_key = entry.data_key;
    dl::widget::assignEncoderFromFactoryName(
            temp_binding.encoder, slot->recommended_encoder);

    auto const source = fetchEncodingSource(dm, temp_binding, frame);
    if (!source) {
        return false;
    }

    dl::DataBankEntryMetadata metadata;
    metadata.data_key = entry.data_key;
    metadata.captured_frame = frame;
    metadata.source_image_size = source_image_size;
    metadata.encoder_factory_name = slot->recommended_encoder;

    if (!_impl->data_bank->putSource(bank_entry_id, *source, metadata)) {
        return false;
    }

    auto const encoder_params =
            dl::encoderParamsFromFactoryName(slot->recommended_encoder);
    if (!encoder_params) {
        spdlog::error(
                "SlotAssembler::captureToBank: unknown encoder '{}' for slot '{}'",
                slot->recommended_encoder,
                entry.slot_name);
        return false;
    }

    return _impl->data_bank->encodeEntry(
            bank_entry_id, encode_slot, *encoder_params);
}

bool SlotAssembler::captureStaticInput(
        DataManager & dm,
        StaticInputData const & entry,
        int frame,
        ImageSize source_image_size) {

    std::string const bank_id = entry.resolvedBankEntryId();
    if (bank_id.empty()) {
        return false;
    }

    return captureToBank(dm, bank_id, entry, frame, source_image_size);
}

// ════════════════════════════════════════════════════════════════════════════
// Instance: inference
// ════════════════════════════════════════════════════════════════════════════

void SlotAssembler::runSingleFrame(
        DataManager & dm,
        std::vector<SlotBindingData> const & input_bindings,
        std::vector<StaticInputData> const & static_inputs,
        std::vector<OutputBindingData> const & output_bindings,
        int current_frame,
        ImageSize source_image_size) {

    if (!isModelReady()) {
        throw std::runtime_error("SlotAssembler::runSingleFrame: "
                                 "model not loaded or weights missing");
    }

    torch::NoGradGuard const no_grad;

    _updateSpatialPoint(dm, current_frame);

    auto inputs = assembleInputs(
            dm, *_impl->model,
            input_bindings, static_inputs,
            *_impl->data_bank,
            {},// no recurrent bindings in single-frame mode
            current_frame, /*batch_size=*/1);

    auto const effective_slots = effectiveOutputSlotsFor(
            _impl->model.get(),
            _impl->post_encoder_module.get());
    auto outputs = runModelAndPostEncoder(
            *_impl->model,
            _impl->post_encoder_module.get(),
            inputs);

    decodeOutputs(
            dm, outputs, output_bindings,
            effective_slots, current_frame, source_image_size);
}

// ════════════════════════════════════════════════════════════════════════════
// Instance: batch range inference
// ════════════════════════════════════════════════════════════════════════════

void SlotAssembler::runBatchRange(
        DataManager & dm,
        std::vector<SlotBindingData> const & input_bindings,
        std::vector<StaticInputData> const & static_inputs,
        std::vector<OutputBindingData> const & output_bindings,
        int start_frame,
        int end_frame,
        ImageSize source_image_size,
        int batch_size,
        ProgressCallback const & progress) {

    if (!isModelReady()) {
        throw std::runtime_error("SlotAssembler::runBatchRange: "
                                 "model not loaded or weights missing");
    }

    torch::NoGradGuard const no_grad;

    if (end_frame < start_frame) {
        throw std::runtime_error("SlotAssembler::runBatchRange: "
                                 "end_frame must be >= start_frame");
    }
    if (batch_size < 1) batch_size = 1;

    if (requiresSingleFrameBatch(_impl->post_encoder_module.get())) {
        batch_size = 1;
    }

    auto const effective_slots = effectiveOutputSlotsFor(
            _impl->model.get(),
            _impl->post_encoder_module.get());
    int const total_frames = end_frame - start_frame + 1;
    int frames_processed = 0;

    for (int chunk_start = start_frame; chunk_start <= end_frame;
         chunk_start += batch_size) {

        int const chunk_end = std::min(chunk_start + batch_size - 1, end_frame);
        int const chunk_size = chunk_end - chunk_start + 1;

        if (progress) {
            progress(frames_processed, total_frames);
        }

        _updateSpatialPoint(dm, chunk_start);

        auto inputs = assembleInputs(
                dm, *_impl->model,
                input_bindings, static_inputs,
                *_impl->data_bank,
                {},// no recurrent bindings
                chunk_start, /*batch_size=*/chunk_size);

        auto outputs = runModelAndPostEncoder(
                *_impl->model,
                _impl->post_encoder_module.get(),
                inputs);

        // Decode each batch element individually
        for (int b = 0; b < chunk_size; ++b) {
            int const frame = chunk_start + b;
            decodeOutputs(
                    dm, outputs, output_bindings,
                    effective_slots, frame, source_image_size, b);
        }

        frames_processed += chunk_size;
    }

    if (progress) {
        progress(total_frames, total_frames);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Instance: offline batch range inference (worker-thread safe)
// ════════════════════════════════════════════════════════════════════════════

BatchInferenceResult SlotAssembler::runBatchRangeOffline(
        DataManager & dm,
        MediaOverrides const & media_overrides,
        std::vector<SlotBindingData> const & input_bindings,
        std::vector<StaticInputData> const & static_inputs,
        std::vector<OutputBindingData> const & output_bindings,
        int start_frame,
        int end_frame,
        ImageSize source_image_size,
        std::atomic<bool> const & cancel_requested,
        int batch_size,
        ProgressCallback const & progress,
        ResultCallback const & result_callback) {

    BatchInferenceResult batch_result;

    if (!isModelReady()) {
        batch_result.success = false;
        batch_result.error_message =
                "SlotAssembler::runBatchRangeOffline: "
                "model not loaded or weights missing";
        return batch_result;
    }

    torch::NoGradGuard const no_grad;

    if (end_frame < start_frame) {
        batch_result.success = false;
        batch_result.error_message =
                "SlotAssembler::runBatchRangeOffline: "
                "end_frame must be >= start_frame";
        return batch_result;
    }
    if (batch_size < 1) batch_size = 1;

    if (requiresSingleFrameBatch(_impl->post_encoder_module.get())) {
        batch_size = 1;
    }

    auto const effective_slots = effectiveOutputSlotsFor(
            _impl->model.get(),
            _impl->post_encoder_module.get());
    int const total_frames = end_frame - start_frame + 1;
    int frames_processed = 0;

    for (int chunk_start = start_frame; chunk_start <= end_frame;
         chunk_start += batch_size) {

        if (cancel_requested.load(std::memory_order_relaxed)) {
            break;
        }

        int const chunk_end = std::min(chunk_start + batch_size - 1, end_frame);
        int const chunk_size = chunk_end - chunk_start + 1;

        if (progress) {
            progress(frames_processed, total_frames);
        }

        try {
            _updateSpatialPoint(dm, chunk_start);

            auto inputs = assembleInputs(
                    dm, *_impl->model,
                    input_bindings, static_inputs,
                    *_impl->data_bank,
                    {},// no recurrent bindings
                    chunk_start, /*batch_size=*/chunk_size,
                    &media_overrides);

            auto outputs = runModelAndPostEncoder(
                    *_impl->model,
                    _impl->post_encoder_module.get(),
                    inputs);

            // Decode each batch element individually
            for (int b = 0; b < chunk_size; ++b) {
                int const frame = chunk_start + b;
                auto frame_results = decodeOutputsToBuffer(
                        outputs, output_bindings,
                        effective_slots, frame, source_image_size, b);

                if (result_callback) {
                    result_callback(std::move(frame_results));
                } else {
                    batch_result.results.insert(
                            batch_result.results.end(),
                            std::make_move_iterator(frame_results.begin()),
                            std::make_move_iterator(frame_results.end()));
                }
            }
        } catch (std::exception const & e) {
            batch_result.success = false;
            batch_result.error_message = e.what();
            break;
        }

        frames_processed += chunk_size;
    }

    if (progress) {
        progress(total_frames, total_frames);
    }

    return batch_result;
}

// ════════════════════════════════════════════════════════════════════════════
// Instance: recurrent tensor cache
// ════════════════════════════════════════════════════════════════════════════

void SlotAssembler::clearRecurrentCache() {
    _impl->recurrent_cache.clear();
}

// ════════════════════════════════════════════════════════════════════════════
// Instance: recurrent sequence inference
// ════════════════════════════════════════════════════════════════════════════

void SlotAssembler::runRecurrentSequence(
        DataManager & dm,
        std::vector<SlotBindingData> const & input_bindings,
        std::vector<StaticInputData> const & static_inputs,
        std::vector<OutputBindingData> const & output_bindings,
        std::vector<RecurrentBindingData> const & recurrent_bindings,
        int start_frame,
        int frame_count,
        ImageSize source_image_size,
        ProgressCallback const & progress) {

    if (!isModelReady()) {
        throw std::runtime_error("SlotAssembler::runRecurrentSequence: "
                                 "model not loaded or weights missing");
    }

    torch::NoGradGuard const no_grad;

    if (recurrent_bindings.empty()) {
        throw std::runtime_error("SlotAssembler::runRecurrentSequence: "
                                 "no recurrent bindings provided");
    }

    auto const input_slot_vec = _impl->model->inputSlots();
    auto const output_slot_vec = _impl->model->outputSlots();

    // ── t=0 initialization: populate recurrent cache ──
    for (auto const & rb: recurrent_bindings) {
        auto const key = recurrentCacheKey(rb.input_slot_name);
        auto const mode = rb.initMode();

        auto const * input_slot = findSlot(input_slot_vec, rb.input_slot_name);
        if (!input_slot) {
            throw std::runtime_error(
                    "SlotAssembler::runRecurrentSequence: unknown input slot '" + rb.input_slot_name + "'");
        }

        // For hybrid mode (target_memory_index >= 0), the init tensor is
        // per-element (excluding sequence dim) rather than whole-slot.
        std::vector<int64_t> init_shape = {1};
        if (rb.hasTargetMemoryIndex() && input_slot->hasSequenceDim()) {
            // Per-element shape for the recurrent position
            auto const elem = perElementShape(*input_slot);
            init_shape.insert(init_shape.end(), elem.begin(), elem.end());
        } else {
            init_shape.insert(init_shape.end(),
                              input_slot->shape.begin(),
                              input_slot->shape.end());
        }

        if (mode == RecurrentInitMode::Zeros) {
            _impl->recurrent_cache[key] =
                    at::zeros(init_shape, toTorchDType(input_slot->dtype));

        } else if (mode == RecurrentInitMode::StaticCapture) {
            int const mem_idx = rb.hasTargetMemoryIndex()
                                        ? rb.target_memory_index
                                        : 0;

            std::optional<at::Tensor> init_tensor;
            if (auto const * static_entry = findStaticInputEntry(
                        static_inputs, rb.input_slot_name, mem_idx)) {
                if (static_entry->sourceType() == StaticInputSource::DataBank) {
                    init_tensor = getBankCachedTensor(
                            *static_entry, *_impl->data_bank, /*batch_size=*/1);
                }
            }
            if (!init_tensor) {
                auto const bank_id = defaultBankEntryId(rb.input_slot_name, mem_idx);
                init_tensor = _impl->data_bank->getEncoded(bank_id);
            }

            if (init_tensor) {
                _impl->recurrent_cache[key] = init_tensor->clone();
            } else if (!rb.init_data_key.empty() && rb.init_frame >= 0) {
                StaticInputData temp_entry;
                temp_entry.slot_name = rb.input_slot_name;
                temp_entry.memory_index = mem_idx;
                temp_entry.data_key = rb.init_data_key;
                temp_entry.setSourceType(StaticInputSource::DataBank);

                if (captureStaticInput(dm, temp_entry, rb.init_frame,
                                       source_image_size)) {
                    if (auto encoded = _impl->data_bank->getEncoded(
                                defaultBankEntryId(rb.input_slot_name, mem_idx))) {
                        _impl->recurrent_cache[key] = encoded->clone();
                    }
                }
            }

            if (!_impl->recurrent_cache.contains(key)) {
                spdlog::warn(
                        "SlotAssembler: StaticCapture init failed for '{}', "
                        "falling back to zeros",
                        rb.input_slot_name);
                _impl->recurrent_cache[key] =
                        at::zeros(init_shape, toTorchDType(input_slot->dtype));
            }

        } else if (mode == RecurrentInitMode::FirstOutput) {
            // Will be filled after the first forward pass below
            _impl->recurrent_cache[key] =
                    at::zeros(init_shape, toTorchDType(input_slot->dtype));
        }
    }

    // ── Validate hybrid bindings ──
    for (auto const & rb: recurrent_bindings) {
        if (!rb.hasTargetMemoryIndex()) continue;

        auto const * input_slot = findSlot(input_slot_vec, rb.input_slot_name);
        if (!input_slot || !input_slot->hasSequenceDim()) {
            std::cerr << "SlotAssembler: recurrent binding for '"
                      << rb.input_slot_name
                      << "' has target_memory_index="
                      << rb.target_memory_index
                      << " but the slot has no sequence dimension\n";
            continue;
        }

        auto const seq_len = sequenceLength(*input_slot);
        if (rb.target_memory_index >= static_cast<int>(seq_len)) {
            std::cerr << "SlotAssembler: recurrent binding target_memory_index "
                      << rb.target_memory_index
                      << " exceeds sequence length " << seq_len
                      << " for slot '" << rb.input_slot_name << "'\n";
        }

        // Validate shape compatibility with output slot
        auto const * output_slot = findSlot(output_slot_vec, rb.output_slot_name);
        if (output_slot) {
            auto const elem = perElementShape(*input_slot);
            if (output_slot->shape != elem) {
                std::cerr << "SlotAssembler: shape mismatch — output slot '"
                          << rb.output_slot_name << "' shape does not match "
                          << "per-element shape of input slot '"
                          << rb.input_slot_name << "' at position "
                          << rb.target_memory_index << "\n";
            }
        }
    }

    // ── Sequential frame loop ──
    for (int f = 0; f < frame_count; ++f) {
        int const frame = start_frame + f;

        if (progress) {
            progress(f, frame_count);
        }

        _updateSpatialPoint(dm, frame);

        // 1. Assemble dynamic + static inputs (batch_size = 1)
        //    In hybrid mode, assembleInputs skips recurrent-claimed positions.
        auto inputs = assembleInputs(
                dm, *_impl->model,
                input_bindings, static_inputs,
                *_impl->data_bank,
                recurrent_bindings,
                frame, /*batch_size=*/1);

        // 2. Inject recurrent tensors into the input map
        for (auto const & rb: recurrent_bindings) {
            auto const key = recurrentCacheKey(rb.input_slot_name);
            auto cache_it = _impl->recurrent_cache.find(key);
            if (cache_it == _impl->recurrent_cache.end()) continue;

            if (rb.hasTargetMemoryIndex()) {
                // Hybrid mode: inject into specific sequence position
                auto const * input_slot = findSlot(input_slot_vec,
                                                   rb.input_slot_name);
                if (!input_slot || !input_slot->hasSequenceDim()) continue;

                auto slot_it = inputs.find(rb.input_slot_name);
                if (slot_it == inputs.end()) continue;

                int const full_seq_dim = input_slot->sequence_dim + 1;
                auto target_slice = slot_it->second.select(
                        full_seq_dim, rb.target_memory_index);
                target_slice.copy_(cache_it->second);
            } else {
                // Whole-slot replacement (original Phase 4 behavior)
                inputs[rb.input_slot_name] = cache_it->second;
            }
        }

        // 3. Forward pass and post-encoder
        auto outputs = runModelAndPostEncoder(
                *_impl->model,
                _impl->post_encoder_module.get(),
                inputs);
        auto const effective_slots = effectiveOutputSlotsFor(
                _impl->model.get(),
                _impl->post_encoder_module.get());

        // 4. For FirstOutput mode on t=0: use raw output, skip decoding
        if (f == 0) {
            bool has_first_output = false;
            for (auto const & rb: recurrent_bindings) {
                if (rb.initMode() == RecurrentInitMode::FirstOutput) {
                    has_first_output = true;
                    break;
                }
            }
            if (has_first_output) {
                // Cache outputs for recurrent slots that use FirstOutput
                for (auto const & rb: recurrent_bindings) {
                    if (rb.initMode() == RecurrentInitMode::FirstOutput) {
                        auto out_it = outputs.find(rb.output_slot_name);
                        if (out_it != outputs.end()) {
                            auto const rkey = recurrentCacheKey(rb.input_slot_name);
                            _impl->recurrent_cache[rkey] = out_it->second.detach();
                        }
                    }
                }
                // Skip decoding for t=0 in FirstOutput mode — the output
                // is just used as initialization, not as a real prediction
                // Re-carry non-FirstOutput recurrent outputs
                for (auto const & rb: recurrent_bindings) {
                    if (rb.initMode() != RecurrentInitMode::FirstOutput) {
                        auto out_it = outputs.find(rb.output_slot_name);
                        if (out_it != outputs.end()) {
                            auto const rkey = recurrentCacheKey(rb.input_slot_name);
                            _impl->recurrent_cache[rkey] = out_it->second.detach();
                        }
                    }
                }
                continue;// Skip to next frame
            }
        }

        // 5. Decode outputs into DataManager
        decodeOutputs(
                dm, outputs, output_bindings,
                effective_slots, frame, source_image_size);

        // 6. Cache output tensors for next frame's recurrent inputs
        for (auto const & rb: recurrent_bindings) {
            auto out_it = outputs.find(rb.output_slot_name);
            if (out_it != outputs.end()) {
                auto const rkey = recurrentCacheKey(rb.input_slot_name);
                _impl->recurrent_cache[rkey] = out_it->second.detach();
            }
        }
    }

    // Final progress callback
    if (progress) {
        progress(frame_count, frame_count);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Static: registry queries
// ════════════════════════════════════════════════════════════════════════════

std::vector<std::string> SlotAssembler::availableModelIds() {
    return dl::ModelRegistry::instance().availableModels();
}

std::optional<ModelDisplayInfo> SlotAssembler::getModelDisplayInfo(
        std::string const & model_id) {

    auto info = dl::ModelRegistry::instance().getModelInfo(model_id);
    if (!info) return std::nullopt;

    ModelDisplayInfo display;
    display.model_id = info->model_id;
    display.display_name = info->display_name;
    display.description = info->description;
    display.inputs = info->inputs;
    display.outputs = info->outputs;
    display.preferred_batch_size = info->preferred_batch_size;
    display.max_batch_size = info->max_batch_size;
    display.batch_mode = info->batch_mode;
    display.recommended_post_encoder = info->recommended_post_encoder;
    return display;
}

std::optional<ModelDisplayInfo> SlotAssembler::currentModelDisplayInfo() const {
    if (!_impl || !_impl->model) return std::nullopt;

    auto const & model = *_impl->model;
    ModelDisplayInfo display;
    display.model_id = model.modelId();
    display.display_name = model.displayName();
    display.description = model.description();
    display.inputs = model.inputSlots();
    display.outputs = effectiveOutputSlotsFor(
            _impl->model.get(),
            _impl->post_encoder_module.get());
    display.preferred_batch_size = model.preferredBatchSize();
    display.max_batch_size = model.maxBatchSize();
    display.batch_mode = model.batchMode();
    display.recommended_post_encoder = model.recommendedPostEncoderModule();
    return display;
}

// ════════════════════════════════════════════════════════════════════════════
// Static: encoder / decoder queries
// ════════════════════════════════════════════════════════════════════════════

std::vector<std::string> SlotAssembler::availableEncoders() {
    return dl::EncoderFactory::availableEncoders();
}

std::vector<std::string> SlotAssembler::availableDecoders() {
    return dl::DecoderFactory::availableDecoders();
}

std::string SlotAssembler::dataTypeForEncoder(
        dl::widget::EncoderVariant const & encoder) {
    return dl::widget::dataTypeForEncoder(encoder);
}

std::string SlotAssembler::dataTypeForEncoder(std::string const & factory_name) {
    if (auto const params = dl::encoderParamsFromFactoryName(factory_name)) {
        return dl::dataTypeForEncoder(*params);
    }
    return {};
}

std::string SlotAssembler::dataTypeForDecoder(
        dl::widget::DecoderVariant const & decoder) {
    std::string data_type;
    decoder.visit([&](auto const & params) {
        data_type = dl::dataTypeForDecoderParams<std::decay_t<decltype(params)>>();
    });
    return data_type;
}

std::string SlotAssembler::dataTypeForDecoder(
        std::string const & decoder_factory_name) {
    if (auto const params = dl::decoderParamsFromFactoryName(decoder_factory_name)) {
        return dl::dataTypeForDecoder(*params);
    }
    return {};
}

// ════════════════════════════════════════════════════════════════════════════
// Post-encoder configuration
// ════════════════════════════════════════════════════════════════════════════

void SlotAssembler::configurePostEncoderModule(
        dl::widget::PostEncoderSlotParams const & params,
        ImageSize source_image_size) {
    if (!_impl || !_impl->model) return;

    _impl->post_encoder_module = dl::PostEncoderModuleRegistry::instance().create(
            params.module_key,
            params.parameters_json,
            source_image_size);
}

void SlotAssembler::_updateSpatialPoint(DataManager & dm, int frame) {
    if (!_impl || !_impl->model) return;

    auto * module = _impl->post_encoder_module.get();
    if (!module || module->name() != "spatial_point") return;

    auto * sp = dynamic_cast<dl::SpatialPointExtractModule *>(module);
    if (!sp) return;

    auto const & point_key = sp->pointKey();
    if (point_key.empty()) return;

    auto point_data = dm.getData<PointData>(point_key);
    if (!point_data) return;

    auto points = point_data->getAtTime(TimeFrameIndex(frame));
    if (!points.empty()) {
        sp->setPoint(*points.begin());
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Model shape configuration
// ════════════════════════════════════════════════════════════════════════════

void SlotAssembler::configureModelShape(
        int input_height,
        int input_width,
        std::vector<int64_t> const & output_shape) {
    if (!_impl || !_impl->model) return;

    auto * enc = dynamic_cast<dl::GeneralEncoderModel *>(_impl->model.get());
    if (!enc) return;

    enc->setInputResolution(input_height, input_width);

    if (!output_shape.empty()) {
        enc->setOutputShape(output_shape);
    }
}
