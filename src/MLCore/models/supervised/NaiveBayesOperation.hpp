#ifndef MLCORE_NAIVEBAYESOPERATION_HPP
#define MLCORE_NAIVEBAYESOPERATION_HPP

/**
 * @file NaiveBayesOperation.hpp
 * @brief MLCore-native Naive Bayes classifier (wraps mlpack::NaiveBayesClassifier)
 *
 * This is a fresh implementation conforming to the MLCore::MLModelOperation
 * interface. It does NOT depend on or modify the legacy NaiveBayesModelOperation
 * in src/WhiskerToolbox/ML_Widget/ML_Naive_Bayes_Widget/.
 *
 * Features beyond the legacy implementation:
 * - Conforms to the evolved MLModelOperation interface (MLTaskType, numFeatures, etc.)
 * - Supports save/load for model persistence via binary serialization
 * - Tracks training metadata (numClasses, numFeatures) for validation
 * - Uses pimpl to isolate mlpack headers from consumers
 * - Uses the MLCore::NaiveBayesParameters (epsilon smoothing parameter)
 *
 * @see ml_library_roadmap.md §2.3
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
 * @brief Naive Bayes classifier wrapping mlpack::NaiveBayesClassifier
 *
 * Supervised binary/multi-class classification using Gaussian Naive Bayes.
 * Assumes features are conditionally independent given the class label,
 * with each feature modeled as a Gaussian distribution per class.
 *
 * ## Usage
 * ```cpp
 * NaiveBayesOperation nb;
 * NaiveBayesParameters params;
 * params.epsilon = 1e-9;
 *
 * nb.train(features, labels, &params);
 * nb.predict(new_features, predictions);
 *
 * // Optional: get per-class probabilities
 * arma::mat probs;
 * nb.predictProbabilities(new_features, probs);
 * ```
 */
class NaiveBayesOperation : public MLModelOperation {
public:
    NaiveBayesOperation();
    ~NaiveBayesOperation() override;

    // Non-copyable (owns mlpack model)
    NaiveBayesOperation(NaiveBayesOperation const &) = delete;
    NaiveBayesOperation & operator=(NaiveBayesOperation const &) = delete;

    // Movable
    NaiveBayesOperation(NaiveBayesOperation &&) noexcept;
    NaiveBayesOperation & operator=(NaiveBayesOperation &&) noexcept;

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

#endif // MLCORE_NAIVEBAYESOPERATION_HPP
