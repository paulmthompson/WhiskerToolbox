#include "SumReduction.hpp"

#include "transforms/v2/core/ComputeContext.hpp"

#include <cmath>
#include <numeric>

namespace WhiskerToolbox::Transforms::V2::Examples {

std::vector<float> sumReduction(
    std::span<float const> values,
    SumReductionParams const& params) {
    
    if (values.empty()) {
        return {params.getDefaultValue()};
    }
    
    float sum = 0.0f;
    if (params.getIgnoreNan()) {
        // Sum only non-NaN values
        for (float value : values) {
            if (!std::isnan(value)) {
                sum += value;
            }
        }
    } else {
        // Standard sum
        sum = std::accumulate(values.begin(), values.end(), 0.0f);
    }
    
    return {sum};
}

/**
 * @brief Context-aware version with progress reporting
 */
std::vector<float> sumReductionWithContext(
    std::span<float const> values,
    SumReductionParams const& params,
    ComputeContext const& ctx) {
    
    (void)params;
    
    ctx.reportProgress(0);
    
    if (ctx.shouldCancel()) {
        return {0.0f};
    }
    
    if (values.empty()) {
        ctx.reportProgress(100);
        return {0.0f};
    }
    
    ctx.reportProgress(50);
    float sum = std::accumulate(values.begin(), values.end(), 0.0f);
    ctx.reportProgress(100);
    
    return {sum};
}

} // namespace WhiskerToolbox::Transforms::V2::Examples