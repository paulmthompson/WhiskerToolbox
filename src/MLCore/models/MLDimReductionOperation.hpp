/**
 * @file MLDimReductionOperation.hpp
 * @brief Abstract interface for dimensionality reduction operations (PCA, t-SNE, etc.)
 *
 * Extends MLModelOperation with a fit/transform pattern specific to
 * dimensionality reduction algorithms. Input and output are armadillo matrices
 * in mlpack convention (features × observations).
 */

#ifndef MLCORE_MLDIMREDUCTIONOPERATION_HPP
#define MLCORE_MLDIMREDUCTIONOPERATION_HPP

#include "MLCore/models/MLModelOperation.hpp"
#include "MLCore/models/MLModelParameters.hpp"

#include <armadillo>

#include <cstddef>
#include <vector>

namespace MLCore {

/**
 * @brief Abstract base for dimensionality reduction operations
 *
 * ## Fit/Transform workflow
 * ```
 * op->fitTransform(features, params, result);  // fit + transform in one call
 * op->transform(new_features, result);          // project new data (if supported)
 * auto ratio = op->explainedVarianceRatio();    // inspect explained variance
 * ```
 *
 * All matrices use mlpack convention: (features × observations) — each column
 * is one observation.
 *
 * @note Unlike clustering operations, dimensionality reduction produces a
 * transformed matrix rather than cluster labels. The output matrix has
 * `n_components` rows and the same number of columns as the input.
 */
class MLDimReductionOperation : public MLModelOperation {
public:
    ~MLDimReductionOperation() override = default;

    // ========================================================================
    // Dimensionality reduction interface
    // ========================================================================

    /**
     * @brief Fit the model to data and transform in a single call
     *
     * @param features  Input matrix (features × observations) in mlpack convention
     * @param params    Algorithm-specific parameters
     * @param result    Output: reduced matrix (n_components × observations)
     * @return true on success
     *
     * @pre features must not be empty (enforcement: runtime_check)
     * @pre features must not contain NaN or Inf values (enforcement: none) [IMPORTANT]
     * @pre params must be of the correct concrete type for this operation (enforcement: none)
     * @pre Features are expected to be z-score normalized (mean≈0, std≈1 per feature)
     *      when scale-sensitive comparison across features is desired.
     *      Normalization is the caller's responsibility (via FeatureConverter).
     *      Centering is handled internally by each algorithm. (enforcement: none) [IMPORTANT]
     * @post isTrained() returns true on success
     * @post result has n_components rows and features.n_cols columns
     */
    virtual bool fitTransform(
            arma::mat const & features,
            MLModelParametersBase const * params,
            arma::mat & result) = 0;

    /**
     * @brief Project new data using the fitted model
     *
     * @param features  Input matrix (features × observations)
     * @param result    Output: reduced matrix (n_components × observations)
     * @return true on success, false if not fitted or projection unsupported
     *
     * @pre isTrained() must be true (enforcement: runtime_check)
     * @pre features.n_rows must equal numFeatures() (enforcement: none) [IMPORTANT]
     * @pre features must not contain NaN or Inf values (enforcement: none) [IMPORTANT]
     * @pre Features must be z-score normalized using the SAME parameters (mean/std)
     *      that were used for the training data passed to fitTransform().
     *      Use FeatureConverter::applyZscoreNormalization() with stored parameters.
     *      (enforcement: none) [IMPORTANT]
     */
    virtual bool transform(
            arma::mat const & features,
            arma::mat & result) = 0;

    /**
     * @brief Return the number of output dimensions after reduction
     */
    [[nodiscard]] virtual std::size_t outputDimensions() const = 0;

    /**
     * @brief Return the explained variance ratio per component (if available)
     *
     * @return Vector of explained variance ratios (one per component),
     *         or empty if not applicable to this algorithm
     */
    [[nodiscard]] virtual std::vector<double> explainedVarianceRatio() const = 0;

    // ========================================================================
    // MLModelOperation overrides — route to dim reduction interface
    // ========================================================================

    /**
     * @brief Dimensionality reduction is always its own task type
     */
    [[nodiscard]] MLTaskType getTaskType() const override {
        return MLTaskType::DimensionalityReduction;
    }

    /**
     * @brief Route fit() to fitTransform() (discarding the result)
     */
    bool fit(
            arma::mat const & features,
            MLModelParametersBase const * params) override {
        arma::mat discard;
        return fitTransform(features, params, discard);
    }
};

}// namespace MLCore

#endif// MLCORE_MLDIMREDUCTIONOPERATION_HPP
