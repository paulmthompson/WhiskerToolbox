/// @file BindingConversion.cpp
/// @brief Implementation of pure conversion functions between widget params
///        and DeepLearningState binding data.

#include "BindingConversion.hpp"

#include <type_traits>

namespace dl::conversion {

namespace {

void assignStaticInputFromSourceVariant(
        StaticInputData & si,
        dl::widget::StaticInputSourceVariant const & source) {
    source.visit([&](auto const & params) {
        using T = std::decay_t<decltype(params)>;
        if constexpr (std::is_same_v<T, dl::widget::DataManagerStaticSourceParams>) {
            si.setSourceType(StaticInputSource::DataManager);
            si.data_key = params.data_key;
            si.time_offset = params.time_offset;
            si.bank_entry_id.clear();
        } else if constexpr (std::is_same_v<T,
                                            dl::widget::DataBankStaticSourceParams>) {
            si.setSourceType(StaticInputSource::DataBank);
            si.bank_entry_id = params.bank_entry_id;
            si.data_key.clear();
            si.time_offset = 0;
        }
    });
}

[[nodiscard]] dl::widget::StaticInputSourceVariant
toStaticInputSourceVariant(StaticInputData const & binding) {
    if (binding.sourceType() == StaticInputSource::DataBank) {
        return dl::widget::DataBankStaticSourceParams{
                .bank_entry_id = binding.resolvedBankEntryId()};
    }
    return dl::widget::DataManagerStaticSourceParams{
            .data_key = binding.data_key,
            .time_offset = binding.time_offset};
}

}// namespace

// ════════════════════════════════════════════════════════════════════════════
// Params → Binding
// ════════════════════════════════════════════════════════════════════════════

StaticInputData fromStaticInputParams(
        std::string const & slot_name,
        dl::widget::StaticInputSlotParams const & params) {
    StaticInputData si;
    si.slot_name = slot_name;
    si.memory_index = 0;
    assignStaticInputFromSourceVariant(si, params.source);
    return si;
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
        dl::widget::StaticSequenceEntryParams const & params) {
    StaticInputData si;
    si.slot_name = slot_name;
    si.memory_index = memory_index;
    si.active = true;

    if (params.static_source_kind == "DataBank") {
        si.setSourceType(StaticInputSource::DataBank);
        si.bank_entry_id = params.bank_entry_id;
        si.data_key.clear();
        si.time_offset = 0;
    } else {
        si.setSourceType(StaticInputSource::DataManager);
        si.data_key = params.data_key;
        si.time_offset = params.time_offset;
        si.bank_entry_id.clear();
    }
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

dl::widget::StaticInputSlotParams toStaticInputParams(
        StaticInputData const & binding) {
    dl::widget::StaticInputSlotParams p;
    p.source = toStaticInputSourceVariant(binding);
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

dl::widget::StaticSequenceEntryParams toStaticSequenceEntryParams(
        StaticInputData const & binding) {
    dl::widget::StaticSequenceEntryParams p;
    if (binding.sourceType() == StaticInputSource::DataBank) {
        p.static_source_kind = "DataBank";
        p.bank_entry_id = binding.resolvedBankEntryId();
    } else {
        p.static_source_kind = "DataManager";
        p.data_key = binding.data_key;
        p.time_offset = binding.time_offset;
    }
    return p;
}

}// namespace dl::conversion
