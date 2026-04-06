/**
 * @file PCAOperation.hpp
 * @brief MLCore-native PCA dimensionality reduction (wraps mlpack::PCA)
 */

#ifndef MLCORE_PCAOPERATION_HPP
#define MLCORE_PCAOPERATION_HPP

#include "MLCore/models/MLDimReductionOperation.hpp"
#include "MLCore/models/MLModelParameters.hpp"

#include <armadillo>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace MLCore {

/**
 * @brief PCA dimensionality reduction via mlpack
 *
 * Performs Principal Component Analysis. Internally centers features
 * (subtracts the per-feature mean) before computing eigenvectors.
 * Feature scaling (z-score normalization) is intentionally NOT performed
 * here — it is the caller's responsibility to pre-scale via
 * FeatureConverter if desired.
 *
 * Stores the fitted model (eigenvectors, mean) for re-projection of
 * new data.
 *
 * ## Usage
 * ```cpp
 * PCAOperation pca;
 * PCAParameters params;
 * params.n_components = 3;
 *
 * arma::mat result;
 * pca.fitTransform(features, &params, result);
 * // result is (3 × observations)
 *
 * auto variance = pca.explainedVarianceRatio();
 * // variance[0] = fraction of variance from PC1, etc.
 * ```
 */
class PCAOperation : public MLDimReductionOperation {
public:
    PCAOperation();
    ~PCAOperation() override;

    PCAOperation(PCAOperation const &) = delete;
    PCAOperation & operator=(PCAOperation const &) = delete;

    PCAOperation(PCAOperation &&) noexcept;
    PCAOperation & operator=(PCAOperation &&) noexcept;

    // ========================================================================
    // MLModelOperation identity
    // ========================================================================

    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] std::unique_ptr<MLModelParametersBase> getDefaultParameters() const override;
    [[nodiscard]] bool isTrained() const override;
    [[nodiscard]] std::size_t numFeatures() const override;

    // ========================================================================
    // MLDimReductionOperation interface
    // ========================================================================

    /**
     * @brief Fit the PCA model to data and project in a single call
     *
     * @param features  Input matrix (features × observations) in mlpack convention
     * @param params    Algorithm-specific parameters (must be PCAParameters or null)
     * @param result    Output: reduced matrix (n_components × observations)
     * @return true on success, false on validation failure or eigendecomposition error
     *
     * @pre features must not be empty (enforcement: runtime_check)
     * @pre features must have at least 2 columns (observations) (enforcement: runtime_check)
     * @pre features must not contain NaN or Inf values — eigendecomposition
     *      produces garbage otherwise (enforcement: none) [IMPORTANT]
     * @pre params, if non-null, must be dynamic_castable to PCAParameters
     *      (enforcement: runtime_check — falls back to defaults if cast fails)
     * @pre PCAParameters::n_components must be > 0 (enforcement: runtime_check)
     * @pre PCAParameters::n_components must be <= features.n_rows (enforcement: runtime_check)
     *
     * @post On success: isTrained() == true, result has n_components rows
     *       and features.n_cols columns, numFeatures() == features.n_rows
     * @post On failure: isTrained() unchanged (may be set to false),
     *       result is unmodified
     */
    bool fitTransform(
            arma::mat const & features,
            MLModelParametersBase const * params,
            arma::mat & result) override;

    /**
     * @brief Project new data using the fitted PCA model
     *
     * @param features  Input matrix (features × observations) in mlpack convention
     * @param result    Output: reduced matrix (n_components × observations)
     * @return true on success, false if untrained or dimension mismatch
     *
     * @pre isTrained() must be true (enforcement: runtime_check)
     * @pre features must not be empty (enforcement: runtime_check)
     * @pre features.n_rows must equal numFeatures() (the feature count used
     *      during fitting) (enforcement: runtime_check)
     * @pre features must not contain NaN or Inf values — projection will
     *      propagate them into the result (enforcement: none) [IMPORTANT]
     *
     * @post result has outputDimensions() rows and features.n_cols columns
     */
    bool transform(
            arma::mat const & features,
            arma::mat & result) override;

    [[nodiscard]] std::size_t outputDimensions() const override;
    [[nodiscard]] std::vector<double> explainedVarianceRatio() const override;

    // ========================================================================
    // Serialization
    // ========================================================================

    bool save(std::ostream & out) const override;
    bool load(std::istream & in) override;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

}// namespace MLCore

#endif// MLCORE_PCAOPERATION_HPP
