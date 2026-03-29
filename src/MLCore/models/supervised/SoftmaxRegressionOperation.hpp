#ifndef MLCORE_SOFTMAXREGRESSIONOPERATION_HPP
#define MLCORE_SOFTMAXREGRESSIONOPERATION_HPP

/**
 * @file SoftmaxRegressionOperation.hpp
 * @brief MLCore-native Softmax Regression classifier (wraps mlpack::SoftmaxRegression)
 *
 * Features:
 * - Multi-class logistic regression (C ≥ 2 classes)
 * - Conforms to the MLModelOperation interface (MLTaskType, numFeatures, etc.)
 * - Produces calibrated per-class probability estimates via softmax
 * - Supports save/load for model persistence via binary serialization
 * - Tracks training metadata (numClasses, numFeatures) for validation
 * - Uses pimpl to isolate mlpack headers from consumers
 * - Uses MLCore::SoftmaxRegressionParameters (lambda, max_iterations)
 *
 * Unlike LogisticRegressionOperation (binary-only), SoftmaxRegression handles
 * any number of classes ≥ 2.
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
 * @brief Multi-class Softmax Regression classifier wrapping mlpack::SoftmaxRegression
 *
 * Supervised multi-class classification using L2-regularized softmax regression.
 * Outputs calibrated per-class probability estimates.
 *
 * ## Usage
 * ```cpp
 * SoftmaxRegressionOperation sr;
 * SoftmaxRegressionParameters params;
 * params.lambda = 0.001;
 * params.max_iterations = 10000;
 *
 * sr.train(features, labels, &params);
 * sr.predict(new_features, predictions);
 *
 * // Get per-class probabilities
 * arma::mat probs;
 * sr.predictProbabilities(new_features, probs);
 * ```
 */
class SoftmaxRegressionOperation : public MLModelOperation {
public:
    SoftmaxRegressionOperation();
    ~SoftmaxRegressionOperation() override;

    // Non-copyable (owns mlpack model)
    SoftmaxRegressionOperation(SoftmaxRegressionOperation const &) = delete;
    SoftmaxRegressionOperation & operator=(SoftmaxRegressionOperation const &) = delete;

    // Movable
    SoftmaxRegressionOperation(SoftmaxRegressionOperation &&) noexcept;
    SoftmaxRegressionOperation & operator=(SoftmaxRegressionOperation &&) noexcept;

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

}// namespace MLCore

#endif// MLCORE_SOFTMAXREGRESSIONOPERATION_HPP
