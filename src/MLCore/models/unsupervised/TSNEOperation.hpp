/**
 * @file TSNEOperation.hpp
 * @brief MLCore-native t-SNE dimensionality reduction (wraps tapkee)
 */

#ifndef MLCORE_TSNEOPERATION_HPP
#define MLCORE_TSNEOPERATION_HPP

#include "MLCore/models/MLDimReductionOperation.hpp"
#include "MLCore/models/MLModelParameters.hpp"

#include <armadillo>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace MLCore {

/**
 * @brief t-SNE dimensionality reduction via tapkee
 *
 * Performs t-distributed Stochastic Neighbor Embedding, a non-linear
 * technique well suited for embedding high-dimensional data into 2D or
 * 3D spaces for visualization.
 *
 * ## Usage
 * ```cpp
 * TSNEOperation tsne;
 * TSNEParameters params;
 * params.n_components = 2;
 * params.perplexity = 30.0;
 *
 * arma::mat result;
 * tsne.fitTransform(features, &params, result);
 * // result is (2 × observations)
 * ```
 *
 * @note t-SNE is a transductive method — it cannot project unseen data.
 *       Calling transform() will always return false.
 * @note explainedVarianceRatio() returns an empty vector (not applicable).
 */
class TSNEOperation : public MLDimReductionOperation {
public:
    TSNEOperation();
    ~TSNEOperation() override;

    TSNEOperation(TSNEOperation const &) = delete;
    TSNEOperation & operator=(TSNEOperation const &) = delete;

    TSNEOperation(TSNEOperation &&) noexcept;
    TSNEOperation & operator=(TSNEOperation &&) noexcept;

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

    bool fitTransform(
            arma::mat const & features,
            MLModelParametersBase const * params,
            arma::mat & result) override;

    /// @note Always returns false — t-SNE does not support out-of-sample projection.
    bool transform(
            arma::mat const & features,
            arma::mat & result) override;

    [[nodiscard]] std::size_t outputDimensions() const override;

    /// @note Always returns empty — explained variance is not defined for t-SNE.
    [[nodiscard]] std::vector<double> explainedVarianceRatio() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

}// namespace MLCore

#endif// MLCORE_TSNEOPERATION_HPP
