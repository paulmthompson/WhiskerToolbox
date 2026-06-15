#ifndef DEEP_LEARNING_BINDING_DATA_HPP
#define DEEP_LEARNING_BINDING_DATA_HPP

/**
 * @file DeepLearningBindingData.hpp
 * @brief Pure-data structs for model slot bindings.
 *
 * Session I/O configuration types describing how DataManager, DataBank, and
 * recurrent feedback populate model tensor slots. No Qt or libtorch dependencies.
 *
 * @see docs/developer/DeepLearning/vocabulary.qmd for terminology (slot vs Qt slot).
 */

#include <rfl.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

/**
 * @brief Compute the effective frame index for a dynamic input slot.
 *
 * Combines the current frame, batch index, and temporal offset, then
 * clamps the result to [0, max_frame].
 *
 * @pre max_frame >= 0
 * @return Frame index clamped to [0, max_frame]
 */
inline int computeEncodingFrame(
        int current_frame, int batch_index, int time_offset, int max_frame) {
    return std::clamp(current_frame + batch_index + time_offset, 0, max_frame);
}

/**
 * @brief How a static (memory) model input resolves its tensor data.
 *
 * - DataManager: re-encodes from a DataManager key at
 *   `current_frame + time_offset` every invocation.
 * - DataBank: reuses a pre-encoded tensor from a named `dl::DataBank` entry.
 */
enum class StaticInputSource {
    DataManager,
    DataBank
};

/**
 * @brief Convert StaticInputSource to a string for serialization.
 */
[[nodiscard]] inline std::string staticInputSourceToString(StaticInputSource source) {
    return source == StaticInputSource::DataBank ? "DataBank" : "DataManager";
}

/**
 * @brief Convert a string to StaticInputSource (defaults to DataManager).
 */
[[nodiscard]] inline StaticInputSource staticInputSourceFromString(
        std::string const & s) {
    return s == "DataBank" ? StaticInputSource::DataBank
                           : StaticInputSource::DataManager;
}

/**
 * @brief Default DataBank entry ID for a static slot position.
 */
[[nodiscard]] inline std::string defaultBankEntryId(
        std::string const & slot_name, int memory_index) {
    return slot_name + "_" + std::to_string(memory_index);
}

// ════════════════════════════════════════════════════════════════════════════
// Memory frame source variants
// ════════════════════════════════════════════════════════════════════════════

