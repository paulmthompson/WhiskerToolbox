/**
 * @file FeatureConverter.cpp
 * @brief Implementation of TensorData to arma::mat conversion with NaN handling and z-score normalization
 */

#include "FeatureConverter.hpp"

#include "CoreMath/non_finite_rows.hpp"
#include "Tensors/TensorData.hpp"

#include <spdlog/spdlog.h>

#include <cmath>
#include <sstream>
#include <stdexcept>

namespace MLCore {

namespace {

/**
 * @brief Build the observations × features double matrix from TensorData columns
 *
 * Materializes all columns from the tensor (handles lazy backends) and
 * converts float → double. Returns the matrix in observations × features
 * layout (same as TensorData: rows = observations, cols = features).
 */
arma::mat buildObservationsMatrix(TensorData const & tensor) {
    auto const num_rows = tensor.numRows();
    auto const num_cols = tensor.numColumns();

    // Build observations × features matrix
    arma::mat mat(num_rows, num_cols);

    for (std::size_t c = 0; c < num_cols; ++c) {
        auto const col_data = tensor.getColumn(c);
        for (std::size_t r = 0; r < num_rows; ++r) {
            mat(r, c) = static_cast<double>(col_data[r]);
        }
    }

    return mat;
}

/**
 * @brief Validate tensor for conversion, throwing on errors
 */
void validateForConversion(TensorData const & tensor) {
    if (tensor.isEmpty()) {
        throw std::invalid_argument("Cannot convert empty TensorData to arma::mat");
    }

    if (tensor.ndim() != 2) {
        std::ostringstream oss;
        oss << "TensorData must be 2D for conversion, but has " << tensor.ndim() << " dimensions";
        throw std::invalid_argument(oss.str());
    }

    if (tensor.numRows() == 0) {
        throw std::invalid_argument("TensorData has zero rows");
    }

    if (tensor.numColumns() == 0) {
        throw std::invalid_argument("TensorData has zero columns");
    }
}

}// anonymous namespace

// ============================================================================
// Z-score normalization
// ============================================================================

std::pair<std::vector<double>, std::vector<double>> zscoreNormalize(
        arma::mat & matrix,
        double epsilon) {
    auto const n_features = matrix.n_rows;
    auto const n_observations = matrix.n_cols;
    std::vector<double> means(n_features);
    std::vector<double> stds(n_features);

    spdlog::debug("[zscoreNormalize] Matrix shape: {} rows (features) x {} cols (observations), "
                  "epsilon={:.2e}",
                  n_features, n_observations, epsilon);

    for (arma::uword f = 0; f < n_features; ++f) {
        arma::rowvec const row = matrix.row(f);
        double const mean = arma::mean(row);
        double const sd = arma::stddev(row, 0);// 0 = sample stddev (N-1)

        means[f] = mean;
        stds[f] = sd;

        matrix.row(f) = (row - mean) / (sd + epsilon);
    }

    // Log summary statistics for first few and last few features
    auto const n_log = std::min(static_cast<arma::uword>(3), n_features);
    for (arma::uword f = 0; f < n_log; ++f) {
        spdlog::debug("[zscoreNormalize]   feature[{}]: pre-mean={:.4f}, pre-std={:.4f}, "
                      "post-mean={:.4e}, post-std={:.4f}",
                      f, means[f], stds[f],
                      arma::mean(matrix.row(f)), arma::stddev(matrix.row(f), 0));
    }
    if (n_features > 6) {
        spdlog::debug("[zscoreNormalize]   ... ({} features omitted) ...", n_features - 6);
    }
    for (arma::uword f = (n_features > 3 ? n_features - 3 : n_log); f < n_features; ++f) {
        spdlog::debug("[zscoreNormalize]   feature[{}]: pre-mean={:.4f}, pre-std={:.4f}, "
                      "post-mean={:.4e}, post-std={:.4f}",
                      f, means[f], stds[f],
                      arma::mean(matrix.row(f)), arma::stddev(matrix.row(f), 0));
    }

    return {means, stds};
}

void applyZscoreNormalization(
        arma::mat & matrix,
        std::vector<double> const & means,
        std::vector<double> const & stds,
        double epsilon) {
    if (means.size() != matrix.n_rows || stds.size() != matrix.n_rows) {
        std::ostringstream oss;
        oss << "Z-score parameter size mismatch: matrix has " << matrix.n_rows
            << " rows but means has " << means.size()
            << " and stds has " << stds.size() << " entries";
        throw std::invalid_argument(oss.str());
    }

    for (arma::uword f = 0; f < matrix.n_rows; ++f) {
        matrix.row(f) = (matrix.row(f) - means[f]) / (stds[f] + epsilon);
    }
}

// ============================================================================
// Main conversion functions
// ============================================================================

ConvertedFeatures convertTensorToArma(
        TensorData const & tensor,
        ConversionConfig const & config) {
    validateForConversion(tensor);

    spdlog::debug("[convertTensorToArma] Input tensor: {} rows x {} cols, "
                  "drop_nan={}, zscore_normalize={}",
                  tensor.numRows(), tensor.numColumns(),
                  config.drop_nan, config.zscore_normalize);

    ConvertedFeatures result;

    // Collect column names
    if (tensor.hasNamedColumns()) {
        result.column_names = tensor.columnNames();
    } else {
        result.column_names.resize(tensor.numColumns());
        for (std::size_t i = 0; i < tensor.numColumns(); ++i) {
            result.column_names[i] = "feature_" + std::to_string(i);
        }
    }

    // Build observations × features matrix (double precision)
    arma::mat obs_matrix = buildObservationsMatrix(tensor);

    // Optionally drop NaN/Inf rows
    if (config.drop_nan) {
        auto const filtered = filterNonFiniteRows(obs_matrix);
        result.rows_dropped = filtered.rows_dropped;
        result.valid_row_indices = filtered.valid_row_indices;
        obs_matrix = filtered.clean_matrix;
    } else {
        result.rows_dropped = 0;
        result.valid_row_indices.resize(obs_matrix.n_rows);
        for (std::size_t i = 0; i < obs_matrix.n_rows; ++i) {
            result.valid_row_indices[i] = i;
        }
    }

    // Transpose to mlpack layout: features × observations
    result.matrix = obs_matrix.t();

    spdlog::debug("[convertTensorToArma] After transpose: {} rows (features) x {} cols (observations)",
                  result.matrix.n_rows, result.matrix.n_cols);

    // Log pre-normalization feature-level statistics
    if (result.matrix.n_cols > 0) {
        auto const n_feat = result.matrix.n_rows;
        auto const n_log = std::min(static_cast<arma::uword>(3), n_feat);
        for (arma::uword f = 0; f < n_log; ++f) {
            spdlog::debug("[convertTensorToArma] Pre-zscore feature[{}]: "
                          "min={:.4f}, max={:.4f}, mean={:.4f}, std={:.4f}",
                          f,
                          arma::min(result.matrix.row(f)),
                          arma::max(result.matrix.row(f)),
                          arma::mean(result.matrix.row(f)),
                          arma::stddev(result.matrix.row(f), 0));
        }
    }

    // Optionally z-score normalize (operates on features × observations)
    if (config.zscore_normalize) {
        spdlog::debug("[convertTensorToArma] Applying z-score normalization");
        auto [means, stds] = zscoreNormalize(result.matrix, config.zscore_epsilon);
        result.zscore_means = std::move(means);
        result.zscore_stds = std::move(stds);
        spdlog::debug("[convertTensorToArma] Z-score normalization complete. "
                      "Stored {} means and {} stds",
                      result.zscore_means.size(), result.zscore_stds.size());
    } else {
        spdlog::debug("[convertTensorToArma] Z-score normalization SKIPPED (disabled)");
    }

    return result;
}

ConvertedFeatures convertTensorToArmaRowMajor(
        TensorData const & tensor,
        ConversionConfig const & config) {
    validateForConversion(tensor);

    ConvertedFeatures result;

    // Collect column names
    if (tensor.hasNamedColumns()) {
        result.column_names = tensor.columnNames();
    } else {
        result.column_names.resize(tensor.numColumns());
        for (std::size_t i = 0; i < tensor.numColumns(); ++i) {
            result.column_names[i] = "feature_" + std::to_string(i);
        }
    }

    // Build observations × features matrix (double precision)
    arma::mat obs_matrix = buildObservationsMatrix(tensor);

    // Optionally drop NaN/Inf rows
    if (config.drop_nan) {
        auto const filtered = filterNonFiniteRows(obs_matrix);
        result.rows_dropped = filtered.rows_dropped;
        result.valid_row_indices = filtered.valid_row_indices;
        obs_matrix = filtered.clean_matrix;
    } else {
        result.rows_dropped = 0;
        result.valid_row_indices.resize(obs_matrix.n_rows);
        for (std::size_t i = 0; i < obs_matrix.n_rows; ++i) {
            result.valid_row_indices[i] = i;
        }
    }

    // Keep observations × features layout (no transpose)
    result.matrix = std::move(obs_matrix);

    // Optionally z-score normalize — normalize each column (feature)
    if (config.zscore_normalize) {
        auto const n_cols = result.matrix.n_cols;
        result.zscore_means.resize(n_cols);
        result.zscore_stds.resize(n_cols);

        for (arma::uword c = 0; c < n_cols; ++c) {
            arma::vec const col = result.matrix.col(c);
            double const mean = arma::mean(col);
            double const sd = arma::stddev(col, 0);

            result.zscore_means[c] = mean;
            result.zscore_stds[c] = sd;

            result.matrix.col(c) = (col - mean) / (sd + config.zscore_epsilon);
        }
    }

    return result;
}

}// namespace MLCore