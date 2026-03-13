/**
 * @file FeatureConverter.cpp
 * @brief Implementation of TensorData to arma::mat conversion with NaN handling and z-score normalization
 */

#include "FeatureConverter.hpp"

#include "Tensors/TensorData.hpp"

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
 * @brief Identify rows containing non-finite (NaN/Inf) values
 *
 * Returns a boolean vector where true = row is finite (keep), false = drop.
 */
std::vector<bool> identifyFiniteRows(arma::mat const & mat) {
    auto const num_rows = mat.n_rows;
    auto const num_cols = mat.n_cols;
    std::vector<bool> finite(num_rows, true);

    for (arma::uword r = 0; r < num_rows; ++r) {
        for (arma::uword c = 0; c < num_cols; ++c) {
            if (!std::isfinite(mat(r, c))) {
                finite[r] = false;
                break;
            }
        }
    }

    return finite;
}

/**
 * @brief Remove non-finite rows from the matrix, returning the surviving row indices
 */
std::pair<arma::mat, std::vector<std::size_t>> dropNonFiniteRows(arma::mat const & mat) {
    auto const finite = identifyFiniteRows(mat);
    auto const num_cols = mat.n_cols;

    // Count surviving rows
    std::size_t valid_count = 0;
    for (bool f: finite) {
        if (f) ++valid_count;
    }

    // If all rows are finite, return as-is
    if (valid_count == mat.n_rows) {
        std::vector<std::size_t> all_indices(mat.n_rows);
        for (std::size_t i = 0; i < mat.n_rows; ++i) {
            all_indices[i] = i;
        }
        return {mat, all_indices};
    }

    // Build filtered matrix
    arma::mat filtered(valid_count, num_cols);
    std::vector<std::size_t> valid_indices;
    valid_indices.reserve(valid_count);

    std::size_t out_row = 0;
    for (std::size_t r = 0; r < mat.n_rows; ++r) {
        if (finite[r]) {
            filtered.row(out_row) = mat.row(r);
            valid_indices.push_back(r);
            ++out_row;
        }
    }

    return {filtered, valid_indices};
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
    std::vector<double> means(n_features);
    std::vector<double> stds(n_features);

    for (arma::uword f = 0; f < n_features; ++f) {
        arma::rowvec row = matrix.row(f);
        double mean = arma::mean(row);
        double sd = arma::stddev(row, 0);// 0 = sample stddev (N-1)

        means[f] = mean;
        stds[f] = sd;

        matrix.row(f) = (row - mean) / (sd + epsilon);
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
        auto [cleaned, valid_indices] = dropNonFiniteRows(obs_matrix);
        result.rows_dropped = obs_matrix.n_rows - cleaned.n_rows;
        result.valid_row_indices = std::move(valid_indices);
        obs_matrix = std::move(cleaned);
    } else {
        result.rows_dropped = 0;
        result.valid_row_indices.resize(obs_matrix.n_rows);
        for (std::size_t i = 0; i < obs_matrix.n_rows; ++i) {
            result.valid_row_indices[i] = i;
        }
    }

    // Transpose to mlpack layout: features × observations
    result.matrix = obs_matrix.t();

    // Optionally z-score normalize (operates on features × observations)
    if (config.zscore_normalize) {
        auto [means, stds] = zscoreNormalize(result.matrix, config.zscore_epsilon);
        result.zscore_means = std::move(means);
        result.zscore_stds = std::move(stds);
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
        auto [cleaned, valid_indices] = dropNonFiniteRows(obs_matrix);
        result.rows_dropped = obs_matrix.n_rows - cleaned.n_rows;
        result.valid_row_indices = std::move(valid_indices);
        obs_matrix = std::move(cleaned);
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
            arma::vec col = result.matrix.col(c);
            double mean = arma::mean(col);
            double sd = arma::stddev(col, 0);

            result.zscore_means[c] = mean;
            result.zscore_stds[c] = sd;

            result.matrix.col(c) = (col - mean) / (sd + config.zscore_epsilon);
        }
    }

    return result;
}

}// namespace MLCore