/**
 * @file GaussianMixtureOperation.cpp
 * @brief Implementation of Gaussian Mixture Model clustering via mlpack
 */

#include "GaussianMixtureOperation.hpp"

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

struct GaussianMixtureOperation::Impl {
    /// Fitted component means (one vector per component)
    std::vector<arma::vec> means;
    /// Fitted component covariance matrices (one matrix per component)
    std::vector<arma::mat> covariances;
    /// Fitted component weights (mixing coefficients, sum to 1)
    arma::vec weights;

    std::size_t num_components = 0;
    std::size_t num_features = 0;
    bool trained = false;

    /// Reconstruct an mlpack::GMM from stored parameters
    [[nodiscard]] mlpack::GMM toMlpackGMM() const
    {
        // Build GaussianDistribution vector
        std::vector<mlpack::GaussianDistribution<>> dists;
        dists.reserve(num_components);
        for (std::size_t i = 0; i < num_components; ++i) {
            mlpack::GaussianDistribution<> dist(num_features);
            dist.Mean() = means[i];
            dist.Covariance(covariances[i]);
            dists.push_back(std::move(dist));
        }
        return mlpack::GMM(dists, weights);
    }
};

// ============================================================================
// Construction / destruction / move
// ============================================================================

GaussianMixtureOperation::GaussianMixtureOperation()
    : _impl(std::make_unique<Impl>())
{
}

GaussianMixtureOperation::~GaussianMixtureOperation() = default;

GaussianMixtureOperation::GaussianMixtureOperation(GaussianMixtureOperation &&) noexcept = default;
GaussianMixtureOperation & GaussianMixtureOperation::operator=(GaussianMixtureOperation &&) noexcept = default;

// ============================================================================
// Identity & metadata
// ============================================================================

std::string GaussianMixtureOperation::getName() const
{
    return "Gaussian Mixture Model";
}

MLTaskType GaussianMixtureOperation::getTaskType() const
{
    return MLTaskType::Clustering;
}

std::unique_ptr<MLModelParametersBase> GaussianMixtureOperation::getDefaultParameters() const
{
    return std::make_unique<GMMParameters>();
}

// ============================================================================
// Fitting
// ============================================================================

