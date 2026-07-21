/**
 * @file non_finite_rows.hpp
 * @brief NaN/Inf row detection and filtering for Armadillo observation matrices
 */
#ifndef COREMATH_NON_FINITE_ROWS_HPP
#define COREMATH_NON_FINITE_ROWS_HPP

#include <armadillo>

#include <cstddef>
#include <span>
#include <vector>

/**
 * @brief Result of filtering non-finite rows from a matrix
 */
struct NonFiniteRowFilterResult {
    /// Matrix with only finite rows (observations × features layout)
    arma::mat clean_matrix;

    /// Indices of original rows that survived filtering (maps output row → input row)
    std::vector<std::size_t> valid_row_indices;

    /// Number of rows removed
    std::size_t rows_dropped{0};
};

/**
 * @brief Check whether any value in a contiguous row slice is non-finite
 *
 * @param values Row values to scan
 * @return true if at least one element is NaN or Inf
 */
[[nodiscard]] bool rowHasNonFinite(std::span<float const> values);

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
 * @return NonFiniteRowFilterResult with clean matrix, valid indices, and drop count
 *
 * @pre obs_matrix must not be empty
 * @post result.clean_matrix has no NaN or Inf values
 * @post result.valid_row_indices.size() == result.clean_matrix.n_rows
 * @post result.rows_dropped == obs_matrix.n_rows - result.clean_matrix.n_rows
 */
[[nodiscard]] NonFiniteRowFilterResult filterNonFiniteRows(arma::mat const & obs_matrix);

/**
 * @brief Check whether a row in column-major materialized storage contains NaN or Inf
 *
 * @param columns Materialized tensor columns (each column vector has one value per row)
 * @param row Row index to check
 * @return true if any column value at @p row is non-finite
 *
 * @pre row < columns.front().size() when columns is non-empty
 */
[[nodiscard]] bool rowHasNonFiniteAcrossColumns(
        std::vector<std::vector<float>> const & columns,
        std::size_t row);

#endif// COREMATH_NON_FINITE_ROWS_HPP
