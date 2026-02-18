/**
 * @file DBSCANOperation.cpp
 * @brief Implementation of DBSCAN density-based clustering via mlpack
 */

#include "DBSCANOperation.hpp"

#include "mlpack.hpp"

#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace MLCore {

// ============================================================================
// Pimpl — hides mlpack types from header
// ============================================================================

struct DBSCANOperation::Impl {
    /// Fitted centroids (features × num_clusters). Each column is one centroid.
    arma::mat centroids;
    std::size_t num_clusters = 0;
    std::size_t num_features = 0;
    std::size_t noise_count = 0;
    double epsilon = 1.0;
    bool trained = false;
};

// ============================================================================
// Construction / destruction / move
// ============================================================================

DBSCANOperation::DBSCANOperation()
    : _impl(std::make_unique<Impl>())
{
}

DBSCANOperation::~DBSCANOperation() = default;

DBSCANOperation::DBSCANOperation(DBSCANOperation &&) noexcept = default;
DBSCANOperation & DBSCANOperation::operator=(DBSCANOperation &&) noexcept = default;

// ============================================================================
// Identity & metadata
// ============================================================================

std::string DBSCANOperation::getName() const
{
    return "DBSCAN";
}

MLTaskType DBSCANOperation::getTaskType() const
{
    return MLTaskType::Clustering;
}

std::unique_ptr<MLModelParametersBase> DBSCANOperation::getDefaultParameters() const
{
    return std::make_unique<DBSCANParameters>();
}

// ============================================================================
// Fitting
// ============================================================================

