/**
 * @file TensorCentralDifference.hpp
 * @brief TransformsV2 container transform that appends central-difference
 *        (delta) columns to a time-indexed TensorData.
 *
 * For each original column the central difference is computed as:
 *   delta[t] = (value[t+1] - value[t-1]) / 2
 *
 * Operates on sorted row order in TimeIndexStorage (index-adjacent).
 * Supports configurable boundary policies (NaN, Drop, Clamp, Zero).
 */

#ifndef WHISKERTOOLBOX_V2_TENSOR_CENTRAL_DIFFERENCE_HPP
#define WHISKERTOOLBOX_V2_TENSOR_CENTRAL_DIFFERENCE_HPP

#include <memory>

class TensorData;
namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Policy for handling boundary rows where t-1 or t+1 is out-of-range
 */
enum class DeltaBoundaryPolicy {
    NaN,  ///< Fill missing delta with NaN (default)
    Drop, ///< Drop boundary rows from the output
    Clamp,///< Clamp to the first/last row when computing the difference
    Zero  ///< Fill missing delta with zero
};

/**
 * @brief Parameters for central-difference feature augmentation (reflect-cpp compatible)
 *
 * The transform appends one delta column per original column. For an input with
 * N columns the output will have 2*N columns when include_original is true.
 */
struct TensorCentralDifferenceParams {
    /// Boundary policy for the first and last rows
    DeltaBoundaryPolicy boundary_policy = DeltaBoundaryPolicy::NaN;

    /// Whether to include the original (unshifted) columns in the output
    bool include_original = true;
};

/**
 * @brief Append central-difference columns to a time-indexed TensorData
 *
 * Container signature: TensorData → TensorData
 *
 * The input must be 2D with RowType::TimeFrameIndex. For each original column
 * a new column is appended containing the central difference:
 *   delta_c[t] = (value_c[t+1] - value_c[t-1]) / 2
 *
 * Output column layout (include_original=true):
 *   [original cols] [delta_col0] [delta_col1] ...
 *
 * Delta columns are named "{original_col}_delta" (e.g. "feat1_delta").
 *
 * @pre input.ndim() == 2
 * @pre input.rowType() == RowType::TimeFrameIndex
 *
 * @param input  2D time-indexed TensorData
 * @param params Central-difference parameters
 * @param ctx    Compute context for progress/cancellation
 * @return Augmented TensorData, or nullptr on validation failure
 */
[[nodiscard]] auto tensorCentralDifference(
        TensorData const & input,
        TensorCentralDifferenceParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<TensorData>;

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_TENSOR_CENTRAL_DIFFERENCE_HPP