bool GaussianMixtureOperation::fit(
    arma::mat const & features,
    MLModelParametersBase const * params)
{
    // Validate inputs
    if (features.empty()) {
        std::cerr << "GaussianMixtureOperation::fit: Feature matrix is empty.\n";
        return false;
    }

    // Extract parameters (use defaults if nullptr or wrong type)
    auto const * gmm_params = dynamic_cast<GMMParameters const *>(params);
    GMMParameters defaults;
    if (!gmm_params) {
        gmm_params = &defaults;
    }

    std::size_t const k = gmm_params->k;
    std::size_t const max_iterations = gmm_params->max_iterations;

    // Validate k
    if (k == 0) {
        std::cerr << "GaussianMixtureOperation::fit: k must be > 0.\n";
        return false;
    }
    if (k > features.n_cols) {
        std::cerr << "GaussianMixtureOperation::fit: k (" << k
                  << ") exceeds number of observations ("
                  << features.n_cols << ").\n";
        return false;
    }

    try {
        // Set random seed if provided
        if (gmm_params->seed.has_value()) {
            mlpack::RandomSeed(static_cast<size_t>(gmm_params->seed.value()));
        }

        // Create GMM with k components and correct dimensionality
        mlpack::GMM gmm(k, features.n_rows);

        // Create EMFit with the specified max_iterations
        mlpack::EMFit<> fitter(max_iterations);

        // Train using EM algorithm
        gmm.Train(features, /*trials=*/1, /*useExistingModel=*/false, fitter);

        // Extract fitted parameters
        _impl->num_components = k;
        _impl->num_features = features.n_rows;
        _impl->means.clear();
        _impl->covariances.clear();
        _impl->means.reserve(k);
        _impl->covariances.reserve(k);

        for (std::size_t i = 0; i < k; ++i) {
            _impl->means.push_back(gmm.Component(i).Mean());
            _impl->covariances.push_back(gmm.Component(i).Covariance());
        }
        _impl->weights = gmm.Weights();
        _impl->trained = true;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "GaussianMixtureOperation::fit failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// Cluster assignment
// ============================================================================

bool GaussianMixtureOperation::assignClusters(
    arma::mat const & features,
    arma::Row<std::size_t> & assignments)
{
    if (!_impl->trained) {
        std::cerr << "GaussianMixtureOperation::assignClusters: Model not fitted.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "GaussianMixtureOperation::assignClusters: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->num_features) {
        std::cerr << "GaussianMixtureOperation::assignClusters: Feature dimension mismatch — "
                  << "expected " << _impl->num_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        // Reconstruct mlpack::GMM and use its Classify method
        auto gmm = _impl->toMlpackGMM();
        gmm.Classify(features, assignments);
        return true;

    } catch (std::exception const & e) {
        std::cerr << "GaussianMixtureOperation::assignClusters failed: "
                  << e.what() << "\n";
        return false;
    }
}

// ============================================================================
// Cluster probabilities (GMM-specific)
// ============================================================================

bool GaussianMixtureOperation::clusterProbabilities(
    arma::mat const & features,
    arma::mat & probs) const
{
    if (!_impl->trained) {
        std::cerr << "GaussianMixtureOperation::clusterProbabilities: Model not fitted.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "GaussianMixtureOperation::clusterProbabilities: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->num_features) {
        std::cerr << "GaussianMixtureOperation::clusterProbabilities: Feature dimension mismatch — "
                  << "expected " << _impl->num_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        auto gmm = _impl->toMlpackGMM();
        std::size_t const n_obs = features.n_cols;
        std::size_t const k = _impl->num_components;

        probs.set_size(k, n_obs);

        for (std::size_t i = 0; i < n_obs; ++i) {
            arma::vec const obs = features.col(i);

            // Compute weighted log-probability for each component
            arma::vec log_probs(k);
            for (std::size_t c = 0; c < k; ++c) {
                log_probs(c) = std::log(_impl->weights(c))
                              + gmm.Component(c).LogProbability(obs);
            }

            // Log-sum-exp for numerical stability
            double const max_log = log_probs.max();
            arma::vec shifted = arma::exp(log_probs - max_log);
            double const sum = arma::sum(shifted);

            probs.col(i) = shifted / sum;
        }

        return true;

    } catch (std::exception const & e) {
        std::cerr << "GaussianMixtureOperation::clusterProbabilities failed: "
                  << e.what() << "\n";
        return false;
    }
}

// ============================================================================
// Serialization
// ============================================================================

bool GaussianMixtureOperation::save(std::ostream & out) const
{
    if (!_impl->trained) {
        return false;
    }

    try {
        cereal::BinaryOutputArchive ar(out);
        ar(cereal::make_nvp("num_components", _impl->num_components));
        ar(cereal::make_nvp("num_features", _impl->num_features));

        // Serialize weights
        std::vector<double> weights_vec(
            _impl->weights.memptr(),
            _impl->weights.memptr() + _impl->weights.n_elem);
        ar(cereal::make_nvp("weights", weights_vec));

        // Serialize means — each as a flat vector
        for (std::size_t i = 0; i < _impl->num_components; ++i) {
            std::vector<double> mean_data(
                _impl->means[i].memptr(),
                _impl->means[i].memptr() + _impl->means[i].n_elem);
            ar(cereal::make_nvp("mean", mean_data));
        }

        // Serialize covariances — each as flat vector + dimensions
        for (std::size_t i = 0; i < _impl->num_components; ++i) {
            auto const & cov = _impl->covariances[i];
            std::size_t cov_rows = cov.n_rows;
            std::size_t cov_cols = cov.n_cols;
            ar(cereal::make_nvp("cov_rows", cov_rows));
            ar(cereal::make_nvp("cov_cols", cov_cols));

            std::vector<double> cov_data(
                cov.memptr(),
                cov.memptr() + cov_rows * cov_cols);
            ar(cereal::make_nvp("cov_data", cov_data));
        }

        return out.good();
    } catch (std::exception const & e) {
        std::cerr << "GaussianMixtureOperation::save failed: " << e.what() << "\n";
        return false;
    }
}

bool GaussianMixtureOperation::load(std::istream & in)
{
    try {
        cereal::BinaryInputArchive ar(in);

        std::size_t num_components = 0;
        std::size_t num_features = 0;
        ar(cereal::make_nvp("num_components", num_components));
        ar(cereal::make_nvp("num_features", num_features));

        // Load weights
        std::vector<double> weights_vec;
        ar(cereal::make_nvp("weights", weights_vec));

        if (weights_vec.size() != num_components) {
            std::cerr << "GaussianMixtureOperation::load: weights size mismatch.\n";
            _impl->trained = false;
            return false;
        }

        // Load means
        std::vector<arma::vec> means;
        means.reserve(num_components);
        for (std::size_t i = 0; i < num_components; ++i) {
            std::vector<double> mean_data;
            ar(cereal::make_nvp("mean", mean_data));
            if (mean_data.size() != num_features) {
                std::cerr << "GaussianMixtureOperation::load: mean size mismatch for component "
                          << i << ".\n";
                _impl->trained = false;
                return false;
            }
            means.emplace_back(arma::vec(mean_data.data(), num_features));
        }

        // Load covariances
        std::vector<arma::mat> covariances;
        covariances.reserve(num_components);
        for (std::size_t i = 0; i < num_components; ++i) {
            std::size_t cov_rows = 0;
            std::size_t cov_cols = 0;
            ar(cereal::make_nvp("cov_rows", cov_rows));
            ar(cereal::make_nvp("cov_cols", cov_cols));

            std::vector<double> cov_data;
            ar(cereal::make_nvp("cov_data", cov_data));

            if (cov_data.size() != cov_rows * cov_cols) {
                std::cerr << "GaussianMixtureOperation::load: covariance data size mismatch "
                          << "for component " << i << ".\n";
                _impl->trained = false;
                return false;
            }
            covariances.emplace_back(arma::mat(cov_data.data(), cov_rows, cov_cols));
        }

        _impl->weights = arma::vec(weights_vec.data(), num_components);
        _impl->means = std::move(means);
        _impl->covariances = std::move(covariances);
        _impl->num_components = num_components;
        _impl->num_features = num_features;
        _impl->trained = true;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "GaussianMixtureOperation::load failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// Query / introspection
// ============================================================================

bool GaussianMixtureOperation::isTrained() const
{
    return _impl->trained;
}

std::size_t GaussianMixtureOperation::numClasses() const
{
    return _impl->trained ? _impl->num_components : 0;
}

std::size_t GaussianMixtureOperation::numFeatures() const
{
    return _impl->trained ? _impl->num_features : 0;
}

// ============================================================================
// GMM-specific accessors
// ============================================================================

std::vector<arma::vec> const & GaussianMixtureOperation::means() const
{
    static std::vector<arma::vec> const empty;
    return _impl->trained ? _impl->means : empty;
}

std::vector<arma::mat> const & GaussianMixtureOperation::covariances() const
{
    static std::vector<arma::mat> const empty;
    return _impl->trained ? _impl->covariances : empty;
}

arma::vec const & GaussianMixtureOperation::weights() const
{
    static arma::vec const empty;
    return _impl->trained ? _impl->weights : empty;
}

} // namespace MLCore
