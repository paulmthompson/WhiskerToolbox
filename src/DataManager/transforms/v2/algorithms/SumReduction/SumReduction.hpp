#ifndef WHISKERTOOLBOX_V2_SUM_REDUCTION_TRANSFORM_HPP
#define WHISKERTOOLBOX_V2_SUM_REDUCTION_TRANSFORM_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <optional>
#include <span>
#include <vector>

namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for sum reduction
 * 
 * Uses reflect-cpp for automatic JSON serialization/deserialization with validation.
 * 
 * Example JSON:
 * ```json
 * {
 *   "ignore_nan": true,
 *   "default_value": 0.0
 * }
 * ```
 */
struct SumReductionParams {
    // Whether to ignore NaN values when summing
    std::optional<bool> ignore_nan;

    // Default value to return if input is empty
    std::optional<float> default_value;

    // Helper methods to get values with defaults
    bool getIgnoreNan() const {
        return ignore_nan.value_or(false);
    }

    float getDefaultValue() const {
        return default_value.value_or(0.0f);
    }
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
std::vector<float> sumReduction(
        std::span<float const> values,
        SumReductionParams const & params);
/**
 * @brief Context-aware version with progress reporting
 */
std::vector<float> sumReductionWithContext(
        std::span<float const> values,
        SumReductionParams const & params,
        ComputeContext const & ctx);

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_SUM_REDUCTION_TRANSFORM_HPP
