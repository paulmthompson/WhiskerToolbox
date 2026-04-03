/**
 * @file SupervisedRobustPCAOperation.cpp
 * @brief Implementation of Supervised Robust PCA dimensionality reduction
 *
 * Fits Robust PCA (via ROSL decomposition) on the labeled subset of
 * observations, then projects all data through the resulting eigenvectors.
 * The centering statistics are computed from the labeled subset only.
 */

#include "SupervisedRobustPCAOperation.hpp"

#include "MLCore/models/unsupervised/rosl.hpp"

#include <cassert>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cmath>
#include <iostream>

namespace MLCore {

// ============================================================================
// Pimpl
// ============================================================================

struct SupervisedRobustPCAOperation::Impl {
    arma::mat eigenvectors;     ///< (D × n_components) projection matrix
    arma::vec eigenvalues;      ///< Singular values squared / (n-1), sorted descending
    arma::vec mean;             ///< Feature means (from labeled subset)
    double total_variance = 0.0;///< Sum of all eigenvalues
    std::size_t n_components = 0;
    std::size_t n_features = 0;
    std::size_t num_classes = 0;
    bool trained = false;
    std::vector<std::string> col_names;///< "SRPCA:0", "SRPCA:1", ...
};

// ============================================================================
// Construction / destruction / move
// ============================================================================

SupervisedRobustPCAOperation::SupervisedRobustPCAOperation()
    : _impl(std::make_unique<Impl>()) {
}

SupervisedRobustPCAOperation::~SupervisedRobustPCAOperation() = default;

SupervisedRobustPCAOperation::SupervisedRobustPCAOperation(SupervisedRobustPCAOperation &&) noexcept = default;
SupervisedRobustPCAOperation & SupervisedRobustPCAOperation::operator=(SupervisedRobustPCAOperation &&) noexcept = default;

// ============================================================================
// Identity & metadata
// ============================================================================

std::string SupervisedRobustPCAOperation::getName() const {
    return "Supervised Robust PCA";
}

MLTaskType SupervisedRobustPCAOperation::getTaskType() const {
    return MLTaskType::SupervisedDimensionalityReduction;
}

std::unique_ptr<MLModelParametersBase> SupervisedRobustPCAOperation::getDefaultParameters() const {
    return std::make_unique<SupervisedRobustPCAParameters>();
}

// ============================================================================
// fitTransform
// ============================================================================

bool SupervisedRobustPCAOperation::fitTransform(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        MLModelParametersBase const * params,
        arma::mat & result) {

    // ------------------------------------------------------------------
    // Validate inputs
    // ------------------------------------------------------------------
    if (features.empty()) {
        std::cerr << "SupervisedRobustPCAOperation::fitTransform: Feature matrix is empty.\n";
        return false;
    }
    if (labels.empty()) {
        std::cerr << "SupervisedRobustPCAOperation::fitTransform: Labels are empty.\n";
        return false;
    }
    if (features.n_cols != labels.n_elem) {
        std::cerr << "SupervisedRobustPCAOperation::fitTransform: Feature columns ("
                  << features.n_cols << ") != label count ("
                  << labels.n_elem << ").\n";
        return false;
    }

    // Detect number of classes
    auto const max_label = arma::max(labels);
    std::size_t const num_classes = max_label + 1;
    if (num_classes < 2) {
        std::cerr << "SupervisedRobustPCAOperation::fitTransform: Need at least 2 classes, "
                  << "found " << num_classes << ".\n";
        return false;
    }

    // Extract parameters
    auto const * srpca_params = dynamic_cast<SupervisedRobustPCAParameters const *>(params);
    SupervisedRobustPCAParameters const defaults;
    if (!srpca_params) {
        srpca_params = &defaults;
    }

    std::size_t const n_components = srpca_params->n_components;
    double lambda = srpca_params->lambda;
    double const tol = srpca_params->tol;
    auto const max_iter = static_cast<uint32_t>(srpca_params->max_iter);

    if (n_components == 0) {
        std::cerr << "SupervisedRobustPCAOperation::fitTransform: n_components must be > 0.\n";
        return false;
    }
    if (n_components > features.n_rows) {
        std::cerr << "SupervisedRobustPCAOperation::fitTransform: n_components ("
                  << n_components << ") exceeds number of features ("
                  << features.n_rows << ").\n";
        return false;
    }

    // The SupervisedDimReductionPipeline passes only labeled observations
    // through the labels vector; treat ALL columns as the fitting subset.
    arma::mat const & labeled_features = features;
    std::size_t const N_labeled = labeled_features.n_cols;

    if (N_labeled < 2) {
        std::cerr << "SupervisedRobustPCAOperation::fitTransform: Need at least 2 labeled "
                  << "observations for Robust PCA.\n";
        return false;
    }

    // Auto-compute lambda if not provided
    if (lambda <= 0.0) {
        lambda = 1.0 / std::sqrt(static_cast<double>(
                               std::max(labeled_features.n_rows, labeled_features.n_cols)));
    }

    try {
        // ------------------------------------------------------------------
        // 1. Center data using labeled subset statistics
        // ------------------------------------------------------------------
        _impl->mean = arma::mean(labeled_features, 1);
        arma::mat const centered = labeled_features.each_col() - _impl->mean;

        // ------------------------------------------------------------------
        // 2. Run ROSL to decompose: centered = A + E
        // ------------------------------------------------------------------
        arma::mat A;
        arma::mat E;
        auto const max_rank = static_cast<uint32_t>(std::min(
                static_cast<std::size_t>(std::min(centered.n_rows, centered.n_cols)),
                n_components * 2));

        rosl_lrs<double>(centered, A, E,
                         lambda, tol,
                         false,// no subsampling
                         0.0,  // sampleLFrac (unused)
                         0.0,  // sampleHFrac (unused)
                         max_rank,
                         max_iter,
                         42);// fixed seed for reproducibility

        // ------------------------------------------------------------------
        // 3. SVD on the low-rank component A
        // ------------------------------------------------------------------
        arma::vec singular_values;
        arma::mat U;
        arma::mat V;

        if (!arma::svd_econ(U, singular_values, V, A)) {
            std::cerr << "SupervisedRobustPCAOperation::fitTransform: SVD of low-rank component failed.\n";
            return false;
        }

        // Clamp to available rank
        std::size_t const available_rank = singular_values.n_elem;
        std::size_t const actual_components = std::min(n_components, available_rank);

        if (actual_components == 0) {
            std::cerr << "SupervisedRobustPCAOperation::fitTransform: ROSL produced rank-0 decomposition.\n";
            return false;
        }

        // Store eigenvectors (top n_components left-singular vectors)
        _impl->eigenvectors = U.cols(0, actual_components - 1);

        // Store eigenvalues as squared singular values / (n-1) for variance interpretation
        double const scale = 1.0 / static_cast<double>(N_labeled - 1);
        _impl->eigenvalues = arma::square(singular_values.subvec(0, actual_components - 1)) * scale;

        // Total variance from all singular values
        _impl->total_variance = arma::accu(arma::square(singular_values)) * scale;

        _impl->n_components = actual_components;
        _impl->n_features = features.n_rows;
        _impl->num_classes = num_classes;
        _impl->trained = true;

        // Generate column names
        _impl->col_names.resize(actual_components);
        for (std::size_t i = 0; i < actual_components; ++i) {
            _impl->col_names[i] = "SRPCA:" + std::to_string(i);
        }

        // ------------------------------------------------------------------
        // 4. Project ALL data (not just labeled subset)
        // ------------------------------------------------------------------
        arma::mat const all_centered = features.each_col() - _impl->mean;
        result = _impl->eigenvectors.t() * all_centered;

        return true;

    } catch (std::exception const & e) {
        std::cerr << "SupervisedRobustPCAOperation::fitTransform failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// transform
// ============================================================================

bool SupervisedRobustPCAOperation::transform(
        arma::mat const & features,
        arma::mat & result) {
    if (!_impl->trained) {
        std::cerr << "SupervisedRobustPCAOperation::transform: Model not fitted.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "SupervisedRobustPCAOperation::transform: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->n_features) {
        std::cerr << "SupervisedRobustPCAOperation::transform: Feature dimension mismatch — "
                  << "expected " << _impl->n_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        arma::mat const centered = features.each_col() - _impl->mean;
        result = _impl->eigenvectors.t() * centered;
        return true;
    } catch (std::exception const & e) {
        std::cerr << "SupervisedRobustPCAOperation::transform failed: " << e.what() << "\n";
        return false;
    }
}

// ============================================================================
// Query / introspection
// ============================================================================

std::size_t SupervisedRobustPCAOperation::outputDimensions() const {
    return _impl->trained ? _impl->n_components : 0;
}

std::vector<std::string> SupervisedRobustPCAOperation::outputColumnNames() const {
    return _impl->col_names;
}

bool SupervisedRobustPCAOperation::isTrained() const {
    return _impl->trained;
}

std::size_t SupervisedRobustPCAOperation::numClasses() const {
    return _impl->trained ? _impl->num_classes : 0;
}

std::size_t SupervisedRobustPCAOperation::numFeatures() const {
    return _impl->trained ? _impl->n_features : 0;
}

std::vector<double> SupervisedRobustPCAOperation::explainedVarianceRatio() const {
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

bool SupervisedRobustPCAOperation::save(std::ostream & out) const {
    if (!_impl->trained) {
        return false;
    }
    try {
        cereal::BinaryOutputArchive ar(out);

        ar(cereal::make_nvp("n_components", _impl->n_components));
        ar(cereal::make_nvp("n_features", _impl->n_features));
        ar(cereal::make_nvp("num_classes", _impl->num_classes));
        ar(cereal::make_nvp("total_variance", _impl->total_variance));
        ar(cereal::make_nvp("col_names", _impl->col_names));

        // Eigenvectors
        auto const ev_rows = _impl->eigenvectors.n_rows;
        auto const ev_cols = _impl->eigenvectors.n_cols;
        ar(cereal::make_nvp("ev_rows", ev_rows));
        ar(cereal::make_nvp("ev_cols", ev_cols));
        std::vector<double> const ev_data(
                _impl->eigenvectors.memptr(),
                _impl->eigenvectors.memptr() + _impl->eigenvectors.n_elem);
        ar(cereal::make_nvp("ev_data", ev_data));

        // Eigenvalues
        std::vector<double> const eigval_data(
                _impl->eigenvalues.memptr(),
                _impl->eigenvalues.memptr() + _impl->eigenvalues.n_elem);
        ar(cereal::make_nvp("eigval_data", eigval_data));

        // Mean
        std::vector<double> const mean_data(
                _impl->mean.memptr(),
                _impl->mean.memptr() + _impl->mean.n_elem);
        ar(cereal::make_nvp("mean_data", mean_data));

        return true;
    } catch (std::exception const & e) {
        std::cerr << "SupervisedRobustPCAOperation::save failed: " << e.what() << "\n";
        return false;
    }
}

bool SupervisedRobustPCAOperation::load(std::istream & in) {
    try {
        cereal::BinaryInputArchive ar(in);

        ar(_impl->n_components, _impl->n_features, _impl->num_classes);
        ar(_impl->total_variance);
        ar(_impl->col_names);

        // Eigenvectors
        arma::uword ev_rows = 0;
        arma::uword ev_cols = 0;
        ar(ev_rows, ev_cols);
        std::vector<double> ev_data;
        ar(ev_data);
        _impl->eigenvectors = arma::mat(ev_data.data(), ev_rows, ev_cols);

        // Eigenvalues
        std::vector<double> eigval_data;
        ar(eigval_data);
        _impl->eigenvalues = arma::vec(eigval_data);

        // Mean
        std::vector<double> mean_data;
        ar(mean_data);
        _impl->mean = arma::vec(mean_data);

        _impl->trained = true;
        return true;
    } catch (std::exception const & e) {
        std::cerr << "SupervisedRobustPCAOperation::load failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

}// namespace MLCore
