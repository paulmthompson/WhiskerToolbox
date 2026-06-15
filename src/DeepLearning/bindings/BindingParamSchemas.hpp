/// @file BindingParamSchemas.hpp
/// @brief AutoParamWidget form structs and ParameterUIHints for slot bindings.

#ifndef DEEP_LEARNING_BINDING_PARAM_SCHEMAS_HPP
#define DEEP_LEARNING_BINDING_PARAM_SCHEMAS_HPP

#include "bindings/DeepLearningBindingData.hpp"
#include "bindings/EncoderDecoderBindingTypes.hpp"
#include "bindings/SlotBindingTypes.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include <string>
#include <utility>
#include <vector>

namespace dl {

/**
 * @brief User-editable fields for a dynamic input slot.
 *
 * @p slot_name is supplied by the widget shell, not this form.
 */
struct DynamicInputBindingForm {
    std::string data_key;
    EncoderVariant encoder = ImageEncoderParams{};
    int time_offset = 0;
};

/**
 * @brief User-editable fields for an output slot.
 *
 * @p slot_name is supplied by the widget shell, not this form.
 */
struct OutputBindingForm {
    std::string data_key;
    DecoderVariant decoder = MaskDecoderParams{};
};

/**
 * @brief AutoParamWidget form for a static memory frame source.
 */
struct StaticFrameSourceForm {
    StaticInputSourceVariant source = DataManagerStaticSource{};
};

/**
 * @brief AutoParamWidget form for a recurrent memory frame source.
 */
struct RecurrentFrameSourceForm {
    std::string output_slot_name;
    RecurrentInitVariant init = ZerosInit{};
};

/**
 * @brief Normalize AutoParamWidget combo sentinel values for binding storage.
 * @param key Raw combo value from the form.
 * @return Empty string when @p key is @c "(None)" or empty.
 */
[[nodiscard]] inline std::string normalizeBindingDataKey(std::string key) {
    if (key == "(None)") {
        return {};
    }
    return key;
}

/**
 * @brief Build a full dynamic input binding from form fields.
 * @param slot_name Model input slot name.
 * @param form User-edited form values.
 */
[[nodiscard]] inline SlotBindingData toSlotBindingData(
        std::string slot_name,
        DynamicInputBindingForm const & form) {
    return SlotBindingData{
            .slot_name = std::move(slot_name),
            .data_key = normalizeBindingDataKey(form.data_key),
            .encoder = form.encoder,
            .time_offset = form.time_offset};
}

/**
 * @brief Extract editable form fields from a dynamic input binding.
 */
[[nodiscard]] inline DynamicInputBindingForm fromSlotBindingData(
        SlotBindingData const & binding) {
    return DynamicInputBindingForm{
            .data_key = binding.data_key,
            .encoder = binding.encoder,
            .time_offset = binding.time_offset};
}

/**
 * @brief Build a full output binding from form fields.
 * @param slot_name Model output slot name.
 * @param form User-edited form values.
 */
[[nodiscard]] inline OutputBindingData toOutputBindingData(
        std::string slot_name,
        OutputBindingForm const & form) {
    return OutputBindingData{
            .slot_name = std::move(slot_name),
            .data_key = normalizeBindingDataKey(form.data_key),
            .decoder = form.decoder};
}

/**
 * @brief Extract editable form fields from an output binding.
 */
[[nodiscard]] inline OutputBindingForm fromOutputBindingData(
        OutputBindingData const & binding) {
    return OutputBindingForm{
            .data_key = binding.data_key,
            .decoder = binding.decoder};
}

/**
 * @brief Build a static memory frame binding.
 */
[[nodiscard]] inline MemoryFrameBinding toStaticMemoryFrame(
        std::string slot_name,
        int memory_index,
        StaticFrameSourceForm const & form) {
    return MemoryFrameBinding{
            .slot_name = std::move(slot_name),
            .memory_index = memory_index,
            .frame = StaticFrameSource{.source = form.source},
            .active = true};
}

/**
 * @brief Build a recurrent memory frame binding.
 */
[[nodiscard]] inline MemoryFrameBinding toRecurrentMemoryFrame(
        std::string slot_name,
        int memory_index,
        RecurrentFrameSourceForm const & form) {
    RecurrentFrameSource source;
    source.output_slot_name = normalizeBindingDataKey(form.output_slot_name);
    source.init = form.init;
    return MemoryFrameBinding{
            .slot_name = std::move(slot_name),
            .memory_index = memory_index,
            .frame = std::move(source),
            .active = true};
}

/**
 * @brief Extract static frame form fields from a memory frame binding.
 */
[[nodiscard]] inline StaticFrameSourceForm fromStaticMemoryFrame(
        MemoryFrameBinding const & binding) {
    if (isStaticFrame(binding)) {
        return StaticFrameSourceForm{
                .source = rfl::get<StaticFrameSource>(binding.frame.variant()).source};
    }
    return {};
}

/**
 * @brief Extract recurrent frame form fields from a memory frame binding.
 */
[[nodiscard]] inline RecurrentFrameSourceForm fromRecurrentMemoryFrame(
        MemoryFrameBinding const & binding) {
    if (!isRecurrentFrame(binding)) {
        return {};
    }
    auto const & source = rfl::get<RecurrentFrameSource>(binding.frame.variant());
    return RecurrentFrameSourceForm{
            .output_slot_name = source.output_slot_name,
            .init = source.init};
}

/**
 * @brief Build a capture binding that reads @p data_key from DataManager.
 *
 * Used with @ref SlotAssembler::captureToBank, which takes the bank entry ID
 * separately.
 */
[[nodiscard]] inline MemoryFrameBinding makeCaptureBinding(
        std::string slot_name,
        int memory_index,
        std::string data_key) {
    return MemoryFrameBinding{
            .slot_name = std::move(slot_name),
            .memory_index = memory_index,
            .frame = StaticFrameSource{
                    DataManagerStaticSource{
                            .data_key = std::move(data_key),
                            .time_offset = 0}},
            .active = true};
}

} // namespace dl

template<>
struct ParameterUIHints<dl::DynamicInputBindingForm> {
    static void annotate(ParameterSchema & schema);
};

template<>
struct ParameterUIHints<dl::OutputBindingForm> {
    static void annotate(ParameterSchema & schema);
};

template<>
struct ParameterUIHints<dl::DataManagerStaticSource> {
    static void annotate(ParameterSchema & schema);
};

template<>
struct ParameterUIHints<dl::DataBankStaticSource> {
    static void annotate(ParameterSchema & schema);
};

template<>
struct ParameterUIHints<dl::StaticFrameSourceForm> {
    static void annotate(ParameterSchema & schema);
};

template<>
struct ParameterUIHints<dl::StaticCaptureInit> {
    static void annotate(ParameterSchema & schema);
};

template<>
struct ParameterUIHints<dl::RecurrentFrameSourceForm> {
    static void annotate(ParameterSchema & schema);
};

#endif // DEEP_LEARNING_BINDING_PARAM_SCHEMAS_HPP
