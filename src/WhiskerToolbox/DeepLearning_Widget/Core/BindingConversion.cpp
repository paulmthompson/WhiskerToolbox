/// @file BindingConversion.cpp
/// @brief Implementation of pure conversion functions between widget params
///        and DeepLearningState binding data.

#include "BindingConversion.hpp"


namespace dl::conversion {

// ════════════════════════════════════════════════════════════════════════════
// Params → Binding
// ════════════════════════════════════════════════════════════════════════════

SlotBindingData fromDynamicInputParams(
        std::string const & slot_name,
        dl::widget::DynamicInputSlotParams const & params) {
    SlotBindingData binding;
    binding.slot_name = slot_name;
    binding.data_key = params.source;
    binding.encoder = params.encoder;
    binding.time_offset = params.time_offset;
    return binding;
}

StaticInputData fromStaticInputParams(
        std::string const & slot_name,
        dl::widget::StaticInputSlotParams const & params,
        int captured_frame) {
    StaticInputData si;
    si.slot_name = slot_name;
    si.memory_index = 0;
    si.data_key = params.source;
    si.bank_entry_id = params.bank_entry_id;
    si.captured_frame = captured_frame;

    params.capture_mode.visit([&](auto const & cm) {
        using T = std::decay_t<decltype(cm)>;
        if constexpr (std::is_same_v<T, dl::widget::RelativeCaptureParams>) {
            si.capture_mode_str = "Relative";
            si.time_offset = cm.time_offset;
        } else if constexpr (std::is_same_v<T,
                                            dl::widget::AbsoluteCaptureParams>) {
            si.capture_mode_str = "Absolute";
            si.time_offset = 0;
        }
    });

    return si;
}

OutputBindingData fromOutputParams(
        std::string const & slot_name,
        dl::widget::OutputSlotParams const & params) {
    OutputBindingData binding;
    binding.slot_name = slot_name;
    binding.data_key = (params.data_key == "(None)") ? "" : params.data_key;
    binding.decoder = params.decoder;
    return binding;
}

RecurrentBindingData fromRecurrentParams(
        std::string const & slot_name,
        dl::widget::RecurrentBindingSlotParams const & params) {
    RecurrentBindingData binding;
    binding.input_slot_name = slot_name;
    binding.output_slot_name =
            (params.output_slot_name == "(None)") ? "" : params.output_slot_name;
    binding.target_memory_index = -1;

    params.init.visit([&](auto const & init) {
        using T = std::decay_t<decltype(init)>;
        if constexpr (std::is_same_v<T, dl::widget::ZerosInitParams>) {
            binding.init_mode_str = "Zeros";
        } else if constexpr (std::is_same_v<T,
                                            dl::widget::StaticCaptureInitParams>) {
            binding.init_mode_str = "StaticCapture";
            binding.init_data_key =
                    (init.data_key == "(None)") ? "" : init.data_key;
            binding.init_frame = init.frame;
        } else if constexpr (std::is_same_v<T,
                                            dl::widget::FirstOutputInitParams>) {
            binding.init_mode_str = "FirstOutput";
        }
    });

    return binding;
}

StaticInputData fromStaticSequenceEntryParams(
        std::string const & slot_name,
        int memory_index,
        dl::widget::StaticSequenceEntryParams const & params,
        int captured_frame) {
    StaticInputData si;
    si.slot_name = slot_name;
    si.memory_index = memory_index;
    si.data_key = params.data_key;
    si.bank_entry_id = params.bank_entry_id;
    si.capture_mode_str = params.capture_mode_str;
    si.time_offset = params.time_offset;
    si.captured_frame = captured_frame;
    si.active = true;
    return si;
}

RecurrentBindingData fromRecurrentSequenceEntryParams(
        std::string const & slot_name,
        int memory_index,
        dl::widget::RecurrentSequenceEntryParams const & params) {
    RecurrentBindingData rb;
    rb.input_slot_name = slot_name;
    rb.output_slot_name =
            (params.output_slot_name == "(None)") ? "" : params.output_slot_name;
    rb.target_memory_index = memory_index;
    rb.init_mode_str = params.init_mode_str;
    return rb;
}

// ════════════════════════════════════════════════════════════════════════════
// Binding → Params
// ════════════════════════════════════════════════════════════════════════════

dl::widget::DynamicInputSlotParams toDynamicInputParams(
        SlotBindingData const & binding) {
    dl::widget::DynamicInputSlotParams p;
    p.source = binding.data_key;
    p.encoder = binding.encoder;
    p.time_offset = binding.time_offset;
    return p;
}

dl::widget::StaticInputSlotParams toStaticInputParams(
        StaticInputData const & binding) {
    dl::widget::StaticInputSlotParams p;
    p.source = binding.data_key;
    p.bank_entry_id = binding.bank_entry_id;

    if (binding.capture_mode_str == "Absolute") {
        p.capture_mode = dl::widget::AbsoluteCaptureParams{};
    } else {
        p.capture_mode =
                dl::widget::RelativeCaptureParams{.time_offset =
                                                          binding.time_offset};
    }
    return p;
}

dl::widget::OutputSlotParams toOutputParams(
        OutputBindingData const & binding) {
    dl::widget::OutputSlotParams p;
    p.data_key = binding.data_key;
    p.decoder = binding.decoder;
    return p;
}

dl::widget::RecurrentBindingSlotParams toRecurrentParams(
        RecurrentBindingData const & binding) {
    dl::widget::RecurrentBindingSlotParams p;
    p.output_slot_name = binding.output_slot_name;

    if (binding.init_mode_str == "StaticCapture") {
        p.init = dl::widget::StaticCaptureInitParams{
                .data_key = binding.init_data_key,
                .frame = binding.init_frame};
    } else if (binding.init_mode_str == "FirstOutput") {
        p.init = dl::widget::FirstOutputInitParams{};
    } else {
        p.init = dl::widget::ZerosInitParams{};
    }
    return p;
}

}// namespace dl::conversion
