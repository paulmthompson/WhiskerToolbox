/**
 * @file RobustPCAOperation.hpp
 * @brief MLCore Robust PCA dimensionality reduction via ROSL
 */

#ifndef MLCORE_ROBUSTPCAOPERATION_HPP
#define MLCORE_ROBUSTPCAOPERATION_HPP

#include "MLCore/models/MLDimReductionOperation.hpp"
#include "MLCore/models/MLModelParameters.hpp"

#include <armadillo>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace MLCore {

/**
 * @brief Robust PCA dimensionality reduction via ROSL
 *
 * Decomposes a matrix X into a low-rank component A and sparse errors E:
 *   X = A + E,  where A = D * B
 *
 * The dictionary D (features × rank) and coefficients B (rank × observations)
 * are the key outputs. For dimensionality reduction, SVD is applied to the
 * low-rank component A to extract the top n_components principal directions,
 * which are robust to sparse outliers.
 *
 * ## Usage
 * ```cpp
 * RobustPCAOperation rpca;
 * RobustPCAParameters params;
 * params.n_components = 2;
 * params.lambda = 0.0;  // auto-compute
 *
 * arma::mat result;
 * rpca.fitTransform(features, &params, result);
 * // result is (2 × observations)
 * ```
 *
 * @note Supports transform() for projecting new data using the learned basis.
 */
class RobustPCAOperation : public MLDimReductionOperation {
public:
    RobustPCAOperation();
    ~RobustPCAOperation() override;

    RobustPCAOperation(RobustPCAOperation const &) = delete;
    RobustPCAOperation & operator=(RobustPCAOperation const &) = delete;

    RobustPCAOperation(RobustPCAOperation &&) noexcept;
    RobustPCAOperation & operator=(RobustPCAOperation &&) noexcept;

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

#endif// MLCORE_ROBUSTPCAOPERATION_HPP
