/**
 * @file TensorTemporalNeighbors.hpp
 * @brief TransformsV2 container transform that augments a time-indexed TensorData
 *        with feature columns from neighboring time indices (lag/lead).
 *
 * Operates on sorted row order in TimeIndexStorage (index-adjacent), not raw
 * frame ±1. Supports configurable boundary policies (NaN, Drop, Clamp, Zero).
 */

#ifndef WHISKERTOOLBOX_V2_TENSOR_TEMPORAL_NEIGHBORS_HPP
#define WHISKERTOOLBOX_V2_TENSOR_TEMPORAL_NEIGHBORS_HPP

#include <memory>
#include <vector>

class TensorData;
namespace WhiskerToolbox::Transforms::V2 {
struct ComputeContext;
}

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Policy for handling boundary rows where a neighbor offset is out-of-range
 */
enum class BoundaryPolicy {
    NaN,  ///< Fill missing neighbor features with NaN (default)
    Drop, ///< Drop rows where any offset is out-of-range
    Clamp,///< Clamp to the first/last row
    Zero  ///< Fill missing neighbor features with zero
};

/**
 * @brief Parameters for temporal neighbor feature augmentation (reflect-cpp compatible)
 *
 * Offsets are generated from separate lag and lead ranges:
 *   - Lags:  {-lag_step, -2*lag_step, ..., -lag_range}  (negative offsets, past)
 *   - Leads: {+lead_step, +2*lead_step, ..., +lead_range}  (positive offsets, future)
 *
 * Example: lag_range=3, lag_step=1, lead_range=2, lead_step=1
 *   → offsets: {-3, -2, -1, +1, +2}
 */
struct TensorTemporalNeighborParams {
    /// Maximum lag (past) offset magnitude; 0 disables lags
    int lag_range = 0;

    /// Step size for lag offsets (must be >= 1 when lag_range > 0)
    int lag_step = 1;

    /// Maximum lead (future) offset magnitude; 0 disables leads
    int lead_range = 0;

    /// Step size for lead offsets (must be >= 1 when lead_range > 0)
    int lead_step = 1;

    /// Boundary policy for rows where a neighbor offset is out-of-range
    BoundaryPolicy boundary_policy = BoundaryPolicy::NaN;

    /// Whether to include the original (unshifted) columns in the output
    bool include_original = true;

    /// Build the sorted offset list from lag/lead range and step parameters
    [[nodiscard]] std::vector<int> buildOffsets() const;
};


/**
 * @brief Augment a time-indexed TensorData with neighbor feature columns
 *
 * Container signature: TensorData → TensorData
 *
 * The input must be 2D with RowType::TimeFrameIndex. Offsets are generated
 * from the lag_range/lag_step and lead_range/lead_step parameters. For each
 * offset, the features from the neighbor row at position (current_row + offset)
 * in sorted time order are appended as new columns.
 *
 * Output column layout: [original cols (if include_original)] [offset_1 cols] [offset_2 cols] ...
 * Shifted columns are named "{original_col}_lag{offset}" (e.g. "feat1_lag-1").
 *
 * @pre input.ndim() == 2
 * @pre input.rowType() == RowType::TimeFrameIndex
 *
 * @param input  2D time-indexed TensorData
 * @param params Temporal neighbor parameters
 * @param ctx    Compute context for progress/cancellation
 * @return Augmented TensorData, or nullptr on validation failure
 */
[[nodiscard]] auto tensorTemporalNeighbors(
        TensorData const & input,
        TensorTemporalNeighborParams const & params,
        ComputeContext const & ctx) -> std::shared_ptr<TensorData>;

}// namespace WhiskerToolbox::Transforms::V2::Examples

#endif// WHISKERTOOLBOX_V2_TENSOR_TEMPORAL_NEIGHBORS_HPP
