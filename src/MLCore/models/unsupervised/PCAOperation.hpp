/**
 * @file PCAOperation.hpp
 * @brief MLCore-native PCA dimensionality reduction (wraps mlpack::PCA)
 */

#ifndef MLCORE_PCAOPERATION_HPP
#define MLCORE_PCAOPERATION_HPP

#include "MLCore/models/MLDimReductionOperation.hpp"
#include "MLCore/models/MLModelParameters.hpp"

#include <armadillo>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace MLCore {

/**
 * @brief PCA dimensionality reduction via mlpack
 *
 * Performs Principal Component Analysis, optionally scaling features to
 * unit variance before computing eigenvectors. Stores the fitted model
 * (eigenvectors, mean, scaling) for re-projection of new data.
 *
 * ## Usage
 * ```cpp
 * PCAOperation pca;
 * PCAParameters params;
 * params.n_components = 3;
 * params.scale = true;
 *
 * arma::mat result;
 * pca.fitTransform(features, &params, result);
 * // result is (3 × observations)
 *
 * auto variance = pca.explainedVarianceRatio();
 * // variance[0] = fraction of variance from PC1, etc.
 * ```
 */
class PCAOperation : public MLDimReductionOperation {
public:
    PCAOperation();
    ~PCAOperation() override;

    PCAOperation(PCAOperation const &) = delete;
    PCAOperation & operator=(PCAOperation const &) = delete;

    PCAOperation(PCAOperation &&) noexcept;
    PCAOperation & operator=(PCAOperation &&) noexcept;

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

    bool transform(
            arma::mat const & features,
            arma::mat & result) override;

    [[nodiscard]] std::size_t outputDimensions() const override;
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

#endif// MLCORE_PCAOPERATION_HPP
