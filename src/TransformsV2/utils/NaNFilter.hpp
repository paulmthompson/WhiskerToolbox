/**
 * @file NaNFilter.hpp
 * @brief NaN/Inf row detection and filtering for TensorData transforms
 *
 * Provides utilities for detecting and removing rows containing NaN or Inf
 * values from Armadillo matrices. Used by dimensionality reduction container
 * transforms (TensorPCA, TensorRobustPCA, TensorTSNE) to handle non-finite
 * input data.
 *
 * @see MLCore::FeatureConverter for the equivalent functionality used by the
 *      MLCore_Widget / DimReductionPipeline pathway. The logic here is
 *      intentionally duplicated to avoid adding a TransformsV2 → MLCore
 *      dependency for this lightweight utility.
 */

#ifndef WHISKERTOOLBOX_V2_NANFILTER_HPP
#define WHISKERTOOLBOX_V2_NANFILTER_HPP

#include <armadillo>

#include <cstddef>
#include <vector>

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief Policy for handling NaN/Inf rows in dimensionality reduction transforms
 */
enum class NaNPolicy {
    Fail,     ///< Return nullptr if any NaN/Inf rows are present
    Propagate,///< Skip NaN rows for computation, fill them with NaN in the output
    Drop      ///< Remove NaN rows entirely (output has fewer rows than input)
};

/**
 * @brief Result of filtering non-finite rows from a matrix
 */
struct NaNFilterResult {
    /// Matrix with only finite rows (observations × features layout)
    arma::mat clean_matrix;

    /// Indices of original rows that survived filtering (maps output row → input row)
    std::vector<std::size_t> valid_row_indices;

    /// Number of rows removed
    std::size_t rows_dropped{0};
};

/**
 * @brief Check whether any row of a float matrix contains NaN or Inf
 *
 * Scans in row-major order with early exit on the first non-finite element.
 *
 * @param fmat Matrix to check (any layout)
 * @return true if at least one element is NaN or Inf
 */
[[nodiscard]] bool hasNonFiniteRows(arma::fmat const & fmat);

/**
 * @brief Remove rows containing NaN or Inf from an observations × features matrix
 *
 * Returns the cleaned matrix together with the surviving row indices.
 * If all rows are finite, returns the original matrix and a full index vector
 * without copying.
 *
 * @param obs_matrix Matrix in observations × features layout (double precision)
 * @return NaNFilterResult with clean matrix, valid indices, and drop count
 *
 * @pre obs_matrix must not be empty
 * @post result.clean_matrix has no NaN or Inf values
 * @post result.valid_row_indices.size() == result.clean_matrix.n_rows
 * @post result.rows_dropped == obs_matrix.n_rows - result.clean_matrix.n_rows
 */
[[nodiscard]] NaNFilterResult filterNonFiniteRows(arma::mat const & obs_matrix);

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_NANFILTER_HPP