namespace dl {

/**
 * @brief Re-encode from DataManager at (current_frame + time_offset) each run.
 */
struct DataManagerStaticSource {
    std::string data_key;
    int time_offset = 0;
};

/**
 * @brief Reuse a pre-encoded tensor from a named DataBank entry.
 */
struct DataBankStaticSource {
    std::string bank_entry_id;
};

/**
 * @brief Tagged union discriminated by @c static_source.
 */
using StaticInputSourceVariant = rfl::TaggedUnion<
        "static_source",
        DataManagerStaticSource,
        DataBankStaticSource>;

/**
 * @brief Static memory frame: input already exists in DataManager or DataBank.
 */
struct StaticFrameSource {
    StaticInputSourceVariant source = DataManagerStaticSource{};
};

/**
 * @brief Initialize recurrent state with zeros at t=0.
 */
struct ZerosInit {};

/**
 * @brief Initialize recurrent state by capturing a static data source.
 */
struct StaticCaptureInit {
    std::string data_key;
    int frame = 0;
};

/**
 * @brief Initialize recurrent state from the first output of the model.
 */
struct FirstOutputInit {};

/**
 * @brief Tagged union discriminated by @c init_mode.
 */
using RecurrentInitVariant = rfl::TaggedUnion<
        "init_mode",
        ZerosInit,
        StaticCaptureInit,
        FirstOutputInit>;

/**
 * @brief Recurrent memory frame: raw model output from the previous step.
 */
struct RecurrentFrameSource {
    std::string output_slot_name;
    RecurrentInitVariant init = ZerosInit{};
};

/**
 * @brief Tagged union discriminated by @c frame_kind.
 */
using MemoryFrameSourceVariant = rfl::TaggedUnion<
        "frame_kind",
        StaticFrameSource,
        RecurrentFrameSource>;

/**
 * @brief Binding for one position along a memory (static) input slot.
 *
 * Each slot position is either static (pre-existing data) or recurrent
 * (feedback from a model output). Boolean mask slots use @p active only.
 */
struct MemoryFrameBinding {
    /** Static slot name (e.g. "memory_images") */
    std::string slot_name;
    /** Position in the memory buffer */
    int memory_index = 0;
    /** Static or recurrent source for this position */
    MemoryFrameSourceVariant frame = StaticFrameSource{};
    /** For boolean mask slots */
    bool active = true;
};

/**
 * @brief Whether @p binding configures a static (non-recurrent) frame.
 */
[[nodiscard]] inline bool isStaticFrame(MemoryFrameBinding const & binding) {
    return rfl::holds_alternative<StaticFrameSource>(binding.frame.variant());
}

/**
 * @brief Whether @p binding configures a recurrent feedback frame.
 */
[[nodiscard]] inline bool isRecurrentFrame(MemoryFrameBinding const & binding) {
    return rfl::holds_alternative<RecurrentFrameSource>(binding.frame.variant());
}

/**
 * @brief Resolve the static source type from a static frame binding.
 */
[[nodiscard]] inline StaticInputSource staticSourceType(
        MemoryFrameBinding const & binding) {
    if (!isStaticFrame(binding)) {
        return StaticInputSource::DataManager;
    }
    auto const & source = rfl::get<StaticFrameSource>(binding.frame.variant()).source;
    if (rfl::holds_alternative<DataBankStaticSource>(source.variant())) {
        return StaticInputSource::DataBank;
    }
    return StaticInputSource::DataManager;
}

/**
 * @brief DataManager key for a static frame binding (empty when DataBank mode).
 */
[[nodiscard]] inline std::string staticDataKey(MemoryFrameBinding const & binding) {
    if (!isStaticFrame(binding)) {
        return {};
    }
    auto const & source = rfl::get<StaticFrameSource>(binding.frame.variant()).source;
    if (rfl::holds_alternative<DataManagerStaticSource>(source.variant())) {
        return rfl::get<DataManagerStaticSource>(source.variant()).data_key;
    }
    return {};
}

/**
 * @brief Temporal offset for a DataManager static frame binding.
 */
[[nodiscard]] inline int staticTimeOffset(MemoryFrameBinding const & binding) {
    if (!isStaticFrame(binding)) {
        return 0;
    }
    auto const & source = rfl::get<StaticFrameSource>(binding.frame.variant()).source;
    if (rfl::holds_alternative<DataManagerStaticSource>(source.variant())) {
        return rfl::get<DataManagerStaticSource>(source.variant()).time_offset;
    }
    return 0;
}

/**
 * @brief Explicit DataBank entry ID for a static frame binding.
 */
[[nodiscard]] inline std::string staticBankEntryId(
        MemoryFrameBinding const & binding) {
    if (!isStaticFrame(binding)) {
        return {};
    }
    auto const & source = rfl::get<StaticFrameSource>(binding.frame.variant()).source;
    if (rfl::holds_alternative<DataBankStaticSource>(source.variant())) {
        return rfl::get<DataBankStaticSource>(source.variant()).bank_entry_id;
    }
    return {};
}

/**
 * @brief Resolve the bank entry ID for a static frame, applying default when unset.
 */
[[nodiscard]] inline std::string resolvedBankEntryId(
        MemoryFrameBinding const & binding) {
    auto const explicit_id = staticBankEntryId(binding);
    if (!explicit_id.empty()) {
        return explicit_id;
    }
    if (staticSourceType(binding) == StaticInputSource::DataBank) {
        return defaultBankEntryId(binding.slot_name, binding.memory_index);
    }
    return {};
}

/**
 * @brief Whether a static frame has enough configuration to participate in assembly.
 */
[[nodiscard]] inline bool hasActiveStaticSource(MemoryFrameBinding const & binding) {
    if (!isStaticFrame(binding)) {
        return false;
    }
    if (staticSourceType(binding) == StaticInputSource::DataBank) {
        return !resolvedBankEntryId(binding).empty();
    }
    return !staticDataKey(binding).empty();
}

/**
 * @brief Output slot name for a recurrent frame binding.
 */
[[nodiscard]] inline std::string recurrentOutputSlot(
        MemoryFrameBinding const & binding) {
    if (!isRecurrentFrame(binding)) {
        return {};
    }
    return rfl::get<RecurrentFrameSource>(binding.frame.variant()).output_slot_name;
}

/**
 * @brief Whether a recurrent frame binding is active (output slot selected).
 */
[[nodiscard]] inline bool hasActiveRecurrentBinding(
        MemoryFrameBinding const & binding) {
    if (!isRecurrentFrame(binding)) {
        return false;
    }
    auto const name = recurrentOutputSlot(binding);
    return !name.empty() && name != "(None)";
}

/**
 * @brief Whether any memory frame uses active recurrent feedback.
 */
[[nodiscard]] inline bool hasActiveRecurrentBindings(
        std::vector<MemoryFrameBinding> const & frames) {
    return std::any_of(
            frames.begin(),
            frames.end(),
            [](MemoryFrameBinding const & frame) {
                return hasActiveRecurrentBinding(frame);
            });
}

/**
 * @brief Find a memory frame for @p slot_name and @p memory_index.
 */
[[nodiscard]] inline MemoryFrameBinding const * findMemoryFrame(
        std::vector<MemoryFrameBinding> const & frames,
        std::string const & slot_name,
        int memory_index) {
    for (auto const & frame: frames) {
        if (frame.slot_name == slot_name && frame.memory_index == memory_index) {
            return &frame;
        }
    }
    return nullptr;
}

} // namespace dl

