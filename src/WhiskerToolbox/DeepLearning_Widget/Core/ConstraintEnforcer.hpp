/**
 * @file ConstraintEnforcer.hpp
 * @brief Pure constraint computation functions for the deep learning widget.
 */

#ifndef DEEP_LEARNING_CONSTRAINT_ENFORCER_HPP
#define DEEP_LEARNING_CONSTRAINT_ENFORCER_HPP

#include "DeepLearning_Widget/Core/DeepLearningBindingData.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"// ModelDisplayInfo

#include <string>
#include <vector>

namespace dl::constraints {

// ════════════════════════════════════════════════════════════════════════════
// Batch-size constraint
// ════════════════════════════════════════════════════════════════════════════

/**
 * @brief Computed batch-size constraint derived from model batch mode and active
 *        recurrent bindings.
 * 
 * The widget applies this result to the batch-size spinbox: when @c min == @c max
 * the spinbox is locked to that single value; @c forced_by_recurrent distinguishes
 * the tooltip message.
 */
struct BatchSizeConstraint {
    int min = 1;                     ///< Minimum allowed batch size
    int max = 0;                     ///< Maximum allowed batch size (0 = unlimited)
    bool forced_by_recurrent = false;///< True if locked to 1 because of active recurrent bindings
};

/**
 * @brief      Compute the batch-size constraint for a given model and current recurrent bindings.
 * @details    Priority (most restrictive wins):
 *             - Active recurrent bindings              → min=1, max=1, forced_by_recurrent=true
 *             - Model RecurrentOnlyBatch or FixedBatch{1} → min=1, max=1, forced_by_recurrent=false
 *             - Model FixedBatch{N>1}                  → min=N, max=N, forced_by_recurrent=false
 *             - Model DynamicBatch                     → min/max from batch mode, forced_by_recurrent=false
 * 
 * A recurrent binding is considered active when its @c output_slot_name is
 * non-empty, i.e. the user has chosen an output slot to feed back.
 * 
 * Priority (most restrictive wins):
 *   - Active recurrent bindings              → min=1, max=1, forced_by_recurrent=true
 *   - Model RecurrentOnlyBatch or FixedBatch{1} → min=1, max=1, forced_by_recurrent=false
 *   - Model FixedBatch{N>1}                  → min=N, max=N, forced_by_recurrent=false
 *   - Model DynamicBatch                     → min/max from batch mode, forced_by_recurrent=false
 * 
 * @param info                      Model display info (contains @c batch_mode).
 * @param active_recurrent_bindings Recurrent bindings from state; activity is
 *                                  determined by checking @c output_slot_name.
 * @return BatchSizeConstraint that the widget can apply to the spinbox.
 */
[[nodiscard]] BatchSizeConstraint computeBatchSizeConstraint(
        ModelDisplayInfo const & info,
        std::vector<RecurrentBindingData> const & active_recurrent_bindings);

// ════════════════════════════════════════════════════════════════════════════
// Post-encoder / decoder consistency
// ════════════════════════════════════════════════════════════════════════════

/**
 * @brief Return the valid decoder alternative names allowed for a given post-encoder
 *        module type.
 * 
 * Modules that collapse spatial dimensions (@c global_avg_pool and
 * @c spatial_point reduce @c [B,C,H,W] → @c [B,C]) restrict output decoders
 * to @c FeatureVectorDecoderParams only. All other module types (i.e. @c "none")
 * permit every decoder variant.
 * 
 * @param module_type  Post-encoder modu        le type string as returned by
 *                     PostEncoderWidget::moduleTypeForState(), e.g.
 *                     @c "none", @c "global_avg_pool", @c "spatial_point".
 * @return Names of the DecoderVariant alternatives that are compatible with
 *         the selected post-encoder module.
 */
[[nodiscard]] std::vector<std::string> validDecodersForModule(
        std::string const & module_type);

}// namespace dl::constraints

#endif// DEEP_LEARNING_CONSTRAINT_ENFORCER_HPP
