/**
 * @file TensorToAnalog.hpp
 * @brief Container transform: extract TensorData columns as AnalogTimeSeries
 *
 * Phase 1 of the multi-channel storage roadmap.
 * Extracts selected columns from a 2D TensorData and returns them as
 * independent AnalogTimeSeries objects (owning copies).
 */

#ifndef WHISKERTOOLBOX_V2_TENSOR_TO_ANALOG_HPP
#define WHISKERTOOLBOX_V2_TENSOR_TO_ANALOG_HPP

#include <memory>
#include <string>
#include <vector>

class AnalogTimeSeries;
class TensorData;

namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Parameters for TensorToAnalog transform (reflect-cpp compatible)
 */
struct TensorToAnalogParams {
    /// Prefix for generated AnalogTimeSeries keys (used by DataManager integration)
    std::string output_key_prefix;

    /// Which columns to extract (0-based indices). Empty means all columns.
    std::vector<int> columns;
};

/**
 * @brief Extract columns from a 2D TensorData as independent AnalogTimeSeries
 *
 * Each selected column is copied into a new owning AnalogTimeSeries.
 * Time indices and TimeFrame are preserved from the source TensorData
 * when the tensor has TimeFrameIndex rows.
 *
 * @param input   2D TensorData to extract columns from
 * @param params  Parameters (columns to extract, output prefix)
 * @param ctx     Compute context for progress/cancellation/logging
 * @return Vector of AnalogTimeSeries (one per selected column), empty on failure
 *
 * @pre input must be 2D
 * @pre All column indices in params.columns must be in [0, input.numColumns())
 */
[[nodiscard]] auto tensorToAnalog(
        TensorData const & input,
        TensorToAnalogParams const & params,
        ComputeContext const & ctx) -> std::vector<std::shared_ptr<AnalogTimeSeries>>;

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_TENSOR_TO_ANALOG_HPP
