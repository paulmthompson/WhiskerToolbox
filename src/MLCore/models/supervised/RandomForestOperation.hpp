#ifndef MLCORE_RANDOMFORESTOPERATION_HPP
#define MLCORE_RANDOMFORESTOPERATION_HPP

/**
 * @file RandomForestOperation.hpp
 * @brief MLCore-native Random Forest classifier (wraps mlpack::RandomForest)
 *
 * Features:
 * - Conforms to the evolved MLModelOperation interface (MLTaskType, numFeatures, etc.)
 * - Supports predictProbabilities() via mlpack's Classify() with probabilities output
 * - Supports save/load for model persistence via binary serialization
 * - Tracks training metadata (numClasses, numFeatures) for validation
 * - Uses the MLCore::RandomForestParameters
 *
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
 * @brief Random Forest classifier wrapping mlpack::RandomForest
 *
 * Supervised binary/multi-class classification using an ensemble of decision
 * trees with Gini impurity and random dimension selection.
 *
 * ## Usage
 * ```cpp
 * RandomForestOperation rf;
 * RandomForestParameters params;
 * params.num_trees = 100;
 * params.maximum_depth = 10;
 *
 * rf.train(features, labels, &params);
 * rf.predict(new_features, predictions);
 *
 * // Optional: get per-class probabilities
 * arma::mat probs;
 * rf.predictProbabilities(new_features, probs);
 * ```
 */
class RandomForestOperation : public MLModelOperation {
public:
    RandomForestOperation();
    ~RandomForestOperation() override;

    // Non-copyable (owns mlpack model)
    RandomForestOperation(RandomForestOperation const &) = delete;
    RandomForestOperation & operator=(RandomForestOperation const &) = delete;

    // Movable
    RandomForestOperation(RandomForestOperation &&) noexcept;
    RandomForestOperation & operator=(RandomForestOperation &&) noexcept;

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

#endif// MLCORE_RANDOMFORESTOPERATION_HPP
