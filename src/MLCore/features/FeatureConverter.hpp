#ifndef MLCORE_FEATURECONVERTER_HPP
#define MLCORE_FEATURECONVERTER_HPP

/**
 * @file FeatureConverter.hpp
 * @brief Converts TensorData feature matrices to arma::mat for mlpack consumption
 *
 * MLPack algorithms operate on double-precision arma::mat in column-major layout
 * with observations as *columns* (features × observations). This converter handles:
 *
 * - Materialization of lazy columns (via TensorData::getColumn)
 * - Float → double precision conversion
 * - NaN/Inf row removal
 * - Optional z-score normalization
 * - Transposition to mlpack's expected layout (features × observations)
 *
 * The resulting ConvertedFeatures struct tracks which rows survived NaN dropping
 * so that predictions can be mapped back to the original time frames.
 *
 * @see ml_library_roadmap.md §3.4.3
 */

#include <armadillo>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// Forward declaration
class TensorData;

namespace MLCore {

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Configuration for TensorData → arma::mat conversion
 */
struct ConversionConfig {
    bool drop_nan = true;              ///< Remove rows containing NaN or Inf
    bool zscore_normalize = false;     ///< Z-score normalize each column (mean=0, std=1)
    double zscore_epsilon = 1e-10;     ///< Added to std to avoid division by zero
};

// ============================================================================
// Result types
// ============================================================================

/**
 * @brief Result of converting a TensorData to arma::mat
 */
struct ConvertedFeatures {
    /**
     * @brief The feature matrix in mlpack layout: features × observations (double)
     *
     * Each column is one observation, each row is one feature.
     * This is the transpose of the TensorData layout (rows × columns).
     */
    arma::mat matrix;

    /**
     * @brief Column names from the original TensorData (one per feature / matrix row)
     */
    std::vector<std::string> column_names;

    /**
     * @brief Indices of original tensor rows that survived NaN/Inf dropping
     *
     * Maps matrix column index → original tensor row index.
     * If drop_nan was false, this is simply {0, 1, ..., N-1}.
     * Useful for mapping predictions back to time frames.
     */
    std::vector<std::size_t> valid_row_indices;

    /**
     * @brief Number of rows dropped due to NaN/Inf (0 if drop_nan was false)
     */
    std::size_t rows_dropped{0};

    /**
     * @brief Per-column mean values computed during z-score normalization
     *
     * Empty if zscore_normalize was false. Size = number of features.
     * Useful for normalizing prediction-time data with the same parameters.
     */
    std::vector<double> zscore_means;

    /**
     * @brief Per-column standard deviation values computed during z-score normalization
     *
     * Empty if zscore_normalize was false. Size = number of features.
     */
    std::vector<double> zscore_stds;
};

// ============================================================================
// Conversion functions
// ============================================================================

/**
 * @brief Convert a TensorData feature matrix to the arma::mat format expected by mlpack
 *
 * This is the primary conversion entry point. It:
 * 1. Validates the tensor is non-empty and 2D
 * 2. Materializes all columns (handles lazy backends transparently)
 * 3. Converts float → double
 * 4. Optionally drops rows containing NaN/Inf values
 * 5. Optionally z-score normalizes each column
 * 6. Transposes to mlpack layout (features × observations)
 *
 * @param tensor The feature tensor (must be 2D, non-empty)
 * @param config Conversion parameters
 * @return ConvertedFeatures with the matrix and metadata
 * @throws std::invalid_argument if tensor is empty or not 2D
 */
[[nodiscard]] ConvertedFeatures convertTensorToArma(
    TensorData const & tensor,
    ConversionConfig const & config = {});

/**
 * @brief Convert a TensorData to a row-major arma::mat (observations × features)
 *
 * Similar to convertTensorToArma but preserves the TensorData's natural layout
 * where rows are observations and columns are features. Useful when you need
 * the matrix in the same orientation as the tensor (e.g., for visualization).
 *
 * @param tensor The feature tensor
 * @param config Conversion parameters
 * @return ConvertedFeatures with matrix in observations × features layout
 * @throws std::invalid_argument if tensor is empty or not 2D
 */
[[nodiscard]] ConvertedFeatures convertTensorToArmaRowMajor(
    TensorData const & tensor,
    ConversionConfig const & config = {});

/**
 * @brief Apply z-score normalization to an arma::mat in-place
 *
 * Normalizes each row (feature) of a features × observations matrix
 * to zero mean and unit standard deviation.
 *
 * @param matrix The matrix to normalize (modified in-place)
 * @param epsilon Added to std to prevent division by zero
 * @return Pair of {means, stds} vectors (one per row/feature)
 */
[[nodiscard]] std::pair<std::vector<double>, std::vector<double>> zscoreNormalize(
    arma::mat & matrix,
    double epsilon = 1e-10);

/**
 * @brief Apply previously computed z-score parameters to new data
 *
 * Useful for normalizing prediction-time data with the same parameters
 * used during training.
 *
 * @param matrix The matrix to normalize (features × observations, modified in-place)
 * @param means Per-feature means (size must == matrix.n_rows)
 * @param stds Per-feature standard deviations (size must == matrix.n_rows)
 * @param epsilon Added to std to prevent division by zero
 * @throws std::invalid_argument if means/stds size doesn't match matrix rows
 */
void applyZscoreNormalization(
    arma::mat & matrix,
    std::vector<double> const & means,
    std::vector<double> const & stds,
    double epsilon = 1e-10);

} // namespace MLCore

#endif // MLCORE_FEATURECONVERTER_HPP