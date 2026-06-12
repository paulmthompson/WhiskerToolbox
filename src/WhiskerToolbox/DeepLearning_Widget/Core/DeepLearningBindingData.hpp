#ifndef DEEP_LEARNING_BINDING_DATA_HPP
#define DEEP_LEARNING_BINDING_DATA_HPP

/**
 * @file DeepLearningBindingData.hpp
 * @brief Pure-data structs for model slot bindings.
 *
 * These are deliberately kept in a separate header from DeepLearningState
 * so that SlotAssembler.cpp (which includes libtorch) can use them
 * without pulling in Qt headers via EditorState.
 */

#include <algorithm>
#include <string>
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

/**
 * @brief Build a cache key for a static input entry.
 *
 * Legacy format used by recurrent hybrid resolution: "slot_name:memory_index".
 */
[[nodiscard]] inline std::string staticCacheKey(
        std::string const & slot_name, int memory_index) {
    return slot_name + ":" + std::to_string(memory_index);
}

/**
 * @brief Serializable entry for a static (memory) model input.
 */
struct StaticInputData {
    /** Static slot name (e.g. "memory_images") */
    std::string slot_name;
    /** Position in the memory buffer */
    int memory_index = 0;
    /** "DataManager" or "DataBank" */
    std::string source_type_str = "DataManager";
    /** DataManager source key (DataManager mode) */
    std::string data_key;
    /** Named DataBank entry ID (DataBank mode) */
    std::string bank_entry_id;
    /** Relative frame offset (DataManager mode) */
    int time_offset = 0;
    /** For boolean mask slots */
    bool active = true;

    /**
     * @brief Legacy capture mode string from workspace JSON ("Relative"/"Absolute").
     *
     * Read during deserialization for migration only; new bindings use
     * @p source_type_str instead.
     */
    std::string capture_mode_str;

    /**
     * @brief Get the resolved source type, migrating legacy capture_mode_str.
     */
    [[nodiscard]] StaticInputSource sourceType() const {
        if (source_type_str == "DataBank") {
            return StaticInputSource::DataBank;
        }
        if (capture_mode_str == "Absolute") {
            return StaticInputSource::DataBank;
        }
        return StaticInputSource::DataManager;
    }

    /**
     * @brief Set the source type for serialization.
     */
    void setSourceType(StaticInputSource source) {
        source_type_str = staticInputSourceToString(source);
        capture_mode_str.clear();
    }

    /**
     * @brief Whether this entry has enough configuration to participate in assembly.
     */
    [[nodiscard]] bool hasActiveSource() const {
        if (sourceType() == StaticInputSource::DataBank) {
            return !bank_entry_id.empty();
        }
        return !data_key.empty();
    }

    /**
     * @brief Resolve the bank entry ID, applying legacy migration defaults.
     */
    [[nodiscard]] std::string resolvedBankEntryId() const {
        if (!bank_entry_id.empty()) {
            return bank_entry_id;
        }
        if (sourceType() == StaticInputSource::DataBank) {
            return defaultBankEntryId(slot_name, memory_index);
        }
        return {};
    }
};

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

/**
 * @brief Build a cache key for a recurrent binding's carried tensor.
 *
 * Format: "recurrent:input_slot_name"
 */
[[nodiscard]] inline std::string recurrentCacheKey(
        std::string const & input_slot_name) {
    return "recurrent:" + input_slot_name;
}

/**
 * @brief Serializable binding that maps a model output slot back to an input slot
 *        for sequential frame-by-frame processing.
 *
 * The prediction at frame t becomes an input at frame t+1.
 *
 * When `target_memory_index >= 0`, the recurrent output is injected into a
 * specific position along the sequence dimension of the input slot rather
 * than replacing the entire slot tensor. This enables hybrid sequence inputs
 * where some positions are static captures and others are recurrent.
 */
struct RecurrentBindingData {
    /** Model input slot to feed into */
    std::string input_slot_name;
    /** Model output slot to read from */
    std::string output_slot_name;
    /** "Zeros", "StaticCapture", or "FirstOutput" */
    std::string init_mode_str = "Zeros";
    /** DataManager key for StaticCapture init mode */
    std::string init_data_key;
    /** Frame to capture for StaticCapture init mode */
    int init_frame = -1;

    /**
     * Target position along the sequence dimension for hybrid mode.
     *
     * When >= 0, the recurrent output is placed at this specific sequence
     * position instead of replacing the entire input slot tensor.
     * When < 0 (default), the recurrent output replaces the full slot.
     */
    int target_memory_index = -1;

    /**
     * @brief Whether this binding targets a specific sequence position.
     */
    [[nodiscard]] bool hasTargetMemoryIndex() const {
        return target_memory_index >= 0;
    }

    /**
     * @brief Get the init mode as an enum.
     */
    [[nodiscard]] RecurrentInitMode initMode() const {
        return recurrentInitModeFromString(init_mode_str);
    }

    /**
     * @brief Set the init mode from an enum.
     */
    void setInitMode(RecurrentInitMode mode) {
        init_mode_str = recurrentInitModeToString(mode);
    }
};

#endif// DEEP_LEARNING_BINDING_DATA_HPP
