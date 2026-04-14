/**
 * @file DimReductionOutputBuilder.hpp
 * @brief Shared output tensor construction for dimensionality reduction transforms
 *
 * Provides a single function that all dimensionality reduction container
 * transforms (TensorPCA, TensorRobustPCA, TensorTSNE) delegate to for
 * reconstructing a TensorData from the reduced arma::mat result.
 *
 * Handles all three RowTypes (TimeFrameIndex, Interval, Ordinal) and both
 * NaN policies (Propagate: fill NaN positions; Drop: output only valid rows).
 *
 * @see MLCore::DimReductionPipeline for the equivalent buildOutputTensor()
 *      used by the MLCore_Widget pathway.
 */

#ifndef WHISKERTOOLBOX_V2_DIMREDUCTION_OUTPUT_BUILDER_HPP
#define WHISKERTOOLBOX_V2_DIMREDUCTION_OUTPUT_BUILDER_HPP

#include "NaNFilter.hpp"

#include <armadillo>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class TensorData;

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief Build an output TensorData from a reduced matrix, handling NaN policies
 *
 * Converts the algorithm result (n_components × valid_observations, mlpack layout)
 * into a TensorData that preserves the input's RowDescriptor. The NaN policy
 * determines how rows that were filtered during computation are handled:
 *
 * - **Propagate**: Output has the same row count as the input. Rows that were
 *   filtered out (NaN/Inf) get NaN-filled output. Preserves temporal alignment.
 * - **Drop**: Output has only the valid rows. TimeFrameIndex and Interval rows
 *   are filtered to match.
 *
 * @param reduced       Algorithm result matrix (n_components × valid_observations)
 * @param input         Original input TensorData (for RowDescriptor, TimeFrame)
 * @param valid_indices Mapping from valid observation index → original row index
 * @param policy        NaN policy (only Propagate and Drop are relevant here)
 * @param n_components  Number of output columns
 * @param col_names     Column names for the output (e.g. "PC1", "PC2")
 * @return Output TensorData, or nullptr on construction failure
 *
 * @pre reduced.n_cols == valid_indices.size()
 * @pre reduced.n_rows == n_components
 * @pre col_names.size() == n_components
 */
[[nodiscard]] std::shared_ptr<TensorData> buildDimReductionOutput(
        arma::mat const & reduced,
        TensorData const & input,
        std::vector<std::size_t> const & valid_indices,
        NaNPolicy policy,
        std::size_t n_components,
        std::vector<std::string> const & col_names);

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_DIMREDUCTION_OUTPUT_BUILDER_HPP
