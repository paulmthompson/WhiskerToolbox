/**
 * @file LogitProjectionOperation.hpp
 * @brief Supervised dimensionality reduction via logit (pre-activation) projection
 *
 * Trains a softmax regression classifier on labeled feature data and extracts
 * the raw pre-softmax logit activations as a C-dimensional discriminative
 * projection. The result is a (C × N) matrix where each column holds the
 * class logit scores for one observation.
 *
 * Since logits represent the linear decision boundary in feature space, this
 * is equivalent to a supervised linear projection — a data-driven counterpart
 * to PCA that maximizes class separation rather than variance.
 *
 * Uses mlpack::SoftmaxRegression<arma::mat> for both binary (C=2) and
 * multi-class (C>2) cases, with pimpl isolation of mlpack headers.
 */

#ifndef MLCORE_LOGITPROJECTIONOPERATION_HPP
#define MLCORE_LOGITPROJECTIONOPERATION_HPP

#include "MLCore/models/MLModelOperation.hpp"
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
 * @brief Supervised linear projection via softmax logit extraction
 *
 * Given labeled feature data (D × N) and integer class labels, trains a
 * softmax regression model and extracts the weight matrix W (C × D) and
 * bias b (C). The logit projection for new data X is:
 *
 *   result = W * X + b_broadcast   (C × N)
 *
 * ## Usage
 * ```cpp
 * LogitProjectionOperation lp;
 * LogitProjectionParameters params;
 * params.lambda = 0.001;
 *
 * arma::mat logits;
 * lp.fitTransform(features, labels, &params, logits);  // (C × N) output
 *
 * arma::mat new_logits;
 * lp.transform(new_features, new_logits);  // project without labels
 *
 * // Column names: ["Logit:0", "Logit:1", ...]
 * auto names = lp.outputColumnNames();
 * ```
 *
 * Feature scaling (z-score normalization) is intentionally NOT performed
 * here — it is the caller's responsibility to pre-scale via FeatureConverter
 * if desired.
 *
 * @note Output convention: (C × N), each column is one observation. This
 * matches the mlpack convention used throughout MLCore.
 */
class LogitProjectionOperation : public MLSupervisedDimReductionOperation {
public:
    LogitProjectionOperation();
    ~LogitProjectionOperation() override;

    // Non-copyable (owns mlpack model)
    LogitProjectionOperation(LogitProjectionOperation const &) = delete;
    LogitProjectionOperation & operator=(LogitProjectionOperation const &) = delete;

    // Movable
    LogitProjectionOperation(LogitProjectionOperation &&) noexcept;
    LogitProjectionOperation & operator=(LogitProjectionOperation &&) noexcept;

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
     * @brief Train softmax regression and project labeled data to logit space
     *
     * @param features  (D × N) feature matrix in mlpack convention
     * @param labels    (N,) label row with class indices in [0, C)
     * @param params    LogitProjectionParameters or nullptr for defaults
     * @param result    (C × N) output logit matrix
     * @return true on success
     *
     * @pre features must not be empty (enforcement: runtime_check)
     * @pre labels.n_elem == features.n_cols (enforcement: runtime_check)
     * @pre labels must contain at least 2 distinct classes (enforcement: runtime_check)
     * @pre params is nullptr or points to a valid MLModelParametersBase (enforcement: runtime_check)
     * @pre all values in features are finite — no NaN or Inf (enforcement: none) [IMPORTANT]
     * @post isTrained() returns true on success
     * @post result.n_rows == numClasses(), result.n_cols == features.n_cols on success
     */
    bool fitTransform(
            arma::mat const & features,
            arma::Row<std::size_t> const & labels,
            MLModelParametersBase const * params,
            arma::mat & result) override;

    /**
     * @brief Project new data using the fitted weight matrix
     *
     * Computes logits = W * features + b_broadcast without rerunning training.
     *
     * @param features  (D × N) feature matrix — D must match training data
     * @param result    (C × N) logit output
     * @return true on success
     *
     * @pre isTrained() must be true (enforcement: runtime_check)
     * @pre features must not be empty (enforcement: runtime_check)
     * @pre features.n_rows == numFeatures() (enforcement: runtime_check)
     * @pre all values in features are finite — no NaN or Inf (enforcement: none) [IMPORTANT]
     * @post result.n_rows == numClasses(), result.n_cols == features.n_cols on success
     */
    bool transform(
            arma::mat const & features,
            arma::mat & result) override;

    // ========================================================================
    // Query / introspection
    // ========================================================================

    [[nodiscard]] std::size_t outputDimensions() const override;
    [[nodiscard]] std::vector<std::string> outputColumnNames() const override;

    // ========================================================================
    // Serialization
    // ========================================================================

    bool save(std::ostream & out) const override;
    bool load(std::istream & in) override;

    // ========================================================================
    // MLModelOperation query methods
    // ========================================================================

    [[nodiscard]] bool isTrained() const override;
    [[nodiscard]] std::size_t numClasses() const override;
    [[nodiscard]] std::size_t numFeatures() const override;

private:
    /// Pimpl to keep mlpack.hpp out of the header
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

}// namespace MLCore

#endif// MLCORE_LOGITPROJECTIONOPERATION_HPP
