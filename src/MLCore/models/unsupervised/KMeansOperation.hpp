#ifndef MLCORE_KMEANSOPERATION_HPP
#define MLCORE_KMEANSOPERATION_HPP

/**
 * @file KMeansOperation.hpp
 * @brief MLCore-native K-Means clustering (wraps mlpack::KMeans)
 *
 * This is a fresh implementation conforming to the MLCore::MLModelOperation
 * interface. It does NOT depend on or modify any legacy code in
 * src/WhiskerToolbox/ML_Widget/.
 *
 * Features:
 * - Conforms to the evolved MLModelOperation interface (unsupervised path)
 * - Overrides fit() and assignClusters() (not train/predict)
 * - Stores fitted centroids for assignment on new data
 * - Supports save/load for model persistence via binary serialization
 * - Tracks metadata (numClusters, numFeatures) for validation
 * - Uses pimpl to isolate mlpack headers from consumers
 * - Uses the MLCore::KMeansParameters (k, max_iterations, seed)
 *
 * @see ml_library_roadmap.md §2.5
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
 * @brief K-Means clustering wrapping mlpack::KMeans
 *
 * Unsupervised clustering that partitions data into K clusters by minimizing
 * within-cluster sum of squared distances. After fitting, the centroids can
 * be used to assign new observations to the nearest cluster.
 *
 * ## Usage
 * ```cpp
 * KMeansOperation km;
 * KMeansParameters params;
 * params.k = 3;
 * params.max_iterations = 1000;
 *
 * km.fit(features, &params);
 * km.assignClusters(new_features, assignments);
 * ```
 */
class KMeansOperation : public MLModelOperation {
public:
    KMeansOperation();
    ~KMeansOperation() override;

    // Non-copyable (owns mlpack model state)
    KMeansOperation(KMeansOperation const &) = delete;
    KMeansOperation & operator=(KMeansOperation const &) = delete;

    // Movable
    KMeansOperation(KMeansOperation &&) noexcept;
    KMeansOperation & operator=(KMeansOperation &&) noexcept;

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
    // K-Means specific accessors
    // ========================================================================

    /**
     * @brief Get the fitted cluster centroids
     *
     * @return Centroid matrix (features × k), or empty if not fitted.
     *         Each column is one centroid.
     */
    [[nodiscard]] arma::mat const & centroids() const;

private:
    /// Pimpl to keep mlpack.hpp out of the header
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace MLCore

#endif // MLCORE_KMEANSOPERATION_HPP
