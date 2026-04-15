/**
 * @file ICAOperation.hpp
 * @brief MLCore-native ICA dimensionality reduction (wraps mlpack::Radical)
 */

#ifndef MLCORE_ICAOPERATION_HPP
#define MLCORE_ICAOPERATION_HPP

#include "MLCore/models/MLDimReductionOperation.hpp"
#include "MLCore/models/MLModelParameters.hpp"

#include <armadillo>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace MLCore {

/**
 * @brief ICA via mlpack's RADICAL algorithm
 *
 * Performs Independent Component Analysis using the RADICAL (Robust,
 * Accurate, Direct ICA aLgorithm). Decomposes the input matrix X into
 * independent components Y and a square unmixing matrix W such that
 * Y = W * X.
 *
 * Unlike PCA, ICA seeks statistical independence (not just uncorrelatedness).
 * The number of output dimensions equals the number of input features
 * (RADICAL does not reduce dimensionality).
 *
 * Stores the fitted unmixing matrix for re-projection of new data via
 * transform().
 *
 * ## Usage
 * ```cpp
 * ICAOperation ica;
 * ICAParameters params;
 *
 * arma::mat result;
 * ica.fitTransform(features, &params, result);
 * // result is (features × observations) — each row is an independent component
 *
 * // Project new data using the learned unmixing matrix
 * arma::mat new_result;
 * ica.transform(new_features, new_result);
 * ```
 *
 * @note RADICAL scales quadratically in the number of dimensions.
 *       Consider reducing dimensionality with PCA first for high-D data.
 * @note explainedVarianceRatio() returns an empty vector (not applicable to ICA).
 */
class ICAOperation : public MLDimReductionOperation {
public:
    ICAOperation();
    ~ICAOperation() override;

    ICAOperation(ICAOperation const &) = delete;
    ICAOperation & operator=(ICAOperation const &) = delete;

    ICAOperation(ICAOperation &&) noexcept;
    ICAOperation & operator=(ICAOperation &&) noexcept;

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
     * @brief Fit the ICA model to data and project in a single call
     *
     * @param features  Input matrix (features × observations) in mlpack convention
     * @param params    Algorithm-specific parameters (must be ICAParameters or null)
     * @param result    Output: independent components matrix (features × observations)
     * @return true on success, false on validation failure
     *
     * @pre features must not be empty (enforcement: runtime_check)
     * @pre features must have at least 2 columns (observations) (enforcement: runtime_check)
     * @pre features must not contain NaN or Inf values — RADICAL produces
     *      garbage otherwise (enforcement: none) [IMPORTANT]
     * @pre params, if non-null, must be dynamic_castable to ICAParameters
     *      (enforcement: runtime_check — falls back to defaults if cast fails)
     *
     * @post On success: isTrained() == true, result has features.n_rows rows
     *       and features.n_cols columns, numFeatures() == features.n_rows
     * @post On failure: isTrained() unchanged, result is unmodified
     */
    bool fitTransform(
            arma::mat const & features,
            MLModelParametersBase const * params,
            arma::mat & result) override;

    /**
     * @brief Project new data using the fitted unmixing matrix
     *
     * Computes result = W * features, where W is the unmixing matrix
     * learned during fitTransform().
     *
     * @param features  Input matrix (features × observations) in mlpack convention
     * @param result    Output: independent components (features × observations)
     * @return true on success, false if untrained or dimension mismatch
     *
     * @pre isTrained() must be true (enforcement: runtime_check)
     * @pre features must not be empty (enforcement: runtime_check)
     * @pre features.n_rows must equal numFeatures() (enforcement: runtime_check)
     *
     * @post result has outputDimensions() rows and features.n_cols columns
     */
    bool transform(
            arma::mat const & features,
            arma::mat & result) override;

    [[nodiscard]] std::size_t outputDimensions() const override;

    /// @note Always returns empty — explained variance is not defined for ICA.
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

#endif// MLCORE_ICAOPERATION_HPP
