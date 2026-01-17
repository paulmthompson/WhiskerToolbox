#ifndef WHISKERTOOLBOX_V2_REGISTERED_TRANSFORMS_HPP
#define WHISKERTOOLBOX_V2_REGISTERED_TRANSFORMS_HPP

/**
 * @file RegisteredTransforms.hpp
 * @brief Central header for all registered V2 transforms
 *
 * This header includes the PipelineLoader and core transform types.
 * 
 * ## V2 Pattern (Recommended)
 * The V2 pattern uses pre-reductions and parameter bindings instead of preprocessing:
 * 
 * 1. Use pre-reductions to compute statistics before pipeline execution
 * 2. Use parameter bindings to wire reduction outputs to transform parameters
 * 3. Store values in PipelineValueStore for access during pipeline execution
 * 
 * Example JSON pipeline:
 * ```json
 * {
 *   "pre_reductions": [{"type": "mean"}, {"type": "std"}],
 *   "bindings": {
 *     "mean": {"key": "mean"},
 *     "std": {"key": "std"}
 *   },
 *   "steps": [{"name": "zscore_normalization_v2"}]
 * }
 * ```
 * 
 * See PipelineValueStore.hpp and ParameterBinding.hpp for the V2 mechanisms.
 */

#include "transforms/v2/core/PipelineLoader.hpp"

namespace WhiskerToolbox::Transforms::V2 {

// V2 transforms use pre-reductions and parameter bindings instead of preprocessing.
// Include PipelineLoader.hpp for pipeline construction and execution.

} // namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_REGISTERED_TRANSFORMS_HPP
