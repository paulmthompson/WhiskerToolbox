#ifndef MLCORE_DBSCANOPERATION_HPP
#define MLCORE_DBSCANOPERATION_HPP

/**
 * @file DBSCANOperation.hpp
 * @brief MLCore-native DBSCAN clustering (wraps mlpack::DBSCAN)
 *
 * Features:
 * - Conforms to the evolved MLModelOperation interface (unsupervised path)
 * - Overrides fit() and assignClusters()
 * - DBSCAN does not produce a reusable model — fit() clusters the data and
 *   stores centroids + epsilon for later nearest-centroid assignment
 * - Points identified as noise (no cluster) get assignment SIZE_MAX
 * - assignClusters() on new data uses nearest-centroid with epsilon threshold:
 *   points farther than epsilon from all centroids are labeled noise
 * - Supports save/load for model persistence via binary serialization
 * - Tracks metadata (numClusters, numFeatures) for validation
 * - Uses pimpl to isolate mlpack headers from consumers
 * - Uses MLCore::DBSCANParameters (epsilon, min_points)
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
 * @brief DBSCAN clustering wrapping mlpack::DBSCAN
 *
 * Density-Based Spatial Clustering of Applications with Noise. Discovers
 * clusters of arbitrary shape based on local density. Points in low-density
 * regions are labeled as noise (SIZE_MAX).
 *
 * Unlike K-Means, DBSCAN does not require specifying the number of clusters
 * in advance — the algorithm discovers them from the data. However, it also
 * does not produce a fitted model that can directly predict on new data.
 *
 * After fitting, this implementation stores the cluster centroids and the
 * epsilon radius. New data can be assigned to clusters via nearest-centroid
 * lookup, with points farther than epsilon from all centroids marked as noise.
 *
 * ## Usage
 * ```cpp
 * DBSCANOperation dbscan;
 * DBSCANParameters params;
 * params.epsilon = 1.0;
 * params.min_points = 5;
 *
 * dbscan.fit(features, &params);
 * dbscan.assignClusters(new_features, assignments);
 * // assignments may contain SIZE_MAX for noise points
 * ```
 */
class DBSCANOperation : public MLModelOperation {
public:
    DBSCANOperation();
    ~DBSCANOperation() override;

    // Non-copyable (owns model state)
    DBSCANOperation(DBSCANOperation const &) = delete;
    DBSCANOperation & operator=(DBSCANOperation const &) = delete;

    // Movable
    DBSCANOperation(DBSCANOperation &&) noexcept;
    DBSCANOperation & operator=(DBSCANOperation &&) noexcept;

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
    // DBSCAN-specific accessors
    // ========================================================================

    /**
     * @brief Get the fitted cluster centroids
     *
     * @return Centroid matrix (features × num_clusters), or empty if not fitted.
     *         Each column is one centroid. Does not include noise.
     */
    [[nodiscard]] arma::mat const & centroids() const;

    /**
     * @brief Get the number of noise points from the last fit
     *
     * @return Number of points labeled as noise (SIZE_MAX), or 0 if not fitted.
     */
    [[nodiscard]] std::size_t numNoisePoints() const;

    /**
     * @brief Get the epsilon used during fitting
     *
     * Used as the distance threshold for assigning new points to clusters.
     */
    [[nodiscard]] double fittedEpsilon() const;

private:
    /// Pimpl to keep mlpack.hpp out of the header
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

}// namespace MLCore

#endif// MLCORE_DBSCANOPERATION_HPP
