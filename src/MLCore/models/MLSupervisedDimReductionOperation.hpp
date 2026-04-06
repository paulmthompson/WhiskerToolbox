/**
 * @file MLSupervisedDimReductionOperation.hpp
 * @brief Abstract interface for supervised dimensionality reduction operations
 *
 * Parallel to MLDimReductionOperation but requires class labels during fitting.
 * Concrete implementations (e.g., LogitProjectionOperation) learn a labeled
 * projection that maximally separates classes, producing a C-dimensional output
 * where C is the number of classes.
 */

#ifndef MLCORE_MLSUPERVISEDDIMREDUCTIONOPERATION_HPP
#define MLCORE_MLSUPERVISEDDIMREDUCTIONOPERATION_HPP

#include "MLCore/models/MLModelOperation.hpp"
#include "MLCore/models/MLModelParameters.hpp"
#include "MLCore/models/MLTaskType.hpp"

#include <armadillo>

#include <cstddef>
#include <string>
#include <vector>

namespace MLCore {

/**
 * @brief Abstract base for supervised dimensionality reduction operations
 *
 * Unlike `MLDimReductionOperation`, these algorithms require class labels
 * during fitting to learn a discriminative projection. The output dimensionality
 * is determined by the number of classes (not a user parameter).
 *
 * ## Workflow
 * ```cpp
 * op->fitTransform(features, labels, params, result);  // fit + project in one call
 * op->transform(new_features, result);                 // project new unlabeled data
 * auto names = op->outputColumnNames();                // "Logit:0", "Logit:1", ...
 * ```
 *
 * All matrices use mlpack convention: (features × observations) — each column
 * is one observation. The output result has `outputDimensions()` rows and the
 * same number of columns as the input.
 *
 * @note `transform()` requires a prior call to `fitTransform()`. It uses the
 * fitted weight matrix directly and does not require labels.
 */
class MLSupervisedDimReductionOperation : public MLModelOperation {
public:
    ~MLSupervisedDimReductionOperation() override = default;

    // ========================================================================
    // Supervised dimensionality reduction interface
    // ========================================================================

    /**
     * @brief Fit the model on labeled data and project to reduced space
     *
     * @param features  Input matrix (D × N) in mlpack convention
     * @param labels    Label row vector (N,) with class indices in [0, C)
     * @param params    Algorithm-specific parameters
     * @param result    Output: reduced matrix (C × N) — one row per class logit
     * @return true on success
     *
     * @pre features must not be empty (enforcement: runtime_check)
     * @pre features must not contain NaN or Inf values (enforcement: none) [IMPORTANT]
     * @pre labels.n_elem must equal features.n_cols (enforcement: runtime_check)
     * @pre At least 2 distinct classes must appear in labels (enforcement: runtime_check)
     * @pre params must be of the correct concrete type for this operation (enforcement: none)
     * @pre Features are expected to be z-score normalized (mean≈0, std≈1 per feature)
     *      when scale-sensitive comparison across features is desired.
     *      Normalization is the caller's responsibility (via FeatureConverter).
     *      Centering is handled internally by each algorithm. (enforcement: none) [IMPORTANT]
     * @post isTrained() returns true on success
     * @post result has outputDimensions() rows and features.n_cols columns
     */
    virtual bool fitTransform(
            arma::mat const & features,
            arma::Row<std::size_t> const & labels,
            MLModelParametersBase const * params,
            arma::mat & result) = 0;

    /**
     * @brief Project new unlabeled data using the fitted weight matrix
     *
     * @param features  Input matrix (D × N) — must have same D as training data
     * @param result    Output: reduced matrix (C × N)
     * @return true on success, false if model not fitted or dimension mismatch
     *
     * @pre isTrained() must be true (enforcement: runtime_check)
     * @pre features.n_rows must equal numFeatures() (enforcement: runtime_check)
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
     * @brief Number of output dimensions (equals number of classes for logit projection)
     */
    [[nodiscard]] virtual std::size_t outputDimensions() const = 0;

    /**
     * @brief Column names for the output reduced matrix
     *
     * @return Vector of column names (one per output dimension), e.g. ["Logit:0", "Logit:1"]
     *
     * Default names are generated as "Logit:0", "Logit:1", etc. Concrete
     * implementations may override with class-specific names after fitting.
     */
    [[nodiscard]] virtual std::vector<std::string> outputColumnNames() const = 0;

    /**
     * @brief Whether the model requires all class labels for fitting
     *
     * Discriminative models (e.g. LogitProjection) need samples from every
     * class to learn a decision boundary ⇒ return true (the default).
     *
     * Non-discriminative models (e.g. SupervisedPCA) only need the "positive"
     * class subset to fit a projection ⇒ return false.  The pipeline uses
     * this flag to subset to only the positive class (label > 0) when all
     * rows already carry a label (e.g. interval-based labeling).
     */
    [[nodiscard]] virtual bool requiresAllClassesForFitting() const { return true; }

    // ========================================================================
    // MLModelOperation overrides
    // ========================================================================

    /**
     * @brief Supervised dimensionality reduction task type
     */
    [[nodiscard]] MLTaskType getTaskType() const override {
        return MLTaskType::SupervisedDimensionalityReduction;
    }

    /**
     * @brief Route train() to fitTransform() discarding the projected output
     *
     * Allows supervised dim reduction ops to be registered in MLModelRegistry
     * and trained via the standard supervised interface. The projected result
     * is discarded; call fitTransform() directly to obtain it.
     */
    bool train(
            arma::mat const & features,
            arma::Row<std::size_t> const & labels,
            MLModelParametersBase const * params) override {
        arma::mat discard;
        return fitTransform(features, labels, params, discard);
    }
};

}// namespace MLCore

#endif// MLCORE_MLSUPERVISEDDIMREDUCTIONOPERATION_HPP
