/**
 * @file LARSProjectionOperation.hpp
 * @brief LARS-based supervised feature selection (LASSO / Ridge / Elastic Net)
 *
 * Uses mlpack's LARS (Least Angle Regression Stagewise) algorithm to train
 * penalized regression models against class labels, then identifies which
 * input features have non-zero coefficients (the "active set"). The output
 * is the subset of input features that survived regularization.
 *
 * Binary (C=2): fits a single LARS model (response = class indicator).
 * Multi-class (C>=3): fits one-vs-all LARS models and takes the union of
 * active feature sets across all models.
 *
 * The regularization mode can be set to LASSO (L1 only), Ridge (L2 only),
 * or Elastic Net (L1 + L2).
 *
 * ## Motivation
 * LASSO regression produces sparse coefficient vectors — features whose
 * coefficients are shrunk to zero are not predictive and are removed.
 * This provides an interpretable, supervised dimensionality reduction
 * where the output features are a subset of the originals (not a linear
 * combination), making downstream analysis easier to interpret.
 *
 * ## Output Convention
 * Output is (A × N), where A = number of active (non-zero) features.
 * Each row corresponds to one selected input feature, preserving its
 * original values. Column names indicate which original feature index
 * was selected (e.g. "LARS_sel:3", "LARS_sel:7").
 */

#ifndef MLCORE_LARSPROJECTIONOPERATION_HPP
#define MLCORE_LARSPROJECTIONOPERATION_HPP

#include "MLCore/models/MLModelParameters.hpp"
#include "MLCore/models/MLSupervisedDimReductionOperation.hpp"
#include "MLCore/models/MLTaskType.hpp"

#include <armadillo>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace MLCore {

/**
 * @brief LARS-based supervised feature selection
 *
 * Trains LARS regression models against class labels and selects
 * input features whose coefficients are non-zero. The output is the
 * subset of original features that survived regularization.
 *
 * ## Usage
 * ```cpp
 * LARSProjectionOperation lars_op;
 * LARSProjectionParameters params;
 * params.regularization_type = RegularizationType::LASSO;
 * params.lambda1 = 0.1;
 *
 * arma::mat result;
 * lars_op.fitTransform(features, labels, &params, result);
 * // result is (A × N) where A = number of selected features
 *
 * // Re-select features on new data
 * arma::mat new_result;
 * lars_op.transform(new_features, new_result);
 *
 * // Inspect which feature indices were selected
 * auto active = lars_op.activeFeatureIndices();
 * ```
 *
 * @note Output convention: (A × N), where A ≤ D is the number of features
 * with non-zero regression coefficients. Each row preserves the original
 * feature values (no linear combination).
 */
class LARSProjectionOperation : public MLSupervisedDimReductionOperation {
public:
    LARSProjectionOperation();
    ~LARSProjectionOperation() override;

    // Non-copyable (owns fitted state)
    LARSProjectionOperation(LARSProjectionOperation const &) = delete;
    LARSProjectionOperation & operator=(LARSProjectionOperation const &) = delete;

    // Movable
    LARSProjectionOperation(LARSProjectionOperation &&) noexcept;
    LARSProjectionOperation & operator=(LARSProjectionOperation &&) noexcept;

    // ========================================================================
    // Identity & metadata
    // ========================================================================

    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] MLTaskType getTaskType() const override;
    [[nodiscard]] std::unique_ptr<MLModelParametersBase> getDefaultParameters() const override;

    // ========================================================================
    // Supervised dimensionality reduction
    // ========================================================================

    /**
     * @brief Fit LARS models and select features with non-zero coefficients
     *
     * Binary (C=2): fits a single LARS model with binary response.
     * Multi-class (C>=3): fits one-vs-all LARS models and takes the union
     * of active feature sets.
     *
     * @param features  (D × N) feature matrix in mlpack convention
     * @param labels    (N,) label row with class indices in [0, C)
     * @param params    LARSProjectionParameters or nullptr for defaults
     * @param result    (A × N) output — input rows for selected features only
     * @return true on success, false on validation failure
     *
     * @pre features must not be empty (enforcement: runtime_check)
     * @pre features must have at least 2 columns (enforcement: runtime_check)
     * @pre features must not contain NaN or Inf (enforcement: none) [IMPORTANT]
     * @pre labels must not be empty (enforcement: runtime_check)
     * @pre labels.n_elem must equal features.n_cols (enforcement: runtime_check)
     * @pre labels must contain at least 2 distinct class values (enforcement: runtime_check)
     * @pre params, if non-null, must be dynamic_castable to LARSProjectionParameters
     *      (enforcement: runtime_check — falls back to defaults if cast fails)
     *
     * @post On success: isTrained() == true, result.n_rows == outputDimensions()
     *       (the number of selected features), result.n_cols == features.n_cols
     * @post On failure: isTrained() unchanged, result is unmodified
     */
    bool fitTransform(
            arma::mat const & features,
            arma::Row<std::size_t> const & labels,
            MLModelParametersBase const * params,
            arma::mat & result) override;

    /**
     * @brief Select the active features from new data
     *
     * Extracts the rows corresponding to features that had non-zero
     * coefficients during training.
     *
     * @param features  (D × N) feature matrix — D must match training data
     * @param result    (A × N) output — selected feature rows
     * @return true on success, false if not trained or dimension mismatch
     *
     * @pre isTrained() must be true (enforcement: runtime_check)
     * @pre features must not be empty (enforcement: runtime_check)
     * @pre features.n_rows must equal numFeatures() (enforcement: runtime_check)
     * @pre features must not contain NaN or Inf (enforcement: none) [IMPORTANT]
     *
     * @post result has outputDimensions() rows and features.n_cols columns
     */
    bool transform(
            arma::mat const & features,
            arma::mat & result) override;

    // ========================================================================
    // Query / introspection
    // ========================================================================

    [[nodiscard]] std::size_t outputDimensions() const override;
    [[nodiscard]] std::vector<std::string> outputColumnNames() const override;
    [[nodiscard]] bool isTrained() const override;
    [[nodiscard]] std::size_t numClasses() const override;
    [[nodiscard]] std::size_t numFeatures() const override;

    /**
     * @brief Union of non-zero coefficient indices across all OvA models
     *
     * After fitting with LASSO or Elastic Net regularization, some features
     * will have zero coefficients. This method returns the sorted set of
     * feature indices that have a non-zero coefficient in at least one
     * of the per-class models.
     *
     * @return Sorted vector of active feature indices, or empty if not trained
     */
    [[nodiscard]] std::vector<std::size_t> activeFeatureIndices() const;

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

#endif// MLCORE_LARSPROJECTIONOPERATION_HPP
