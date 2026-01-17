#ifndef WHISKERTOOLBOX_V2_REGISTERED_TRANSFORMS_HPP
#define WHISKERTOOLBOX_V2_REGISTERED_TRANSFORMS_HPP

/**
 * @file RegisteredTransforms.hpp
 * @brief Registration header for V2 transforms
 *
 * This header includes all registered transforms and their registrations.
 * It is intended to be included in translation units that need to execute
 * transforms by name.
 *
 * ## V2 Pattern
 *
 * The V2 transform system uses parameter bindings from PipelineValueStore
 * instead of preprocessing. To add statistical pre-computation to a transform:
 *
 * 1. Add pre-reductions to your pipeline JSON
 * 2. Add param_bindings to wire reduction outputs to transform parameters
 *
 * Example:
 * @code
 * {
 *   "pre_reductions": [
 *     {"reduction": "MeanValue", "output_key": "computed_mean"}
 *   ],
 *   "steps": [{
 *     "transform": "MyTransform",
 *     "param_bindings": {"mean": "computed_mean"}
 *   }]
 * }
 * @endcode
 *
 * @see PIPELINE_VALUE_STORE_ROADMAP.md for the full design
 * @see PipelineValueStore for value storage
 * @see ParameterBinding.hpp for binding mechanism
 */

#include "transforms/v2/algorithms/ZScoreNormalization/ZScoreNormalization.hpp"
#include "transforms/v2/core/PipelineLoader.hpp"

namespace WhiskerToolbox::Transforms::V2 {

// V2 transforms use parameter bindings instead of preprocessing.
// See PipelineValueStore and ParameterBinding.hpp for the new pattern.

} // namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_REGISTERED_TRANSFORMS_HPP
