/// @file DeepLearningParamSchemas.hpp
/// @brief Widget-level reflect-cpp parameter structs for deep learning forms.
///
/// AutoParamWidget form layouts for slot configuration. Session binding types
/// (`SlotBindingData`, `EncoderVariant`, etc.) live in `DeepLearning/bindings/`.

#ifndef DEEP_LEARNING_PARAM_SCHEMAS_HPP
#define DEEP_LEARNING_PARAM_SCHEMAS_HPP

#include "DeepLearning/bindings/EncoderDecoderBindingTypes.hpp"
#include "DeepLearning/bindings/SlotBindingTypes.hpp"

#include <rfl.hpp>

#include <string>

namespace dl::widget {

// ============================================================================
// Static input source — DataManager-relative vs DataBank entry
// ============================================================================

/**
 * @brief Re-encode from DataManager at (current_frame + time_offset) each run.
 */
struct DataManagerStaticSourceParams {
    std::string data_key;
    int time_offset = 0;///< Temporal offset from the current frame
};

/**
 * @brief Reuse a pre-encoded tensor from a named DataBank entry.
 */
struct DataBankStaticSourceParams {
    std::string bank_entry_id;
};

/**
 * @brief Tagged union discriminated by "static_source".
 */
using StaticInputSourceVariant = rfl::TaggedUnion<
        "static_source",
        DataManagerStaticSourceParams,
        DataBankStaticSourceParams>;

// ============================================================================
// Recurrent Init Mode — how recurrent (feedback) state is initialized at t=0
// ============================================================================

/**
 * @brief Initialize recurrent state with zeros.
 */
struct ZerosInitParams {};

/**
 * @brief Initialize recurrent state by capturing a static data source.
 */
struct StaticCaptureInitParams {
    std::string data_key;///< DataManager key to encode from
    int frame = 0;       ///< Frame index to capture
};

/**
 * @brief Initialize recurrent state from the first output of the model.
 */
struct FirstOutputInitParams {};

/**
 * @brief Tagged union discriminated by "init_mode".
 */
using RecurrentInitVariant = rfl::TaggedUnion<
        "init_mode",
        ZerosInitParams,
        StaticCaptureInitParams,
        FirstOutputInitParams>;

// ============================================================================
// Sequence Entry — one entry in a sequence (static) input slot
// ============================================================================

/**
 * @brief A sequence entry sourced from a DataManager data object (static data).
 *
 * Uses flat fields (not StaticInputSourceVariant) because AutoParamWidget
 * does not yet support nested tagged unions inside variant alternatives.
 */
struct StaticSequenceEntryParams {
    /// "DataManager" or "DataBank" (not the SequenceEntryVariant discriminator)
    std::string static_source_kind = "DataManager";
    std::string data_key;      ///< DataManager key (DataManager mode)
    std::string bank_entry_id; ///< DataBank entry ID (DataBank mode)
    int time_offset = 0;       ///< Temporal offset (DataManager mode)
};

/**
 * @brief A sequence entry sourced from a model output (recurrent feedback).
 */
struct RecurrentSequenceEntryParams {
    std::string output_slot_name;       ///< Which output slot feeds back
    std::string init_mode_str = "Zeros";///< How to initialize at t=0
};

/**
 * @brief Tagged union discriminated by "source_type".
 */
using SequenceEntryVariant = rfl::TaggedUnion<
        "source_type",
        StaticSequenceEntryParams,
        RecurrentSequenceEntryParams>;

/**
 * @brief Wrapper for AutoParamWidget
 *
 *  variant must be a struct field (like encoder in DynamicInputSlotParams),
 * not the root type, so fromJson/toJson nesting works.
 */
struct SequenceEntryParams {
    SequenceEntryVariant entry = StaticSequenceEntryParams{};
};

// ============================================================================
// Composite Slot Parameter Structs
// ============================================================================

/**
 * @brief Full configuration for one dynamic (per-frame) input slot.
 */
struct DynamicInputSlotParams {
    std::string source;                          ///< DataManager key (dynamic combo)
    dl::EncoderVariant encoder = dl::ImageEncoderParams{};///< Encoder type + params
    int time_offset = 0;                         ///< Temporal offset from current frame
};

/**
 * @brief Full configuration for one non-sequence static (memory) input slot.
 */
struct StaticInputSlotParams {
    StaticInputSourceVariant source = DataManagerStaticSourceParams{};
};

/**
 * @brief Parameters for an output slot binding (target key + decoder configuration).
 */
struct OutputSlotParams {
    std::string data_key;                         ///< DataManager key for results
    dl::DecoderVariant decoder = dl::MaskDecoderParams{};///< Decoder configuration
};

/**
 * @brief Full configuration for one non-sequence recurrent (feedback) binding.
 */
struct RecurrentBindingSlotParams {
    std::string output_slot_name;                 ///< Model output slot (dynamic combo)
    RecurrentInitVariant init = ZerosInitParams{};///< Init mode variant
};

/**
 * @brief Custom encoder input/output dimensions (UI-only, passed to
 * SlotAssembler::configureModelShape()).
 */
struct EncoderShapeParams {
    rfl::Validator<int, rfl::Minimum<1>> input_height = 224;
    rfl::Validator<int, rfl::Minimum<1>> input_width = 224;
    std::string output_shape;///< Raw output shape string, e.g. "768,16,16"
};

}// namespace dl::widget

#endif// DEEP_LEARNING_PARAM_SCHEMAS_HPP
