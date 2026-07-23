/**
 * @file AnalogDifference.hpp
 * @brief TransformsV2 container transform that computes backward, central, or
 *        forward differences on an AnalogTimeSeries.
 *
 * Operates on index-adjacent samples in storage order (not raw TimeFrameIndex
 * frame gaps). Supports configurable window lag and edge policies.
 */

#ifndef NEURALYZER_V2_ANALOG_DIFFERENCE_HPP
#define NEURALYZER_V2_ANALOG_DIFFERENCE_HPP

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>

class AnalogTimeSeries;
namespace Neuralyzer::Transforms::V2 {
struct ComputeContext;
}

namespace Neuralyzer::Transforms::V2::Examples {

/**
 * @brief Difference method applied to each sample
 */
enum class DifferenceMethod {
    Backward,///< data[i] - data[i - window]
    Central, ///< (data[i + window] - data[i - window]) / (2 * window)
    Forward  ///< data[i + window] - data[i]
};

/**
 * @brief Policy for handling samples where the differencing window is incomplete
 */
enum class DifferenceEdgePolicy {
    Skip,///< Omit samples where the window is incomplete (default)
    NaN, ///< Fill with quiet_NaN()
    Zero,///< Fill with 0
    Clamp///< Clamp missing neighbors to the first/last sample
};

/**
 * @brief Parameters for analog time series differencing (reflect-cpp compatible)
 */
struct AnalogDifferenceParams {
    /// Difference method to apply
    DifferenceMethod method = DifferenceMethod::Backward;

    /// Window lag in index-adjacent samples (must be >= 1)
    rfl::Validator<int, rfl::Minimum<1>> window = 1;

    /// Edge behavior when the window extends beyond series boundaries
    DifferenceEdgePolicy edge_policy = DifferenceEdgePolicy::Skip;
};

/**
 * @brief Compute backward, central, or forward difference on an AnalogTimeSeries
 *
 * Container signature: AnalogTimeSeries → AnalogTimeSeries
 *
 * Each output sample is anchored at the same TimeFrameIndex as the corresponding
 * input index. With Skip edge policy, boundary samples are omitted from output.
 *
 * @pre params.window >= 1
 *
 * @param input  Input analog time series
 * @param params Differencing parameters
 * @param ctx    Compute context for progress/cancellation
 * @return New AnalogTimeSeries with differenced values, or nullptr on validation
 *         failure or when Skip policy eliminates all samples
 */
[[nodiscard]] auto analogDifference(
        AnalogTimeSeries const & input,
        AnalogDifferenceParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<AnalogTimeSeries>;

}// namespace Neuralyzer::Transforms::V2::Examples

#endif// NEURALYZER_V2_ANALOG_DIFFERENCE_HPP
