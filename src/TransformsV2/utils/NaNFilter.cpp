/**
 * @file NaNFilter.cpp
 * @brief Implementation of NaN/Inf row detection and filtering
 *
 * @see MLCore::FeatureConverter (FeatureConverter.cpp) for the equivalent
 *      implementation used by the MLCore_Widget pathway.
 */

#include "NaNFilter.hpp"

#include <cmath>

namespace WhiskerToolbox::Transforms::V2 {

bool hasNonFiniteRows(arma::fmat const & fmat) {
    for (arma::uword r = 0; r < fmat.n_rows; ++r) {
        for (arma::uword c = 0; c < fmat.n_cols; ++c) {
            if (!std::isfinite(fmat(r, c))) {
                return true;
            }
        }
    }
    return false;
}

NaNFilterResult filterNonFiniteRows(arma::mat const & obs_matrix) {
    auto const num_rows = obs_matrix.n_rows;
    auto const num_cols = obs_matrix.n_cols;

    // Identify finite rows
    std::vector<bool> finite(num_rows, true);
    for (arma::uword r = 0; r < num_rows; ++r) {
        for (arma::uword c = 0; c < num_cols; ++c) {
            if (!std::isfinite(obs_matrix(r, c))) {
                finite[r] = false;
                break;
            }
        }
    }

    // Count survivors
    std::size_t valid_count = 0;
    for (bool const f: finite) {
        if (f) {
            ++valid_count;
        }
    }

    // Fast path: all rows finite
    if (valid_count == num_rows) {
        std::vector<std::size_t> all_indices(num_rows);
        for (std::size_t i = 0; i < num_rows; ++i) {
            all_indices[i] = i;
        }
        return {obs_matrix, std::move(all_indices), 0};
    }

    // Build filtered matrix
    arma::mat filtered(valid_count, num_cols);
    std::vector<std::size_t> valid_indices;
    valid_indices.reserve(valid_count);

    std::size_t out_row = 0;
    for (std::size_t r = 0; r < num_rows; ++r) {
        if (finite[r]) {
            filtered.row(out_row) = obs_matrix.row(r);
            valid_indices.push_back(r);
            ++out_row;
        }
    }

    return {std::move(filtered), std::move(valid_indices), num_rows - valid_count};
}

}// namespace WhiskerToolbox::Transforms::V2
