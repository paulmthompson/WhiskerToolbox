#ifndef WHISKERTOOLBOX_TENSOR_SLOT_DESCRIPTOR_HPP
#define WHISKERTOOLBOX_TENSOR_SLOT_DESCRIPTOR_HPP

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace dl {

// ════════════════════════════════════════════════════════════════════════════
// BatchMode — describes what batch sizes a model supports
// ════════════════════════════════════════════════════════════════════════════

/// Fixed batch size: the model is compiled for exactly N samples per call.
struct FixedBatch {
    int size = 1;
};

/// Dynamic batch size: the model accepts a range of batch sizes.
/// min_size=1, max_size=0 means unlimited upper bound.
struct DynamicBatch {
    int min_size = 1;
    int max_size = 0;///< 0 = unlimited
};

/// Recurrent-only mode: batch size is locked to 1 because the model
/// uses output→input feedback that requires sequential processing.
struct RecurrentOnlyBatch {};

/// Discriminated union describing batch-size constraints for a model.
///
/// Models report their `BatchMode` via `ModelBase::batchMode()`. The widget
/// uses this to configure the batch-size spinbox and select the correct
/// weights variant.
using BatchMode = std::variant<FixedBatch, DynamicBatch, RecurrentOnlyBatch>;

/// Check if a BatchMode forces batch_size = 1.
[[nodiscard]] inline bool isBatchLocked(BatchMode const & mode) {
    return std::holds_alternative<RecurrentOnlyBatch>(mode) ||
           (std::holds_alternative<FixedBatch>(mode) &&
            std::get<FixedBatch>(mode).size == 1);
}

/// Get the maximum batch size allowed by a BatchMode.
/// Returns 0 for unlimited.
[[nodiscard]] inline int maxBatchSizeFromMode(BatchMode const & mode) {
    if (auto const * f = std::get_if<FixedBatch>(&mode)) return f->size;
    if (auto const * d = std::get_if<DynamicBatch>(&mode)) return d->max_size;
    return 1;// RecurrentOnlyBatch
}

/// Get the minimum batch size required by a BatchMode.
[[nodiscard]] inline int minBatchSizeFromMode(BatchMode const & mode) {
    if (auto const * f = std::get_if<FixedBatch>(&mode)) return f->size;
    if (auto const * d = std::get_if<DynamicBatch>(&mode)) return d->min_size;
    return 1;// RecurrentOnlyBatch
}

/// Get a human-readable description of a BatchMode.
[[nodiscard]] inline std::string batchModeDescription(BatchMode const & mode) {
    if (auto const * f = std::get_if<FixedBatch>(&mode)) {
        return "Fixed(" + std::to_string(f->size) + ")";
    }
    if (auto const * d = std::get_if<DynamicBatch>(&mode)) {
        auto result = std::string("Dynamic(") + std::to_string(d->min_size) + ", ";
        result += d->max_size > 0 ? std::to_string(d->max_size) : "unlimited";
        result += ")";
        return result;
    }
    return "RecurrentOnly";
}

/// Tensor data types supported for slot descriptors.
/// Mapped to torch::ScalarType at runtime.
enum class TensorDType : int {
    Float32 = 0,///< torch::kFloat32
    Float64 = 1,///< torch::kFloat64
    Byte = 2,   ///< torch::kByte (uint8)
    Int32 = 3,  ///< torch::kInt32
    Int64 = 4   ///< torch::kInt64
};

/// Describes one named tensor input or output of a model.
///
/// Each slot has a shape (excluding the leading batch dimension), a name,
/// and hints for encoders/decoders that the UI can use for auto-configuration.
struct TensorSlotDescriptor {
    std::string name;               ///< e.g. "encoder_image"
    std::vector<int64_t> shape;     ///< e.g. {3, 256, 256} (excluding batch)
    std::string description;        ///< Human-readable description
    std::string recommended_encoder;///< e.g. "ImageEncoder" — hint for UI
    std::string recommended_decoder;///< e.g. "TensorToMask2D"
    bool is_static = false;         ///< If true, user sets once (memory frames)
    bool is_boolean_mask = false;   ///< If true, values are 0/1 flags

    /// Expected tensor dtype. Default is Float32.
    /// Use Byte (uint8) for image inputs that the model normalizes internally.
    TensorDType dtype = TensorDType::Float32;

    /// Optional sequence dimension index within `shape`.
    /// When >= 0, this axis represents a sequence of frames (e.g., memory
    /// slots). The model internally consumes the full sequence; the UI
    /// maps each static-input entry to a position along this axis.
    /// When < 0 (default), no sequence dimension — single frame per slot.
    int sequence_dim = -1;

    /// Compute the total number of elements in one slot (excluding batch).
    [[nodiscard]] int64_t numElements() const {
        int64_t n = 1;
        for (auto const dim: shape) {
            n *= dim;
        }
        return n;
    }

    /// Check if this slot has a sequence dimension.
    [[nodiscard]] bool hasSequenceDim() const {
        return sequence_dim >= 0;
    }
};

/// Direction of a tensor slot relative to the model.
enum class SlotDirection {
    Input,
    Output
};

}// namespace dl

#endif// WHISKERTOOLBOX_TENSOR_SLOT_DESCRIPTOR_HPP
