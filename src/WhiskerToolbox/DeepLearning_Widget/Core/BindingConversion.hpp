/**
 * @file BindingConversion.hpp
 * @brief Pure conversion functions between widget-level param structs and
 *        DeepLearningState binding data.
 */
#ifndef DEEP_LEARNING_BINDING_CONVERSION_HPP
#define DEEP_LEARNING_BINDING_CONVERSION_HPP

#include "DeepLearning_Widget/Core/DeepLearningParamSchemas.hpp"

#include "DeepLearning/bindings/DeepLearningBindingData.hpp"
#include "DeepLearning/bindings/SlotBindingTypes.hpp"

#include <string>

namespace dl::conversion {

// ════════════════════════════════════════════════════════════════════════════
// Params → Binding (UI → State)
// ════════════════════════════════════════════════════════════════════════════

/**
 * @brief Convert StaticInputSlotParams to StaticInputData.
 * @param slot_name  Model static slot name.
 * @param params     Widget-level params from StaticInputSlotWidget.
 * @return Binding data for SlotAssembler.
 *
 * @pre slot_name must not be empty; an empty name produces a binding with no
 *      identifiable slot, which SlotAssembler cannot match against the model
 *      (enforcement: none) [IMPORTANT]
 */
[[nodiscard]] StaticInputData fromStaticInputParams(
        std::string const & slot_name,
        dl::widget::StaticInputSlotParams const & params);

/**
 * @brief Convert RecurrentBindingSlotParams to RecurrentBindingData.
 * @param slot_name  Model input slot name (recurrent target).
 * @param params     Widget-level params from RecurrentBindingWidget.
 * @return Binding data for SlotAssembler.
 *
 * @pre slot_name must not be empty; an empty name produces a binding with no
 *      identifiable input slot, which SlotAssembler cannot match against the model
 *      (enforcement: none) [IMPORTANT]
 */
[[nodiscard]] RecurrentBindingData fromRecurrentParams(
        std::string const & slot_name,
        dl::widget::RecurrentBindingSlotParams const & params);

/**
 * @brief Convert StaticSequenceEntryParams to StaticInputData.
 * @param slot_name  Model sequence slot name.
 * @param memory_index  Position in the sequence.
 * @param params     Entry params from SequenceSlotWidget.
 * @return Binding data for SlotAssembler.
 *
 * @pre slot_name must not be empty; an empty name produces a binding with no
 *      identifiable slot, which SlotAssembler cannot match against the model
 *      (enforcement: none) [IMPORTANT]
 * @pre memory_index must be >= 0; SlotAssembler uses it as a tensor dimension
 *      index via `tensor.select()` and only checks the upper bound, so a
 *      negative value causes an out-of-bounds access
 *      (enforcement: none) [CRITICAL]
 */
[[nodiscard]] StaticInputData fromStaticSequenceEntryParams(
        std::string const & slot_name,
        int memory_index,
        dl::widget::StaticSequenceEntryParams const & params);

/**
 * @brief Convert RecurrentSequenceEntryParams to RecurrentBindingData.
 * @param slot_name  Model sequence slot name.
 * @param memory_index  Target position in the sequence.
 * @param params     Entry params from SequenceSlotWidget.
 * @return Binding data for SlotAssembler.
 *
 * @pre slot_name must not be empty; an empty name produces a binding with no
 *      identifiable input slot, which SlotAssembler cannot match against the model
 *      (enforcement: none) [IMPORTANT]
 * @pre memory_index must be >= 0; a negative value causes
 *      RecurrentBindingData::hasTargetMemoryIndex() to return false, so the
 *      recurrent output silently replaces the entire input slot rather than
 *      injecting at the intended sequence position
 *      (enforcement: none) [IMPORTANT]
 */
[[nodiscard]] RecurrentBindingData fromRecurrentSequenceEntryParams(
        std::string const & slot_name,
        int memory_index,
        dl::widget::RecurrentSequenceEntryParams const & params);


// ════════════════════════════════════════════════════════════════════════════
// Binding → Params (State → UI restore)
// ════════════════════════════════════════════════════════════════════════════

/**
 * @brief Convert StaticInputData to StaticInputSlotParams.
 */
[[nodiscard]] dl::widget::StaticInputSlotParams toStaticInputParams(
        StaticInputData const & binding);

/**
 * @brief Convert StaticInputData to StaticSequenceEntryParams.
 */
[[nodiscard]] dl::widget::StaticSequenceEntryParams toStaticSequenceEntryParams(
        StaticInputData const & binding);

/**
 * @brief Convert RecurrentBindingData to RecurrentBindingSlotParams.
 */
[[nodiscard]] dl::widget::RecurrentBindingSlotParams toRecurrentParams(
        RecurrentBindingData const & binding);

}// namespace dl::conversion

#endif// DEEP_LEARNING_BINDING_CONVERSION_HPP
