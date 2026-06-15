/**
 * @file ConstraintEnforcer.hpp
 * @brief Session constraint computation for deep learning inference.
 */

#ifndef DEEP_LEARNING_CONSTRAINT_ENFORCER_HPP
#define DEEP_LEARNING_CONSTRAINT_ENFORCER_HPP

#include "bindings/DeepLearningBindingData.hpp"
#include "bindings/SlotBindingTypes.hpp"

#include <string>
#include <vector>

namespace dl {
struct ModelInfo;
}

namespace dl::constraints {

// ════════════════════════════════════════════════════════════════════════════
// Batch-size constraint
// ════════════════════════════════════════════════════════════════════════════

/**
 * @brief Computed batch-size constraint derived from model batch mode and active
 *        recurrent bindings.
 *
 * When @c min == @c max the effective batch size is locked to that single value.
 * @c forced_by_recurrent distinguishes recurrent-binding locks from model locks.
 */
struct BatchSizeConstraint {
    int min = 1;                      ///< Minimum allowed batch size
    int max = 0;                      ///< Maximum allowed batch size (0 = unlimited)
    bool forced_by_recurrent = false; ///< True if locked to 1 because of active recurrent bindings
};

/**
 * @brief      Compute the batch-size constraint for a given model and memory frames.
 * @details    Priority (most restrictive wins):
 *             - Active recurrent memory frames           → min=1, max=1, forced_by_recurrent=true
 *             - Model RecurrentOnlyBatch or FixedBatch{1} → min=1, max=1, forced_by_recurrent=false
 *             - Model FixedBatch{N>1}                  → min=N, max=N, forced_by_recurrent=false
 *             - Model DynamicBatch                     → min/max from batch mode, forced_by_recurrent=false
 *
 * @param info           Model display info (contains @c batch_mode).
 * @param memory_frames  Session memory frame bindings.
 * @return BatchSizeConstraint describing the allowed batch-size range.
 */
[[nodiscard]] BatchSizeConstraint computeBatchSizeConstraint(
        dl::ModelInfo const & info,
        std::vector<MemoryFrameBinding> const & memory_frames);

// ════════════════════════════════════════════════════════════════════════════
// Post-encoder / decoder consistency
// ════════════════════════════════════════════════════════════════════════════

/**
 * @brief Return valid decoder alternatives for the current post-encoder params.
 *
 * Modules that collapse spatial dimensions (per `PostEncoderModuleRegistry`
 * metadata, i.e. @c [B,C,H,W] → @c [B,C]) restrict output
 * decoders to `FeatureVectorDecoderParams` only. @c module_key @c "none" permits
 * every decoder variant.
 *
 * @param params  Post-encoder slot configuration from state or session.
 */
[[nodiscard]] std::vector<std::string> validDecodersForPostEncoder(
        dl::PostEncoderStepDescriptor const & params);

}// namespace dl::constraints

#endif// DEEP_LEARNING_CONSTRAINT_ENFORCER_HPP