bool DBSCANOperation::fit(
    arma::mat const & features,
    MLModelParametersBase const * params)
{
    // Validate inputs
    if (features.empty()) {
        std::cerr << "DBSCANOperation::fit: Feature matrix is empty.\n";
        return false;
    }

    // Extract parameters (use defaults if nullptr or wrong type)
    auto const * db_params = dynamic_cast<DBSCANParameters const *>(params);
    DBSCANParameters defaults;
    if (!db_params) {
        db_params = &defaults;
    }

    double const eps = db_params->epsilon;
    std::size_t const min_pts = db_params->min_points;

    // Validate epsilon
    if (eps <= 0.0) {
        std::cerr << "DBSCANOperation::fit: epsilon must be > 0, got " << eps << ".\n";
        return false;
    }

    // Validate min_points
    if (min_pts == 0) {
        std::cerr << "DBSCANOperation::fit: min_points must be > 0.\n";
        return false;
    }

    try {
        // Create DBSCAN object
        mlpack::DBSCAN<> dbscan(eps, min_pts);

        // Run clustering — returns number of clusters; assignments contains
        // cluster IDs, with SIZE_MAX for noise points
        arma::Row<std::size_t> assignments;
        arma::mat centroids;
        std::size_t num_clusters = dbscan.Cluster(features, assignments, centroids);

        // Count noise points
        std::size_t noise = 0;
        for (std::size_t i = 0; i < assignments.n_elem; ++i) {
            if (assignments(i) == SIZE_MAX) {
                ++noise;
            }
        }

        _impl->centroids = std::move(centroids);
        _impl->num_clusters = num_clusters;
        _impl->num_features = features.n_rows;
        _impl->noise_count = noise;
        _impl->epsilon = eps;
        _impl->trained = true;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "DBSCANOperation::fit failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// Cluster assignment
// ============================================================================

bool DBSCANOperation::assignClusters(
    arma::mat const & features,
    arma::Row<std::size_t> & assignments)
{
    if (!_impl->trained) {
        std::cerr << "DBSCANOperation::assignClusters: Model not fitted.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "DBSCANOperation::assignClusters: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->num_features) {
        std::cerr << "DBSCANOperation::assignClusters: Feature dimension mismatch — "
                  << "expected " << _impl->num_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    // If no clusters were found (all noise), mark everything as noise
    if (_impl->num_clusters == 0 || _impl->centroids.empty()) {
        assignments.set_size(features.n_cols);
        assignments.fill(SIZE_MAX);
        return true;
    }

    try {
        // Assign each observation to its nearest centroid, but only if within epsilon
        assignments.set_size(features.n_cols);

        for (std::size_t i = 0; i < features.n_cols; ++i) {
            arma::vec const obs = features.col(i);
            double min_dist = std::numeric_limits<double>::max();
            std::size_t best_cluster = SIZE_MAX;

            for (std::size_t c = 0; c < _impl->centroids.n_cols; ++c) {
                double const dist = arma::norm(obs - _impl->centroids.col(c), 2);
                if (dist < min_dist) {
                    min_dist = dist;
                    best_cluster = c;
                }
            }

            // Only assign if within epsilon of the nearest centroid
            if (min_dist <= _impl->epsilon) {
                assignments(i) = best_cluster;
            } else {
                assignments(i) = SIZE_MAX;
            }
        }

        return true;

    } catch (std::exception const & e) {
        std::cerr << "DBSCANOperation::assignClusters failed: " << e.what() << "\n";
        return false;
    }
}

// ============================================================================
// Serialization
// ============================================================================

bool DBSCANOperation::save(std::ostream & out) const
{
    if (!_impl->trained) {
        return false;
    }

    try {
        cereal::BinaryOutputArchive ar(out);
        ar(cereal::make_nvp("num_clusters", _impl->num_clusters));
        ar(cereal::make_nvp("num_features", _impl->num_features));
        ar(cereal::make_nvp("noise_count", _impl->noise_count));
        ar(cereal::make_nvp("epsilon", _impl->epsilon));

        // Serialize centroids as a flat vector + dimensions
        std::size_t n_rows = _impl->centroids.n_rows;
        std::size_t n_cols = _impl->centroids.n_cols;
        ar(cereal::make_nvp("centroids_rows", n_rows));
        ar(cereal::make_nvp("centroids_cols", n_cols));

        // Armadillo stores column-major, so we can serialize the raw data
        std::vector<double> data(_impl->centroids.memptr(),
                                 _impl->centroids.memptr() + n_rows * n_cols);
        ar(cereal::make_nvp("centroids_data", data));

        return out.good();
    } catch (std::exception const & e) {
        std::cerr << "DBSCANOperation::save failed: " << e.what() << "\n";
        return false;
    }
}

bool DBSCANOperation::load(std::istream & in)
{
    try {
        cereal::BinaryInputArchive ar(in);

        std::size_t num_clusters = 0;
        std::size_t num_features = 0;
        std::size_t noise_count = 0;
        double epsilon = 1.0;
        ar(cereal::make_nvp("num_clusters", num_clusters));
        ar(cereal::make_nvp("num_features", num_features));
        ar(cereal::make_nvp("noise_count", noise_count));
        ar(cereal::make_nvp("epsilon", epsilon));

        std::size_t n_rows = 0;
        std::size_t n_cols = 0;
        ar(cereal::make_nvp("centroids_rows", n_rows));
        ar(cereal::make_nvp("centroids_cols", n_cols));

        std::vector<double> data;
        ar(cereal::make_nvp("centroids_data", data));

        if (data.size() != n_rows * n_cols) {
            std::cerr << "DBSCANOperation::load: centroid data size mismatch.\n";
            _impl->trained = false;
            return false;
        }

        if (n_rows * n_cols > 0) {
            _impl->centroids = arma::mat(data.data(), n_rows, n_cols);
        } else {
            _impl->centroids = arma::mat();
        }
        _impl->num_clusters = num_clusters;
        _impl->num_features = num_features;
        _impl->noise_count = noise_count;
        _impl->epsilon = epsilon;
        _impl->trained = true;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "DBSCANOperation::load failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// Query / introspection
// ============================================================================

bool DBSCANOperation::isTrained() const
{
    return _impl->trained;
}

std::size_t DBSCANOperation::numClasses() const
{
    return _impl->trained ? _impl->num_clusters : 0;
}

std::size_t DBSCANOperation::numFeatures() const
{
    return _impl->trained ? _impl->num_features : 0;
}

// ============================================================================
// DBSCAN-specific accessors
// ============================================================================

arma::mat const & DBSCANOperation::centroids() const
{
    static arma::mat const empty;
    return _impl->trained ? _impl->centroids : empty;
}

std::size_t DBSCANOperation::numNoisePoints() const
{
    return _impl->trained ? _impl->noise_count : 0;
}

double DBSCANOperation::fittedEpsilon() const
{
    return _impl->trained ? _impl->epsilon : 0.0;
}

} // namespace MLCore
