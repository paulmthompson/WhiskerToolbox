#ifndef MLCORE_LOGISTICREGRESSIONOPERATION_HPP
#define MLCORE_LOGISTICREGRESSIONOPERATION_HPP

/**
 * @file LogisticRegressionOperation.hpp
 * @brief MLCore-native Logistic Regression classifier (wraps mlpack::LogisticRegression)
 *
 * This is a fresh implementation conforming to the MLCore::MLModelOperation
 * interface. It does NOT depend on or modify any legacy code in
 * src/WhiskerToolbox/ML_Widget/.
 *
 * Features:
 * - Conforms to the evolved MLModelOperation interface (MLTaskType, numFeatures, etc.)
 * - Naturally provides probability estimates via LogisticRegression::Classify()
 * - Supports save/load for model persistence via binary serialization
 * - Tracks training metadata (numClasses, numFeatures) for validation
 * - Uses pimpl to isolate mlpack headers from consumers
 * - Uses the MLCore::LogisticRegressionParameters (lambda, max_iterations)
 *
 * Note: mlpack's LogisticRegression is inherently a binary classifier (2 classes).
 * For multi-class logistic regression, mlpack provides SoftmaxRegression, which
 * could be added as a separate operation in the future.
 *
 * @see ml_library_roadmap.md §2.4
 */

#include "MLCore/models/MLModelOperation.hpp"
#include "MLCore/models/MLModelParameters.hpp"
#include "MLCore/models/MLTaskType.hpp"

#include <armadillo>

#include <cstddef>
#include <memory>
#include <string>

namespace MLCore {

/**
 * @brief Logistic Regression classifier wrapping mlpack::LogisticRegression
 *
 * Binary supervised classification using L2-regularized logistic regression.
 * Naturally outputs calibrated probability estimates (sigmoid of linear model).
 *
 * ## Usage
 * ```cpp
 * LogisticRegressionOperation lr;
 * LogisticRegressionParameters params;
 * params.lambda = 0.01;
 * params.max_iterations = 10000;
 *
 * lr.train(features, labels, &params);
 * lr.predict(new_features, predictions);
 *
 * // Get well-calibrated per-class probabilities
 * arma::mat probs;
 * lr.predictProbabilities(new_features, probs);
 * ```
 */
class LogisticRegressionOperation : public MLModelOperation {
public:
    LogisticRegressionOperation();
    ~LogisticRegressionOperation() override;

    // Non-copyable (owns mlpack model)
    LogisticRegressionOperation(LogisticRegressionOperation const &) = delete;
    LogisticRegressionOperation & operator=(LogisticRegressionOperation const &) = delete;

    // Movable
    LogisticRegressionOperation(LogisticRegressionOperation &&) noexcept;
    LogisticRegressionOperation & operator=(LogisticRegressionOperation &&) noexcept;

    // ========================================================================
    // Identity & metadata
    // ========================================================================

    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] MLTaskType getTaskType() const override;
    [[nodiscard]] std::unique_ptr<MLModelParametersBase> getDefaultParameters() const override;

    // ========================================================================
    // Supervised training & prediction
    // ========================================================================

    bool train(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        MLModelParametersBase const * params) override;

    bool predict(
        arma::mat const & features,
        arma::Row<std::size_t> & predictions) override;

    bool predictProbabilities(
        arma::mat const & features,
        arma::mat & probabilities) override;

    // ========================================================================
    // Serialization
    // ========================================================================

    bool save(std::ostream & out) const override;
    bool load(std::istream & in) override;

    // ========================================================================
    // Query / introspection
    // ========================================================================

    [[nodiscard]] bool isTrained() const override;
    [[nodiscard]] std::size_t numClasses() const override;
    [[nodiscard]] std::size_t numFeatures() const override;

private:
    /// Pimpl to keep mlpack.hpp out of the header
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace MLCore

#endif // MLCORE_LOGISTICREGRESSIONOPERATION_HPP
