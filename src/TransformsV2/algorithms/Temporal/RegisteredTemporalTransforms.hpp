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
 * ## Context-Aware Registration
 *
 * These transforms use NormalizeTimeParams which is context-aware.
 * The TransformPipeline will automatically inject TrialContext when
 * executing these transforms on trial-aligned data.
 *
 * @see NormalizeTime.hpp for transform implementation
 * @see ContextAwareParams.hpp for context injection infrastructure
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

}  // namespace WhiskerToolbox::Transforms::V2::Temporal

#endif  // WHISKERTOOLBOX_V2_REGISTERED_TEMPORAL_TRANSFORMS_HPP
