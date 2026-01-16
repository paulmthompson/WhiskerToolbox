#ifndef WHISKERTOOLBOX_V2_REGISTERED_TRANSFORMS_HPP
#define WHISKERTOOLBOX_V2_REGISTERED_TRANSFORMS_HPP

#include "transforms/v2/algorithms/ZScoreNormalization/ZScoreNormalization.hpp"
#include "transforms/v2/core/PipelineLoader.hpp"

namespace WhiskerToolbox::Transforms::V2 {

struct PipelineStep;
// Overload tryAllRegisteredPreprocessing to actually try preprocessing types
// This overload will be found by ADL when RegisteredTransforms.hpp is included

template<typename View>
inline void tryAllRegisteredPreprocessing(PipelineStep const& step, View const& view) {
    // Try each registered parameter type
    // Only the matching type will succeed; others return false immediately
    if (step.template tryPreprocessTyped<View, ZScoreNormalizationParams>(view)) return;
    // Add more types here as they're created
}

} // namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_REGISTERED_TRANSFORMS_HPP
