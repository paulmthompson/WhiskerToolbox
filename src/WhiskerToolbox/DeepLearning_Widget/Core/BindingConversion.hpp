/// @file BindingConversion.hpp
/// @brief Pure conversion functions between widget-level param structs and
///        DeepLearningState binding data.
///
/// Phase 3.1: Extracts conversion logic from _syncBindingsFromUi into
/// testable free functions. Enables live state updates (connect bindingChanged
/// directly to conversion+state-set) and eliminates the "flush before run" pattern.

#ifndef DEEP_LEARNING_BINDING_CONVERSION_HPP
#define DEEP_LEARNING_BINDING_CONVERSION_HPP

#include "DeepLearning_Widget/Core/DeepLearningBindingData.hpp"
#include "DeepLearning_Widget/Core/DeepLearningParamSchemas.hpp"

#include <string>

namespace dl::conversion {

// ════════════════════════════════════════════════════════════════════════════
// Params → Binding (UI → State)
// ════════════════════════════════════════════════════════════════════════════

/// Convert DynamicInputSlotParams to SlotBindingData.
///
/// @param slot_name  Model input slot name (e.g. "encoder_image").
/// @param params     Widget-level params from DynamicInputSlotWidget.
/// @return Binding data for SlotAssembler.
///
/// @pre slot_name must not be empty; an empty name produces a binding with no
///      identifiable slot, which SlotAssembler cannot match against the model
///      (enforcement: none) [IMPORTANT]
[[nodiscard]] SlotBindingData fromDynamicInputParams(
        std::string const & slot_name,
        dl::widget::DynamicInputSlotParams const & params);

/// Convert StaticInputSlotParams to StaticInputData.
///
/// @param slot_name       Model static slot name.
/// @param params          Widget-level params from StaticInputSlotWidget.
/// @param captured_frame  Frame index from last capture (-1 if never captured).
/// @return Binding data for SlotAssembler.
///
/// @pre slot_name must not be empty; an empty name produces a binding with no
///      identifiable slot, which SlotAssembler cannot match against the model
///      (enforcement: none) [IMPORTANT]
[[nodiscard]] StaticInputData fromStaticInputParams(
        std::string const & slot_name,
        dl::widget::StaticInputSlotParams const & params,
        int captured_frame);

/// Convert OutputSlotParams to OutputBindingData.
///
/// @param slot_name  Model output slot name.
/// @param params     Widget-level params from OutputSlotWidget.
/// @return Binding data for SlotAssembler.
///
/// @pre slot_name must not be empty; an empty name produces a binding with no
///      identifiable slot, which SlotAssembler cannot match against the model
///      (enforcement: none) [IMPORTANT]
[[nodiscard]] OutputBindingData fromOutputParams(
        std::string const & slot_name,
        dl::widget::OutputSlotParams const & params);

/// Convert RecurrentBindingSlotParams to RecurrentBindingData.
///
/// @param slot_name  Model input slot name (recurrent target).
/// @param params     Widget-level params from RecurrentBindingWidget.
/// @return Binding data for SlotAssembler.
///
/// @pre slot_name must not be empty; an empty name produces a binding with no
///      identifiable input slot, which SlotAssembler cannot match against the model
///      (enforcement: none) [IMPORTANT]
[[nodiscard]] RecurrentBindingData fromRecurrentParams(
        std::string const & slot_name,
        dl::widget::RecurrentBindingSlotParams const & params);

/// Convert StaticSequenceEntryParams to StaticInputData.
///
/// @param slot_name       Model sequence slot name.
/// @param memory_index    Position in the sequence.
/// @param params          Entry params from SequenceSlotWidget.
/// @param captured_frame  Frame index from last capture (-1 if never captured).
/// @return Binding data for SlotAssembler.
///
/// @pre slot_name must not be empty; an empty name produces a binding with no
///      identifiable slot, which SlotAssembler cannot match against the model
///      (enforcement: none) [IMPORTANT]
/// @pre memory_index must be >= 0; SlotAssembler uses it as a tensor dimension
///      index via `tensor.select()` and only checks the upper bound, so a
///      negative value causes an out-of-bounds access
///      (enforcement: none) [CRITICAL]
[[nodiscard]] StaticInputData fromStaticSequenceEntryParams(
        std::string const & slot_name,
        int memory_index,
        dl::widget::StaticSequenceEntryParams const & params,
        int captured_frame);

/// Convert RecurrentSequenceEntryParams to RecurrentBindingData.
///
/// @param slot_name        Model sequence slot name.
/// @param memory_index     Target position in the sequence.
/// @param params           Entry params from SequenceSlotWidget.
/// @return Binding data for SlotAssembler.
///
/// @pre slot_name must not be empty; an empty name produces a binding with no
///      identifiable input slot, which SlotAssembler cannot match against the model
///      (enforcement: none) [IMPORTANT]
/// @pre memory_index must be >= 0; a negative value causes
///      RecurrentBindingData::hasTargetMemoryIndex() to return false, so the
///      recurrent output silently replaces the entire input slot rather than
///      injecting at the intended sequence position
///      (enforcement: none) [IMPORTANT]
[[nodiscard]] RecurrentBindingData fromRecurrentSequenceEntryParams(
        std::string const & slot_name,
        int memory_index,
        dl::widget::RecurrentSequenceEntryParams const & params);

// ════════════════════════════════════════════════════════════════════════════
// Binding → Params (State → UI restore)
// ════════════════════════════════════════════════════════════════════════════

/// Convert SlotBindingData to DynamicInputSlotParams.
[[nodiscard]] dl::widget::DynamicInputSlotParams toDynamicInputParams(
        SlotBindingData const & binding);

/// Convert StaticInputData to StaticInputSlotParams.
[[nodiscard]] dl::widget::StaticInputSlotParams toStaticInputParams(
        StaticInputData const & binding);

/// Convert OutputBindingData to OutputSlotParams.
[[nodiscard]] dl::widget::OutputSlotParams toOutputParams(
        OutputBindingData const & binding);

/// Convert RecurrentBindingData to RecurrentBindingSlotParams.
[[nodiscard]] dl::widget::RecurrentBindingSlotParams toRecurrentParams(
        RecurrentBindingData const & binding);

}// namespace dl::conversion

#endif// DEEP_LEARNING_BINDING_CONVERSION_HPP
