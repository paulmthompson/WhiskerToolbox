/// @file SlotBindingTypes.hpp
/// @brief Serializable slot binding structs for deep learning inference sessions.

#ifndef DEEP_LEARNING_SLOT_BINDING_TYPES_HPP
#define DEEP_LEARNING_SLOT_BINDING_TYPES_HPP

#include "bindings/EncoderDecoderBindingTypes.hpp"

#include <string>

namespace dl {

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

}// namespace dl

/**
 * @brief Serializable binding for a dynamic (per-frame) model input slot.
 *
 * Persisted in workspace JSON with a nested tagged `encoder` field (reflect-cpp).
 */
struct SlotBindingData {
    /** Model input slot name (e.g. "encoder_image") */
    std::string slot_name;
    /** DataManager key (e.g. "media/video_1") */
    std::string data_key;
    /** Encoder type and user-configurable parameters */
    dl::EncoderVariant encoder = dl::ImageEncoderParams{};
    /** Temporal offset applied per frame (e.g. -1 for previous frame) */
    int time_offset = 0;
};

/**
 * @brief Serializable binding for a model output slot.
 *
 * Persisted in workspace JSON with a nested tagged `decoder` field (reflect-cpp).
 */
struct OutputBindingData {
    /** Model output slot name */
    std::string slot_name;
    /** DataManager key to write results into */
    std::string data_key;
    /** Decoder type and user-configurable parameters */
    dl::DecoderVariant decoder = dl::MaskDecoderParams{};
};

#endif// DEEP_LEARNING_SLOT_BINDING_TYPES_HPP
