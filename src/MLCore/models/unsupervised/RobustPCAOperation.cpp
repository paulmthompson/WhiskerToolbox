/**
 * @file RobustPCAOperation.cpp
 * @brief Implementation of Robust PCA dimensionality reduction via ROSL
 */

#include "RobustPCAOperation.hpp"

#include "rosl.hpp"

#include <cassert>
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <cmath>
#include <iostream>
#include <sstream>

namespace MLCore {

// ============================================================================
// Pimpl
// ============================================================================

struct RobustPCAOperation::Impl {
    arma::mat eigenvectors;     // (features × n_components) projection basis
    arma::vec eigenvalues;      // singular values squared (sorted descending)
    arma::vec mean;             // feature means for centering
    double total_variance = 0.0;// sum of all eigenvalues
    std::size_t n_components = 0;
    std::size_t n_features = 0;
    bool trained = false;
};

// ============================================================================
// Special members
// ============================================================================

RobustPCAOperation::RobustPCAOperation()
    : _impl(std::make_unique<Impl>()) {
}

RobustPCAOperation::~RobustPCAOperation() = default;

RobustPCAOperation::RobustPCAOperation(RobustPCAOperation &&) noexcept = default;
RobustPCAOperation & RobustPCAOperation::operator=(RobustPCAOperation &&) noexcept = default;

// ============================================================================
// Identity
// ============================================================================

std::string RobustPCAOperation::getName() const {
    return "Robust PCA";
}

std::unique_ptr<MLModelParametersBase> RobustPCAOperation::getDefaultParameters() const {
    return std::make_unique<RobustPCAParameters>();
}

bool RobustPCAOperation::isTrained() const {
    return _impl->trained;
}

std::size_t RobustPCAOperation::numFeatures() const {
    return _impl->trained ? _impl->n_features : 0;
}

// ============================================================================
// fitTransform — ROSL decomposition → SVD on low-rank component
// ============================================================================

bool RobustPCAOperation::fitTransform(
        arma::mat const & features,
        MLModelParametersBase const * params,
        arma::mat & result) {

    if (features.empty()) {
        std::cerr << "RobustPCAOperation::fitTransform: Feature matrix is empty.\n";
        return false;
    }

    auto const * rpca_params = dynamic_cast<RobustPCAParameters const *>(params);
    RobustPCAParameters const defaults;
    if (!rpca_params) {
        rpca_params = &defaults;
    }

    std::size_t const n_components = rpca_params->n_components;
    double lambda = rpca_params->lambda;
    double const tol = rpca_params->tol;
    auto const max_iter = static_cast<uint32_t>(rpca_params->max_iter);

    if (n_components == 0) {
        std::cerr << "RobustPCAOperation::fitTransform: n_components must be > 0.\n";
        return false;
    }
    if (n_components > features.n_rows) {
        std::cerr << "RobustPCAOperation::fitTransform: n_components ("
                  << n_components << ") exceeds number of features ("
                  << features.n_rows << ").\n";
        return false;
    }
    if (features.n_cols < 2) {
        std::cerr << "RobustPCAOperation::fitTransform: Need at least 2 observations.\n";
        return false;
    }

    // Auto-compute lambda if not provided (common default: 1/sqrt(max(m,n)))
    if (lambda <= 0.0) {
        lambda = 1.0 / std::sqrt(static_cast<double>(std::max(features.n_rows, features.n_cols)));
    }

    try {
        // 1. Center the data
        _impl->mean = arma::mean(features, 1);
        arma::mat const centered = features.each_col() - _impl->mean;

        // 2. Run ROSL to decompose: centered = A + E
        arma::mat A;
        arma::mat E;
        auto const max_rank = static_cast<uint32_t>(std::min(
                static_cast<std::size_t>(std::min(centered.n_rows, centered.n_cols)),
                n_components * 2));// give ROSL room to find the right rank

        rosl_lrs<double>(centered, A, E,
                         lambda, tol,
                         false,// no subsampling
                         0.0,  // sampleLFrac (unused)
                         0.0,  // sampleHFrac (unused)
                         max_rank,
                         max_iter,
                         42);// fixed seed for reproducibility

        // 3. SVD on the low-rank component A to get principal directions
        arma::vec singular_values;
        arma::mat U;
        arma::mat V;

        if (!arma::svd_econ(U, singular_values, V, A)) {
            std::cerr << "RobustPCAOperation::fitTransform: SVD of low-rank component failed.\n";
            return false;
        }

        // Clamp to available rank
        std::size_t const available_rank = singular_values.n_elem;
        std::size_t const actual_components = std::min(n_components, available_rank);

        if (actual_components == 0) {
            std::cerr << "RobustPCAOperation::fitTransform: ROSL produced rank-0 decomposition.\n";
            return false;
        }

        // Store eigenvectors (top n_components left-singular vectors)
        _impl->eigenvectors = U.cols(0, actual_components - 1);

        // Store eigenvalues as squared singular values / (n-1) for variance interpretation
        double const scale = 1.0 / static_cast<double>(features.n_cols - 1);
        _impl->eigenvalues = arma::square(singular_values.subvec(0, actual_components - 1)) * scale;

        // Total variance from all singular values
        _impl->total_variance = arma::accu(arma::square(singular_values)) * scale;

        _impl->n_components = actual_components;
        _impl->n_features = features.n_rows;
        _impl->trained = true;

        // 4. Project: result = eigenvectors^T * centered
        result = _impl->eigenvectors.t() * centered;

        return true;

    } catch (std::exception const & e) {
        std::cerr << "RobustPCAOperation::fitTransform failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// transform — project new data using learned basis
// ============================================================================

bool RobustPCAOperation::transform(
        arma::mat const & features,
        arma::mat & result) {
    if (!_impl->trained) {
        std::cerr << "RobustPCAOperation::transform: Model not fitted.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "RobustPCAOperation::transform: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->n_features) {
        std::cerr << "RobustPCAOperation::transform: Feature dimension mismatch — "
                  << "expected " << _impl->n_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        arma::mat const centered = features.each_col() - _impl->mean;
        result = _impl->eigenvectors.t() * centered;
        return true;
    } catch (std::exception const & e) {
        std::cerr << "RobustPCAOperation::transform failed: " << e.what() << "\n";
        return false;
    }
}

// ============================================================================
// Accessors
// ============================================================================

std::size_t RobustPCAOperation::outputDimensions() const {
    return _impl->trained ? _impl->n_components : 0;
}

std::vector<double> RobustPCAOperation::explainedVarianceRatio() const {
    if (!_impl->trained || _impl->total_variance <= 0.0) {
        return {};
    }

    std::vector<double> ratios(_impl->eigenvalues.n_elem);
    for (std::size_t i = 0; i < _impl->eigenvalues.n_elem; ++i) {
        ratios[i] = _impl->eigenvalues(i) / _impl->total_variance;
    }
    return ratios;
}

// ============================================================================
// Serialization
// ============================================================================

bool RobustPCAOperation::save(std::ostream & out) const {
    if (!_impl->trained) {
        return false;
    }

    try {
        cereal::BinaryOutputArchive ar(out);

        // Store dimensions
        ar(_impl->n_components, _impl->n_features, _impl->total_variance);

        // Store matrices as vectors
        std::vector<double> const eigvec_data(
                _impl->eigenvectors.memptr(),
                _impl->eigenvectors.memptr() + _impl->eigenvectors.n_elem);
        ar(eigvec_data, _impl->eigenvectors.n_rows, _impl->eigenvectors.n_cols);

        std::vector<double> const eigval_data(
                _impl->eigenvalues.memptr(),
                _impl->eigenvalues.memptr() + _impl->eigenvalues.n_elem);
        ar(eigval_data);

        std::vector<double> const mean_data(
                _impl->mean.memptr(),
                _impl->mean.memptr() + _impl->mean.n_elem);
        ar(mean_data);

        return true;
    } catch (std::exception const & e) {
        std::cerr << "RobustPCAOperation::save failed: " << e.what() << "\n";
        return false;
    }
}

bool RobustPCAOperation::load(std::istream & in) {
    try {
        cereal::BinaryInputArchive ar(in);

        ar(_impl->n_components, _impl->n_features, _impl->total_variance);

        std::vector<double> eigvec_data;
        arma::uword eigvec_rows = 0;
        arma::uword eigvec_cols = 0;
        ar(eigvec_data, eigvec_rows, eigvec_cols);
        _impl->eigenvectors = arma::mat(eigvec_data.data(), eigvec_rows, eigvec_cols);

        std::vector<double> eigval_data;
        ar(eigval_data);
        _impl->eigenvalues = arma::vec(eigval_data);

        std::vector<double> mean_data;
        ar(mean_data);
        _impl->mean = arma::vec(mean_data);

        _impl->trained = true;
        return true;
    } catch (std::exception const & e) {
        std::cerr << "RobustPCAOperation::load failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

}// namespace MLCore
