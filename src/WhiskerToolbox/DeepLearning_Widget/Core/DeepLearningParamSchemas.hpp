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
#include "DeepLearning/post_encoder/PostEncoderModuleFactory.hpp"


#include <rfl.hpp>

#include <string>

namespace dl::widget {

// ============================================================================
// Capture Mode — how a static input is acquired (Relative vs Absolute)
// ============================================================================

/**
 * @brief Relative capture to encode data at (current_frame + time_offset) each run.
 */
struct RelativeCaptureParams {
    int time_offset = 0;///< Temporal offset from the current frame
};

/**
 * @brief Absolute capture to encode data once at a specific frame and cache the tensor.
 */
struct AbsoluteCaptureParams {
    int captured_frame = -1;///< Frame index at which the tensor was captured
};

/**
 * @brief Tagged union discriminated by "capture_mode".
 */
using CaptureModeVariant = rfl::TaggedUnion<
        "capture_mode",
        RelativeCaptureParams,
        AbsoluteCaptureParams>;

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
 */
struct StaticSequenceEntryParams {
    std::string data_key;                     ///< DataManager key
    std::string capture_mode_str = "Relative";///< "Relative" or "Absolute"
    int time_offset = 0;                      ///< Used in Relative mode
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
// Post-Encoder Module — which post-encoder module to apply
// ============================================================================

/**
 * @brief No post-encoder processing (identity pass-through).
 */
struct NoPostEncoderParams {};

/**
 * @brief Tagged union discriminated by "module".
 * 
 * GlobalAvgPoolModuleParams and SpatialPointModuleParams are re-used from
 * the DeepLearning library.
 */
using PostEncoderVariant = rfl::TaggedUnion<
        "module",
        NoPostEncoderParams,
        dl::GlobalAvgPoolModuleParams,
        dl::SpatialPointModuleParams>;

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
    std::string source;                                       ///< DataManager key (dynamic combo)
    CaptureModeVariant capture_mode = RelativeCaptureParams{};///< Relative or Absolute capture mode
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
struct PostEncoderSlotParams {
    PostEncoderVariant module = NoPostEncoderParams{};///< Post-encoder module variant
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
