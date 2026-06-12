/// @file DeepLearningParamSchemas.hpp
/// @brief Widget-level reflect-cpp parameter structs and tagged unions for
///        deep learning slot bindings.
///
/// These types compose the per-component param structs from the DeepLearning
/// library into widget-level binding variants. They describe how the UI
/// organizes slot bindings and are NOT part of the DeepLearning library.

#ifndef DEEP_LEARNING_PARAM_SCHEMAS_HPP
#define DEEP_LEARNING_PARAM_SCHEMAS_HPP

#include "DeepLearning/channel_decoding/ChannelDecoder.hpp"
#include "DeepLearning/channel_encoding/ChannelEncoder.hpp"
#include "DeepLearning/channel_encoding/EncoderDispatch.hpp"

#include <rfl.hpp>

#include <string>
#include <type_traits>

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
// Post-Encoder Module — registry-driven module key + JSON parameters
// ============================================================================

/**
 * @brief Descriptor for one post-encoder pipeline step.
 *
 * @c module_key @c "none" or empty means no post-encoder processing.
 * Parameter schemas are resolved at runtime from `PostEncoderModuleRegistry`.
 */
struct PostEncoderStepDescriptor {
    std::string module_key = "none";
    std::string parameters_json = "{}";
};

// ============================================================================
// Decoder — which decoder to use for an output slot
// ============================================================================

/**
 * @brief Tagged union discriminated by "decoder".
 * 
 * All alternative structs are re-used from the DeepLearning library.
 */
using DecoderVariant = rfl::TaggedUnion<
        "decoder",
        dl::MaskDecoderParams,
        dl::PointDecoderParams,
        dl::LineDecoderParams,
        dl::FeatureVectorDecoderParams>;

// ============================================================================
// Encoder — which encoder to use for a dynamic input slot
// ============================================================================

/**
 * @brief Tagged union discriminated by "encoder".
 * 
 * All alternative structs are re-used from the DeepLearning library.
 */
using EncoderVariant = rfl::TaggedUnion<
        "encoder",
        dl::ImageEncoderParams,
        dl::Point2DEncoderParams,
        dl::Mask2DEncoderParams,
        dl::Line2DEncoderParams>;

/**
 * @brief Whether the active encoder alternative targets MediaData.
 */
[[nodiscard]] inline bool isImageEncoder(EncoderVariant const & encoder) {
    bool result = false;
    encoder.visit([&](auto const & params) {
        result = dl::isImageEncoderParams<std::decay_t<decltype(params)>>();
    });
    return result;
}

/**
 * @brief Map encoder variant to DataManager data type name.
 */
[[nodiscard]] inline std::string dataTypeForEncoder(EncoderVariant const & encoder) {
    std::string data_type;
    encoder.visit([&](auto const & params) {
        data_type = dl::dataTypeForEncoderParams<std::decay_t<decltype(params)>>();
    });
    return data_type;
}

/**
 * @brief Assign default encoder params from a factory registry name.
 */
inline void assignEncoderFromFactoryName(
        EncoderVariant & encoder, std::string const & factory_name) {
    if (auto params = dl::encoderParamsFromFactoryName(factory_name)) {
        std::visit([&](auto const & p) { encoder = p; }, *params);
    } else {
        encoder = dl::ImageEncoderParams{};
    }
}

// ============================================================================
// Composite Slot Parameter Structs
// ============================================================================

/**
 * @brief Full configuration for one dynamic (per-frame) input slot.
 */
struct DynamicInputSlotParams {
    std::string source;                               ///< DataManager key (dynamic combo)
    EncoderVariant encoder = dl::ImageEncoderParams{};///< Encoder type + params
    int time_offset = 0;                              ///< Temporal offset from current frame
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
    std::string data_key;                        ///< DataManager key for results
    DecoderVariant decoder = MaskDecoderParams{};///< Decoder configuration
};

/**
 * @brief Full configuration for one non-sequence recurrent (feedback) binding.
 */
struct RecurrentBindingSlotParams {
    std::string output_slot_name;                 ///< Model output slot (dynamic combo)
    RecurrentInitVariant init = ZerosInitParams{};///< Init mode variant
};

/**
 * @brief Full configuration for the post-encoder module section.
 */
using PostEncoderSlotParams = PostEncoderStepDescriptor;

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

/**
 * @brief Serializable binding for a dynamic (per-frame) model input slot.
 *
 * Persisted in workspace JSON with a nested tagged `encoder` field (reflect-cpp),
 * matching `DynamicInputSlotParams` plus the model slot name.
 */
struct SlotBindingData {
    /** Model input slot name (e.g. "encoder_image") */
    std::string slot_name;
    /** DataManager key (e.g. "media/video_1") */
    std::string data_key;
    /** Encoder type and user-configurable parameters */
    dl::widget::EncoderVariant encoder = dl::ImageEncoderParams{};
    /** Temporal offset applied per frame (e.g. -1 for previous frame) */
    int time_offset = 0;
};

/**
 * @brief Serializable binding for a model output slot.
 *
 * Persisted in workspace JSON with a nested tagged `decoder` field (reflect-cpp),
 * matching `OutputSlotParams` plus the model slot name.
 */
struct OutputBindingData {
    /** Model output slot name */
    std::string slot_name;
    /** DataManager key to write results into */
    std::string data_key;
    /** Decoder type and user-configurable parameters */
    dl::widget::DecoderVariant decoder = dl::MaskDecoderParams{};
};

#endif// DEEP_LEARNING_PARAM_SCHEMAS_HPP
