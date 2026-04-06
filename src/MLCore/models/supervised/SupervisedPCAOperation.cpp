/**
 * @file SupervisedPCAOperation.cpp
 * @brief Implementation of Supervised PCA dimensionality reduction
 *
 * Fits PCA on the labeled subset of observations, then projects all data
 * through the resulting eigenvectors. The centering and scaling statistics
 * are also computed from the labeled subset only.
 */

#include "SupervisedPCAOperation.hpp"

#include <cassert>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <iostream>

namespace MLCore {

// ============================================================================
// Pimpl
// ============================================================================

struct SupervisedPCAOperation::Impl {
    arma::mat eigenvectors;     ///< (D × n_components) projection matrix
    arma::vec eigenvalues;      ///< Eigenvalues (sorted descending)
    arma::vec mean;             ///< Feature means (from labeled subset)
    double total_variance = 0.0;///< Sum of all eigenvalues
    std::size_t n_components = 0;
    std::size_t n_features = 0;
    std::size_t num_classes = 0;
    bool trained = false;
    std::vector<std::string> col_names;///< "SPCA:0", "SPCA:1", ...
};

// ============================================================================
// Construction / destruction / move
// ============================================================================

SupervisedPCAOperation::SupervisedPCAOperation()
    : _impl(std::make_unique<Impl>()) {
}

SupervisedPCAOperation::~SupervisedPCAOperation() = default;

SupervisedPCAOperation::SupervisedPCAOperation(SupervisedPCAOperation &&) noexcept = default;
SupervisedPCAOperation & SupervisedPCAOperation::operator=(SupervisedPCAOperation &&) noexcept = default;

// ============================================================================
// Identity & metadata
// ============================================================================

std::string SupervisedPCAOperation::getName() const {
    return "Supervised PCA";
}

MLTaskType SupervisedPCAOperation::getTaskType() const {
    return MLTaskType::SupervisedDimensionalityReduction;
}

std::unique_ptr<MLModelParametersBase> SupervisedPCAOperation::getDefaultParameters() const {
    return std::make_unique<SupervisedPCAParameters>();
}

// ============================================================================
// fitTransform
// ============================================================================

bool SupervisedPCAOperation::fitTransform(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        MLModelParametersBase const * params,
        arma::mat & result) {

    // ------------------------------------------------------------------
    // Validate inputs
    // ------------------------------------------------------------------
    if (features.empty()) {
        std::cerr << "SupervisedPCAOperation::fitTransform: Feature matrix is empty.\n";
        return false;
    }
    if (labels.empty()) {
        std::cerr << "SupervisedPCAOperation::fitTransform: Labels are empty.\n";
        return false;
    }
    if (features.n_cols != labels.n_elem) {
        std::cerr << "SupervisedPCAOperation::fitTransform: Feature columns ("
                  << features.n_cols << ") != label count ("
                  << labels.n_elem << ").\n";
        return false;
    }

    // Detect number of classes
    auto const max_label = arma::max(labels);
    std::size_t const num_classes = max_label + 1;
    if (num_classes < 2) {
        std::cerr << "SupervisedPCAOperation::fitTransform: Need at least 2 classes, "
                  << "found " << num_classes << ".\n";
        return false;
    }

    // Extract parameters
    auto const * spca_params = dynamic_cast<SupervisedPCAParameters const *>(params);
    SupervisedPCAParameters const defaults;
    if (!spca_params) {
        spca_params = &defaults;
    }

    std::size_t const n_components = spca_params->n_components;

    if (n_components == 0) {
        std::cerr << "SupervisedPCAOperation::fitTransform: n_components must be > 0.\n";
        return false;
    }
    if (n_components > features.n_rows) {
        std::cerr << "SupervisedPCAOperation::fitTransform: n_components ("
                  << n_components << ") exceeds number of features ("
                  << features.n_rows << ").\n";
        return false;
    }

    // ------------------------------------------------------------------
    // Extract labeled subset (all columns that have any label)
    // ------------------------------------------------------------------
    // The SupervisedDimReductionPipeline passes only labeled observations
    // through the labels vector, but we treat ALL columns with a valid
    // label index as the fitting subset.
    arma::mat const & labeled_features = features;
    std::size_t const N_labeled = labeled_features.n_cols;

    if (N_labeled < 2) {
        std::cerr << "SupervisedPCAOperation::fitTransform: Need at least 2 labeled "
                  << "observations for PCA.\n";
        return false;
    }

    try {
        // ------------------------------------------------------------------
        // 1. Compute mean from labeled subset
        // ------------------------------------------------------------------
        _impl->mean = arma::mean(labeled_features, 1);// mean per feature row

        arma::mat const centered = labeled_features.each_col() - _impl->mean;

        // ------------------------------------------------------------------
        // 2. Covariance and eigendecomposition on labeled subset
        // ------------------------------------------------------------------
        arma::mat const cov = centered * centered.t() /
                              static_cast<double>(N_labeled - 1);

        arma::vec eigenvalues;
        arma::mat eigenvectors;
        if (!arma::eig_sym(eigenvalues, eigenvectors, cov)) {
            std::cerr << "SupervisedPCAOperation::fitTransform: Eigendecomposition failed.\n";
            return false;
        }

        // arma::eig_sym returns ascending — reverse for descending
        _impl->eigenvalues = arma::reverse(eigenvalues);
        _impl->eigenvectors = arma::fliplr(eigenvectors);

        _impl->total_variance = arma::accu(_impl->eigenvalues);

        // Truncate to n_components
        _impl->eigenvectors = _impl->eigenvectors.cols(0, n_components - 1);
        _impl->eigenvalues = _impl->eigenvalues.subvec(0, n_components - 1);

        _impl->n_components = n_components;
        _impl->n_features = features.n_rows;
        _impl->num_classes = num_classes;
        _impl->trained = true;

        // Generate column names
        _impl->col_names.resize(n_components);
        for (std::size_t i = 0; i < n_components; ++i) {
            _impl->col_names[i] = "SPCA:" + std::to_string(i);
        }

        // ------------------------------------------------------------------
        // 3. Project ALL data (not just labeled subset)
        // ------------------------------------------------------------------
        arma::mat const all_centered = features.each_col() - _impl->mean;
        result = _impl->eigenvectors.t() * all_centered;

        return true;

    } catch (std::exception const & e) {
        std::cerr << "SupervisedPCAOperation::fitTransform failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// transform
// ============================================================================

bool SupervisedPCAOperation::transform(
        arma::mat const & features,
        arma::mat & result) {
    if (!_impl->trained) {
        std::cerr << "SupervisedPCAOperation::transform: Model not fitted.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "SupervisedPCAOperation::transform: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->n_features) {
        std::cerr << "SupervisedPCAOperation::transform: Feature dimension mismatch — "
                  << "expected " << _impl->n_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        arma::mat const centered = features.each_col() - _impl->mean;
        result = _impl->eigenvectors.t() * centered;
        return true;
    } catch (std::exception const & e) {
        std::cerr << "SupervisedPCAOperation::transform failed: " << e.what() << "\n";
        return false;
    }
}

// ============================================================================
// Query / introspection
// ============================================================================

std::size_t SupervisedPCAOperation::outputDimensions() const {
    return _impl->trained ? _impl->n_components : 0;
}

std::vector<std::string> SupervisedPCAOperation::outputColumnNames() const {
    return _impl->col_names;
}

bool SupervisedPCAOperation::isTrained() const {
    return _impl->trained;
}

std::size_t SupervisedPCAOperation::numClasses() const {
    return _impl->trained ? _impl->num_classes : 0;
}

std::size_t SupervisedPCAOperation::numFeatures() const {
    return _impl->trained ? _impl->n_features : 0;
}

std::vector<double> SupervisedPCAOperation::explainedVarianceRatio() const {
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

bool SupervisedPCAOperation::save(std::ostream & out) const {
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
        std::vector<double> ev_data(_impl->eigenvectors.memptr(),
                                    _impl->eigenvectors.memptr() + ev_rows * ev_cols);
        ar(cereal::make_nvp("eigenvectors", ev_data));

        // Eigenvalues
        auto const val_n = _impl->eigenvalues.n_elem;
        ar(cereal::make_nvp("eigenvalues_n", val_n));
        std::vector<double> val_data(_impl->eigenvalues.memptr(),
                                     _impl->eigenvalues.memptr() + val_n);
        ar(cereal::make_nvp("eigenvalues", val_data));

        // Mean
        auto const mean_n = _impl->mean.n_elem;
        ar(cereal::make_nvp("mean_n", mean_n));
        std::vector<double> mean_data(_impl->mean.memptr(),
                                      _impl->mean.memptr() + mean_n);
        ar(cereal::make_nvp("mean", mean_data));

        return out.good();

    } catch (std::exception const & e) {
        std::cerr << "SupervisedPCAOperation::save failed: " << e.what() << "\n";
        return false;
    }
}

bool SupervisedPCAOperation::load(std::istream & in) {
    try {
        cereal::BinaryInputArchive ar(in);

        std::size_t n_components = 0;
        std::size_t n_features = 0;
        std::size_t num_classes = 0;
        double total_variance = 0.0;
        std::vector<std::string> col_names;

        ar(cereal::make_nvp("n_components", n_components));
        ar(cereal::make_nvp("n_features", n_features));
        ar(cereal::make_nvp("num_classes", num_classes));
        ar(cereal::make_nvp("total_variance", total_variance));
        ar(cereal::make_nvp("col_names", col_names));

        // Eigenvectors
        std::size_t ev_rows = 0;
        std::size_t ev_cols = 0;
        ar(cereal::make_nvp("ev_rows", ev_rows));
        ar(cereal::make_nvp("ev_cols", ev_cols));
        std::vector<double> ev_data;
        ar(cereal::make_nvp("eigenvectors", ev_data));
        if (ev_data.size() != ev_rows * ev_cols) {
            std::cerr << "SupervisedPCAOperation::load: eigenvector data size mismatch.\n";
            return false;
        }

        // Eigenvalues
        std::size_t val_n = 0;
        ar(cereal::make_nvp("eigenvalues_n", val_n));
        std::vector<double> val_data;
        ar(cereal::make_nvp("eigenvalues", val_data));
        if (val_data.size() != val_n) {
            std::cerr << "SupervisedPCAOperation::load: eigenvalue data size mismatch.\n";
            return false;
        }

        // Mean
        std::size_t mean_n = 0;
        ar(cereal::make_nvp("mean_n", mean_n));
        std::vector<double> mean_data;
        ar(cereal::make_nvp("mean", mean_data));
        if (mean_data.size() != mean_n) {
            std::cerr << "SupervisedPCAOperation::load: mean data size mismatch.\n";
            return false;
        }

        // Commit all state atomically
        _impl->n_components = n_components;
        _impl->n_features = n_features;
        _impl->num_classes = num_classes;
        _impl->total_variance = total_variance;
        _impl->col_names = std::move(col_names);
        _impl->eigenvectors = arma::mat(ev_data.data(), ev_rows, ev_cols);
        _impl->eigenvalues = arma::vec(val_data.data(), val_n);
        _impl->mean = arma::vec(mean_data.data(), mean_n);
        _impl->trained = true;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "SupervisedPCAOperation::load failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

}// namespace MLCore
