#ifndef WHISKERTOOLBOX_V2_SUM_REDUCTION_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_SUM_REDUCTION_TRANSFORM_HPP

#include "transforms/v2/core/ElementTransform.hpp"

#include <numeric>
#include <span>
#include <vector>

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for sum reduction
 */
struct SumReductionParams {
    // Could add options like:
    // bool ignore_nan = false;
    // float default_value = 0.0f;  // if empty input
};

/**
 * @brief Sum all floats at a given time point into a single float
 * 
 * This is a time-grouped transform: Range<float> → Range<float>
 * It takes all floats at one time and produces one summed float.
 * 
 * Example:
 * - Input at time T: [10.0, 5.0, 3.0]
 * - Output at time T: [18.0]
 * 
 * Use case: Reducing RaggedAnalogTimeSeries → AnalogTimeSeries
 * 
 * @param values All float values at a single time point
 * @param params Parameters (currently unused)
 * @return Vector containing single summed value
 */
inline std::vector<float> sumReduction(
    std::span<float const> values,
    SumReductionParams const& params) {
    
    (void)params;  // Unused for now
    
    if (values.empty()) {
        return {0.0f};  // Return 0 for empty input
    }
    
    float sum = std::accumulate(values.begin(), values.end(), 0.0f);
    return {sum};  // Return vector with single element
}

/**
 * @brief Context-aware version with progress reporting
 */
inline std::vector<float> sumReductionWithContext(
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

#endif // WHISKERTOOLBOX_V2_SUM_REDUCTION_TRANSFORM_HPP
