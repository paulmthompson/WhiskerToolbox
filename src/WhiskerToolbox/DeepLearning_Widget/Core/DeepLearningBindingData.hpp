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
 * @brief Serializable binding for a dynamic (per-frame) model input slot.
 */
struct SlotBindingData {
    std::string slot_name;      ///< Model input slot name (e.g. "encoder_image")
    std::string data_key;       ///< DataManager key (e.g. "media/video_1")
    std::string encoder_id;     ///< Encoder factory key (e.g. "ImageEncoder")
    std::string mode = "Raw";   ///< "Raw", "Binary", "Heatmap"
    float gaussian_sigma = 2.0f;///< Gaussian sigma (Heatmap mode only)
    bool normalize = true;      ///< Normalize encoder output to [0, 1]
    int time_offset = 0;        ///< Temporal offset applied per frame (e.g. -1 for previous frame)
};

/// Compute the effective frame index for a dynamic input slot.
///
/// Combines the current frame, batch index, and temporal offset, then
/// clamps the result to [0, max_frame].
///
/// @pre max_frame >= 0
/// @return Frame index clamped to [0, max_frame]
inline int computeEncodingFrame(
        int current_frame, int batch_index, int time_offset, int max_frame) {
    return std::clamp(current_frame + batch_index + time_offset, 0, max_frame);
}

/**
 * @brief Serializable binding for a model output slot.
 */
struct OutputBindingData {
    std::string slot_name; ///< Model output slot name
    std::string data_key;  ///< DataManager key to write results into
    std::string decoder_id;///< Decoder factory key (e.g. "TensorToMask2D")
    float threshold = 0.5f;///< Mask/line threshold
    bool subpixel = true;  ///< Point subpixel refinement
};

/// Capture mode for static (memory) model inputs.
///
/// - Relative: re-encodes at `current_frame + time_offset` every invocation.
/// - Absolute: encodes once at a user-chosen frame; the tensor is cached and
///   reused on subsequent invocations without re-encoding.
enum class CaptureMode {
    Relative,///< Re-encode each run (default, original behaviour)
    Absolute ///< Encode once, cache, and reuse
};

/**
 * @brief Convert CaptureMode to a string for serialization.
 */
[[nodiscard]] inline std::string captureModeToString(CaptureMode mode) {
    return mode == CaptureMode::Absolute ? "Absolute" : "Relative";
}

/**
 * @brief Convert a string to CaptureMode (defaults to Relative on unknown input).
 */
[[nodiscard]] inline CaptureMode captureModeFromString(std::string const & s) {
    return s == "Absolute" ? CaptureMode::Absolute : CaptureMode::Relative;
}

/// Build a cache key for a static input entry.
///
/// Used by SlotAssembler to index into its tensor cache.
/// Format: "slot_name:memory_index"
[[nodiscard]] inline std::string staticCacheKey(
        std::string const & slot_name, int memory_index) {
    return slot_name + ":" + std::to_string(memory_index);
}

/**
 * @brief Serializable entry for a static (memory) model input.
 */
struct StaticInputData {
    std::string slot_name;                    ///< Static slot name (e.g. "memory_images")
    int memory_index = 0;                     ///< Position in the memory buffer
    std::string data_key;                     ///< DataManager source key
    int time_offset = 0;                      ///< Relative frame offset (e.g. -1)
    bool active = true;                       ///< For boolean mask slots
    std::string capture_mode_str = "Relative";///< "Relative" or "Absolute" (serialization-friendly)
    int captured_frame = -1;                  ///< Frame that was captured (Absolute mode only, -1 = not captured)

    /**
     * @brief Get the capture mode as an enum.
     */
    [[nodiscard]] CaptureMode captureMode() const {
        return captureModeFromString(capture_mode_str);
    }

    /**
     * @brief Set the capture mode from an enum.
     */
    void setCaptureMode(CaptureMode mode) {
        capture_mode_str = captureModeToString(mode);
    }
};

// ════════════════════════════════════════════════════════════════════════════
// Recurrent (Feedback) Inputs — Phase 4
// ════════════════════════════════════════════════════════════════════════════

/// Initialization mode for a recurrent (feedback) input at t=0.
///
/// - Zeros: start with an all-zeros tensor matching the input slot shape.
/// - StaticCapture: use a user-provided captured tensor (e.g., ground-truth
///   mask at a reference frame).
/// - FirstOutput: run the model once with zeros, discard the decoded result,
///   use the raw output tensor as the initial state for the real sequence.
enum class RecurrentInitMode {
    Zeros,        ///< Initialize with all-zeros tensor
    StaticCapture,///< Use a user-captured tensor from a specific frame
    FirstOutput   ///< Run once with zeros, use raw output as initial state
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

/// Build a cache key for a recurrent binding's carried tensor.
///
/// Format: "recurrent:input_slot_name"
[[nodiscard]] inline std::string recurrentCacheKey(
        std::string const & input_slot_name) {
    return "recurrent:" + input_slot_name;
}

/// Serializable binding that maps a model output slot back to an input slot
/// for sequential frame-by-frame processing.
///
/// The prediction at frame t becomes an input at frame t+1.
///
/// When `target_memory_index >= 0`, the recurrent output is injected into a
/// specific position along the sequence dimension of the input slot rather
/// than replacing the entire slot tensor. This enables hybrid sequence inputs
/// where some positions are static captures and others are recurrent.
struct RecurrentBindingData {
    std::string input_slot_name;        ///< Model input slot to feed into
    std::string output_slot_name;       ///< Model output slot to read from
    std::string init_mode_str = "Zeros";///< "Zeros", "StaticCapture", or "FirstOutput"
    std::string init_data_key;          ///< DataManager key for StaticCapture init mode
    int init_frame = -1;                ///< Frame to capture for StaticCapture init mode

    /// Target position along the sequence dimension for hybrid mode.
    /// When >= 0, the recurrent output is placed at this specific sequence
    /// position instead of replacing the entire input slot tensor.
    /// When < 0 (default), the recurrent output replaces the full slot.
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
