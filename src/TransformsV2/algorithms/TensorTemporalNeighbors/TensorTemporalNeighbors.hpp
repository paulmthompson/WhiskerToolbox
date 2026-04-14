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

#include "ParameterSchema/ParameterSchema.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
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
 */
struct TensorTemporalNeighborParams {
    /// Integer offsets relative to each row (e.g. {-2, -1, +1})
    std::vector<int> offsets;

    /// Boundary policy for rows where a neighbor offset is out-of-range
    BoundaryPolicy boundary_policy = BoundaryPolicy::NaN;

    /// Subset of source column indices to shift; empty/nullopt means all columns
    std::optional<std::vector<std::size_t>> column_indices;

    /// Whether to include the original (unshifted) columns in the output
    bool include_original = true;
};

}// namespace WhiskerToolbox::Transforms::V2::Examples

/**
 * @brief ParameterUIHints specialization for TensorTemporalNeighborParams
 */
template<>
struct ParameterUIHints<WhiskerToolbox::Transforms::V2::Examples::TensorTemporalNeighborParams> {
    static void annotate(ParameterSchema & schema) {
        if (auto * f = schema.field("offsets")) {
            f->tooltip = "Integer offsets relative to each row in sorted time order "
                         "(e.g. -1 = previous row, +1 = next row)";
        }
        if (auto * f = schema.field("boundary_policy")) {
            f->tooltip = "How to handle rows where an offset falls outside the tensor";
        }
        if (auto * f = schema.field("column_indices")) {
            f->tooltip = "Subset of source column indices to shift (empty = all columns)";
        }
        if (auto * f = schema.field("include_original")) {
            f->tooltip = "Include the original (unshifted) columns in the output";
        }
    }
};

namespace WhiskerToolbox::Transforms::V2::Examples {

/**
 * @brief Augment a time-indexed TensorData with neighbor feature columns
 *
 * Container signature: TensorData → TensorData
 *
 * The input must be 2D with RowType::TimeFrameIndex. For each specified offset,
 * the features (or a column subset) from the neighbor row at position
 * (current_row + offset) in sorted time order are appended as new columns.
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
