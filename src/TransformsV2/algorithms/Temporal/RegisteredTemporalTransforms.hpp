#ifndef WHISKERTOOLBOX_V2_REGISTERED_TEMPORAL_TRANSFORMS_HPP
#define WHISKERTOOLBOX_V2_REGISTERED_TEMPORAL_TRANSFORMS_HPP

/**
 * @file RegisteredTemporalTransforms.hpp
 * @brief Registration declarations for temporal transforms
 *
 * This file declares the registration functions for temporal transforms.
 * The actual registration happens in RegisteredTemporalTransforms.cpp.
 *
 * ## Included Transforms
 *
 * - NormalizeEventTime: EventWithId → NormalizedEvent
 * - NormalizeValueTime: TimeValuePoint → NormalizedValue
 *
 * ## Trial-aligned usage
 *
 * For per-trial alignment, use `NormalizeTimeParamsV2` with
 * `param_bindings` and `PipelineValueStore` from `GatherResult::buildTrialStore()`,
 * or `bindValueProjectionV2()` for value projections.
 *
 * @see NormalizeTime.hpp for transform implementation
 * @see PipelineValueStore.hpp
 */

namespace WhiskerToolbox::Transforms::V2::Temporal {

/**
 * @brief Register all temporal transforms with ElementRegistry
 *
 * This is called automatically at static initialization time
 * when RegisteredTemporalTransforms.cpp is linked.
 *
 * Registers:
 * - NormalizeEventTime: Normalize event times relative to alignment
 * - NormalizeValueTime: Normalize analog sample times relative to alignment
 */
void registerTemporalTransforms();

}// namespace WhiskerToolbox::Transforms::V2::Temporal

#endif// WHISKERTOOLBOX_V2_REGISTERED_TEMPORAL_TRANSFORMS_HPP
