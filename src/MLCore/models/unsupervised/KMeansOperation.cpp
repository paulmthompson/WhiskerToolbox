#include "KMeansOperation.hpp"

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

struct KMeansOperation::Impl {
    /// Fitted centroids (features × k). Each column is one cluster centroid.
    arma::mat centroids;
    std::size_t num_clusters = 0;
    std::size_t num_features = 0;
    bool trained = false;
};

// ============================================================================
// Construction / destruction / move
// ============================================================================

KMeansOperation::KMeansOperation()
    : _impl(std::make_unique<Impl>())
{
}

KMeansOperation::~KMeansOperation() = default;

KMeansOperation::KMeansOperation(KMeansOperation &&) noexcept = default;
KMeansOperation & KMeansOperation::operator=(KMeansOperation &&) noexcept = default;

// ============================================================================
// Identity & metadata
// ============================================================================

std::string KMeansOperation::getName() const
{
    return "K-Means";
}

MLTaskType KMeansOperation::getTaskType() const
{
    return MLTaskType::Clustering;
}

std::unique_ptr<MLModelParametersBase> KMeansOperation::getDefaultParameters() const
{
    return std::make_unique<KMeansParameters>();
}

// ============================================================================
// Fitting
// ============================================================================

bool KMeansOperation::fit(
    arma::mat const & features,
    MLModelParametersBase const * params)
{
    // Validate inputs
    if (features.empty()) {
        std::cerr << "KMeansOperation::fit: Feature matrix is empty.\n";
        return false;
    }

    // Extract parameters (use defaults if nullptr or wrong type)
    auto const * km_params = dynamic_cast<KMeansParameters const *>(params);
    KMeansParameters defaults;
    if (!km_params) {
        km_params = &defaults;
    }

    std::size_t const k = km_params->k;
    std::size_t const max_iterations = km_params->max_iterations;

    // Validate k
    if (k == 0) {
        std::cerr << "KMeansOperation::fit: k must be > 0.\n";
        return false;
    }
    if (k > features.n_cols) {
        std::cerr << "KMeansOperation::fit: k (" << k
                  << ") exceeds number of observations ("
                  << features.n_cols << ").\n";
        return false;
    }

    try {
        // Set random seed if provided (mlpack uses its own RNG, not armadillo's)
        if (km_params->seed.has_value()) {
            mlpack::RandomSeed(static_cast<size_t>(km_params->seed.value()));
        }

        // Create KMeans object with max iterations
        mlpack::KMeans<> kmeans(max_iterations);

        // Run clustering — output is centroids and assignments
        arma::Row<std::size_t> assignments;
        _impl->centroids = arma::mat(); // clear previous

        kmeans.Cluster(features, k, assignments, _impl->centroids);

        _impl->num_clusters = k;
        _impl->num_features = features.n_rows;
        _impl->trained = true;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "KMeansOperation::fit failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// Cluster assignment
// ============================================================================

bool KMeansOperation::assignClusters(
    arma::mat const & features,
    arma::Row<std::size_t> & assignments)
{
    if (!_impl->trained) {
        std::cerr << "KMeansOperation::assignClusters: Model not fitted.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "KMeansOperation::assignClusters: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->num_features) {
        std::cerr << "KMeansOperation::assignClusters: Feature dimension mismatch — "
                  << "expected " << _impl->num_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        // Assign each observation to its nearest centroid
        assignments.set_size(features.n_cols);

        for (std::size_t i = 0; i < features.n_cols; ++i) {
            arma::vec const obs = features.col(i);
            double min_dist = std::numeric_limits<double>::max();
            std::size_t best_cluster = 0;

            for (std::size_t c = 0; c < _impl->centroids.n_cols; ++c) {
                double const dist = arma::norm(obs - _impl->centroids.col(c), 2);
                if (dist < min_dist) {
                    min_dist = dist;
                    best_cluster = c;
                }
            }

            assignments(i) = best_cluster;
        }

        return true;

    } catch (std::exception const & e) {
        std::cerr << "KMeansOperation::assignClusters failed: " << e.what() << "\n";
        return false;
    }
}

// ============================================================================
// Serialization
// ============================================================================

bool KMeansOperation::save(std::ostream & out) const
{
    if (!_impl->trained) {
        return false;
    }

    try {
        cereal::BinaryOutputArchive ar(out);
        ar(cereal::make_nvp("num_clusters", _impl->num_clusters));
        ar(cereal::make_nvp("num_features", _impl->num_features));

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
        std::cerr << "KMeansOperation::save failed: " << e.what() << "\n";
        return false;
    }
}

bool KMeansOperation::load(std::istream & in)
{
    try {
        cereal::BinaryInputArchive ar(in);

        std::size_t num_clusters = 0;
        std::size_t num_features = 0;
        ar(cereal::make_nvp("num_clusters", num_clusters));
        ar(cereal::make_nvp("num_features", num_features));

        std::size_t n_rows = 0;
        std::size_t n_cols = 0;
        ar(cereal::make_nvp("centroids_rows", n_rows));
        ar(cereal::make_nvp("centroids_cols", n_cols));

        std::vector<double> data;
        ar(cereal::make_nvp("centroids_data", data));

        if (data.size() != n_rows * n_cols) {
            std::cerr << "KMeansOperation::load: centroid data size mismatch.\n";
            _impl->trained = false;
            return false;
        }

        _impl->centroids = arma::mat(data.data(), n_rows, n_cols);
        _impl->num_clusters = num_clusters;
        _impl->num_features = num_features;
        _impl->trained = true;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "KMeansOperation::load failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// Query / introspection
// ============================================================================

bool KMeansOperation::isTrained() const
{
    return _impl->trained;
}

std::size_t KMeansOperation::numClasses() const
{
    return _impl->trained ? _impl->num_clusters : 0;
}

std::size_t KMeansOperation::numFeatures() const
{
    return _impl->trained ? _impl->num_features : 0;
}

// ============================================================================
// K-Means specific
// ============================================================================

arma::mat const & KMeansOperation::centroids() const
{
    static arma::mat const empty;
    return _impl->trained ? _impl->centroids : empty;
}

} // namespace MLCore
