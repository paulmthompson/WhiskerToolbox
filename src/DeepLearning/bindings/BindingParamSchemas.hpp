/// @file BindingParamSchemas.hpp
/// @brief AutoParamWidget form structs and ParameterUIHints for slot bindings.

#ifndef DEEP_LEARNING_BINDING_PARAM_SCHEMAS_HPP
#define DEEP_LEARNING_BINDING_PARAM_SCHEMAS_HPP

#include "bindings/EncoderDecoderBindingTypes.hpp"
#include "bindings/SlotBindingTypes.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

#include <string>
#include <utility>

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

} // namespace dl

template<>
struct ParameterUIHints<dl::DynamicInputBindingForm> {
    static void annotate(ParameterSchema & schema);
};

template<>
struct ParameterUIHints<dl::OutputBindingForm> {
    static void annotate(ParameterSchema & schema);
};

#endif // DEEP_LEARNING_BINDING_PARAM_SCHEMAS_HPP
