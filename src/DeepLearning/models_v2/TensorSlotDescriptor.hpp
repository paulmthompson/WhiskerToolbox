#ifndef WHISKERTOOLBOX_TENSOR_SLOT_DESCRIPTOR_HPP
#define WHISKERTOOLBOX_TENSOR_SLOT_DESCRIPTOR_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace dl {

/// Describes one named tensor input or output of a model.
///
/// Each slot has a shape (excluding the leading batch dimension), a name,
/// and hints for encoders/decoders that the UI can use for auto-configuration.
struct TensorSlotDescriptor {
    std::string name;                     ///< e.g. "encoder_image"
    std::vector<int64_t> shape;           ///< e.g. {3, 256, 256} (excluding batch)
    std::string description;              ///< Human-readable description
    std::string recommended_encoder;      ///< e.g. "ImageEncoder" — hint for UI
    std::string recommended_decoder;      ///< e.g. "TensorToMask2D"
    bool is_static = false;               ///< If true, user sets once (memory frames)
    bool is_boolean_mask = false;         ///< If true, values are 0/1 flags

    /// Optional sequence dimension index within `shape`.
    /// When >= 0, this axis represents a sequence of frames (e.g., memory
    /// slots). The model internally consumes the full sequence; the UI
    /// maps each static-input entry to a position along this axis.
    /// When < 0 (default), no sequence dimension — single frame per slot.
    int sequence_dim = -1;

    /// Compute the total number of elements in one slot (excluding batch).
    [[nodiscard]] int64_t numElements() const {
        int64_t n = 1;
        for (auto const dim : shape) {
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

} // namespace dl

#endif // WHISKERTOOLBOX_TENSOR_SLOT_DESCRIPTOR_HPP
