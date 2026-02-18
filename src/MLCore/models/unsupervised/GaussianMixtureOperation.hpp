#ifndef MLCORE_GAUSSIANMIXTUREOPERATION_HPP
#define MLCORE_GAUSSIANMIXTUREOPERATION_HPP

/**
 * @file GaussianMixtureOperation.hpp
 * @brief MLCore-native Gaussian Mixture Model clustering (wraps mlpack::GMM)
 *
 * This is a fresh implementation conforming to the MLCore::MLModelOperation
 * interface. It does NOT depend on or modify any legacy code in
 * src/WhiskerToolbox/ML_Widget/.
 *
 * Features:
 * - Conforms to the evolved MLModelOperation interface (unsupervised path)
 * - Overrides fit() and assignClusters()
 * - Provides soft (probabilistic) cluster assignments via clusterProbabilities()
 * - Stores fitted GMM parameters (means, covariances, weights) for assignment
 *   on new data
 * - Supports save/load for model persistence via binary serialization
 * - Tracks metadata (numComponents, numFeatures) for validation
 * - Uses pimpl to isolate mlpack headers from consumers
 * - Uses MLCore::GMMParameters (k, max_iterations, seed)
 *
 * @see ml_library_roadmap.md §2.7
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
 * @brief Gaussian Mixture Model clustering wrapping mlpack::GMM
 *
 * Fits a mixture of K Gaussian distributions to the data using the
 * Expectation-Maximization (EM) algorithm. Each observation is assigned to
 * the component with the highest posterior probability.
 *
 * Unlike K-Means, GMM provides soft (probabilistic) cluster assignments
 * and can model clusters with different shapes and orientations via
 * full covariance matrices.
 *
 * ## Usage
 * ```cpp
 * GaussianMixtureOperation gmm;
 * GMMParameters params;
 * params.k = 3;
 * params.max_iterations = 300;
 *
 * gmm.fit(features, &params);
 * gmm.assignClusters(new_features, assignments);
 *
 * // Get soft assignments (probabilities per component)
 * arma::mat probs;
 * gmm.clusterProbabilities(new_features, probs);
 * ```
 */
class GaussianMixtureOperation : public MLModelOperation {
public:
    GaussianMixtureOperation();
    ~GaussianMixtureOperation() override;

    // Non-copyable (owns mlpack model state)
    GaussianMixtureOperation(GaussianMixtureOperation const &) = delete;
    GaussianMixtureOperation & operator=(GaussianMixtureOperation const &) = delete;

    // Movable
    GaussianMixtureOperation(GaussianMixtureOperation &&) noexcept;
    GaussianMixtureOperation & operator=(GaussianMixtureOperation &&) noexcept;

    // ========================================================================
    // Identity & metadata
    // ========================================================================

    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] MLTaskType getTaskType() const override;
    [[nodiscard]] std::unique_ptr<MLModelParametersBase> getDefaultParameters() const override;

    // ========================================================================
    // Unsupervised fitting & cluster assignment
    // ========================================================================

    bool fit(
        arma::mat const & features,
        MLModelParametersBase const * params) override;

    bool assignClusters(
        arma::mat const & features,
        arma::Row<std::size_t> & assignments) override;

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

    // ========================================================================
    // GMM-specific accessors
    // ========================================================================

    /**
     * @brief Get the fitted component means
     *
     * @return Vector of mean vectors (one per component), or empty if not fitted.
     */
    [[nodiscard]] std::vector<arma::vec> const & means() const;

    /**
     * @brief Get the fitted component covariance matrices
     *
     * @return Vector of covariance matrices (one per component), or empty if not fitted.
     */
    [[nodiscard]] std::vector<arma::mat> const & covariances() const;

    /**
     * @brief Get the fitted component weights (mixing coefficients)
     *
     * @return Weight vector (sums to 1.0), or empty if not fitted.
     */
    [[nodiscard]] arma::vec const & weights() const;

    /**
     * @brief Compute per-component posterior probabilities for each observation
     *
     * For each observation (column), returns the posterior probability of
     * belonging to each Gaussian component. Each column of the output sums to 1.
     *
     * @param features  Feature matrix (features × observations)
     * @param probs     Output matrix (k × observations) of posterior probabilities
     * @return true on success
     */
    [[nodiscard]] bool clusterProbabilities(
        arma::mat const & features,
        arma::mat & probs) const;

private:
    /// Pimpl to keep mlpack.hpp out of the header
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace MLCore

#endif // MLCORE_GAUSSIANMIXTUREOPERATION_HPP