// ════════════════════════════════════════════════════════════════════════════
// Recurrent (Feedback) Inputs
// ════════════════════════════════════════════════════════════════════════════

/**
 * @brief Initialization mode for a recurrent (feedback) input at t=0.
 *
 * - Zeros: start with an all-zeros tensor matching the input slot shape.
 * - StaticCapture: use a user-provided captured tensor (e.g., ground-truth
 *   mask at a reference frame).
 * - FirstOutput: run the model once with zeros, discard the decoded result,
 *   use the raw output tensor as the initial state for the real sequence.
 */
enum class RecurrentInitMode {
    /** Initialize with all-zeros tensor */
    Zeros,
    /** Use a user-captured tensor from a specific frame */
    StaticCapture,
    /** Run once with zeros, use raw output as initial state */
    FirstOutput
};

/**
 * @brief Convert RecurrentInitMode to a string for serialization.
 */
[[nodiscard]] inline std::string recurrentInitModeToString(RecurrentInitMode mode) {
    switch (mode) {
        case RecurrentInitMode::StaticCapture:
            return "StaticCapture";
        case RecurrentInitMode::FirstOutput:
            return "FirstOutput";
        default:
            return "Zeros";
    }
}

/**
 * @brief Convert a string to RecurrentInitMode (defaults to Zeros on unknown input).
 */
[[nodiscard]] inline RecurrentInitMode recurrentInitModeFromString(std::string const & s) {
    if (s == "StaticCapture") return RecurrentInitMode::StaticCapture;
    if (s == "FirstOutput") return RecurrentInitMode::FirstOutput;
    return RecurrentInitMode::Zeros;
}

namespace dl {

/**
 * @brief Resolve the recurrent init mode from a recurrent frame binding.
 */
[[nodiscard]] inline RecurrentInitMode recurrentInitMode(
        MemoryFrameBinding const & binding) {
    if (!isRecurrentFrame(binding)) {
        return RecurrentInitMode::Zeros;
    }
    auto const & init = rfl::get<RecurrentFrameSource>(binding.frame.variant()).init;
    if (rfl::holds_alternative<StaticCaptureInit>(init.variant())) {
        return RecurrentInitMode::StaticCapture;
    }
    if (rfl::holds_alternative<FirstOutputInit>(init.variant())) {
        return RecurrentInitMode::FirstOutput;
    }
    return RecurrentInitMode::Zeros;
}

/**
 * @brief DataManager key for StaticCapture init (empty when not applicable).
 */
[[nodiscard]] inline std::string staticCaptureDataKey(
        MemoryFrameBinding const & binding) {
    if (!isRecurrentFrame(binding)) {
        return {};
    }
    auto const & init = rfl::get<RecurrentFrameSource>(binding.frame.variant()).init;
    if (rfl::holds_alternative<StaticCaptureInit>(init.variant())) {
        return rfl::get<StaticCaptureInit>(init.variant()).data_key;
    }
    return {};
}

/**
 * @brief Frame index for StaticCapture init (-1 when not applicable).
 */
[[nodiscard]] inline int staticCaptureFrame(MemoryFrameBinding const & binding) {
    if (!isRecurrentFrame(binding)) {
        return -1;
    }
    auto const & init = rfl::get<RecurrentFrameSource>(binding.frame.variant()).init;
    if (rfl::holds_alternative<StaticCaptureInit>(init.variant())) {
        return rfl::get<StaticCaptureInit>(init.variant()).frame;
    }
    return -1;
}

} // namespace dl

/**
 * @brief Build a cache key for a recurrent binding's carried tensor.
 *
 * Format: "recurrent:input_slot_name"
 */
[[nodiscard]] inline std::string recurrentCacheKey(
        std::string const & input_slot_name) {
    return "recurrent:" + input_slot_name;
}

#endif// DEEP_LEARNING_BINDING_DATA_HPP
