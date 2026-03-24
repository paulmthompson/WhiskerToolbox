/**
 * @file PCAOperation.cpp
 * @brief Implementation of PCA dimensionality reduction via mlpack
 */

#include "PCAOperation.hpp"

#include "mlpack.hpp"

#include <cassert>
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <iostream>
#include <sstream>

namespace MLCore {

struct PCAOperation::Impl {
    arma::mat eigenvectors;     // (features × n_components) projection matrix
    arma::vec eigenvalues;      // Eigenvalues (sorted descending)
    arma::vec mean;             // Feature means (for centering)
    arma::vec stddev;           // Feature std devs (for scaling, empty if scale=false)
    double total_variance = 0.0;// Sum of all eigenvalues (for variance ratio)
    std::size_t n_components = 0;
    std::size_t n_features = 0;
    bool scaled = false;
    bool trained = false;
};

PCAOperation::PCAOperation()
    : _impl(std::make_unique<Impl>()) {
}

PCAOperation::~PCAOperation() = default;

PCAOperation::PCAOperation(PCAOperation &&) noexcept = default;
PCAOperation & PCAOperation::operator=(PCAOperation &&) noexcept = default;

std::string PCAOperation::getName() const {
    return "PCA";
}

std::unique_ptr<MLModelParametersBase> PCAOperation::getDefaultParameters() const {
    return std::make_unique<PCAParameters>();
}

bool PCAOperation::isTrained() const {
    return _impl->trained;
}

std::size_t PCAOperation::numFeatures() const {
    return _impl->trained ? _impl->n_features : 0;
}

bool PCAOperation::fitTransform(
        arma::mat const & features,
        MLModelParametersBase const * params,
        arma::mat & result) {
    if (features.empty()) {
        std::cerr << "PCAOperation::fitTransform: Feature matrix is empty.\n";
        return false;
    }

    auto const * pca_params = dynamic_cast<PCAParameters const *>(params);
    PCAParameters const defaults;
    if (!pca_params) {
        pca_params = &defaults;
    }

    std::size_t const n_components = pca_params->n_components;
    bool const scale = pca_params->scale;

    if (n_components == 0) {
        std::cerr << "PCAOperation::fitTransform: n_components must be > 0.\n";
        return false;
    }
    if (n_components > features.n_rows) {
        std::cerr << "PCAOperation::fitTransform: n_components ("
                  << n_components << ") exceeds number of features ("
                  << features.n_rows << ").\n";
        return false;
    }
    if (features.n_cols < 2) {
        std::cerr << "PCAOperation::fitTransform: Need at least 2 observations.\n";
        return false;
    }

    try {
        // 1. Compute and store mean
        _impl->mean = arma::mean(features, 1);// mean per feature (row)

        // 2. Center the data
        arma::mat centered = features.each_col() - _impl->mean;

        // 3. Optionally scale to unit variance
        _impl->scaled = scale;
        if (scale) {
            _impl->stddev = arma::stddev(features, 0, 1);// per-feature std dev
            // Replace zero std devs with 1 to avoid division by zero
            _impl->stddev.replace(0.0, 1.0);
            centered.each_col() /= _impl->stddev;
        } else {
            _impl->stddev.reset();
        }

        // 4. Compute covariance matrix and eigendecomposition
        arma::mat const cov = centered * centered.t() / static_cast<double>(features.n_cols - 1);

        arma::vec eigenvalues;
        arma::mat eigenvectors;
        if (!arma::eig_sym(eigenvalues, eigenvectors, cov)) {
            std::cerr << "PCAOperation::fitTransform: Eigendecomposition failed.\n";
            return false;
        }

        // arma::eig_sym returns ascending order — reverse for descending
        _impl->eigenvalues = arma::reverse(eigenvalues);
        _impl->eigenvectors = arma::fliplr(eigenvectors);

        // Store total variance for ratio computation
        _impl->total_variance = arma::accu(_impl->eigenvalues);

        // Truncate to n_components
        _impl->eigenvectors = _impl->eigenvectors.cols(0, n_components - 1);
        _impl->eigenvalues = _impl->eigenvalues.subvec(0, n_components - 1);

        _impl->n_components = n_components;
        _impl->n_features = features.n_rows;
        _impl->trained = true;

        // 5. Project: result = eigenvectors^T * centered
        result = _impl->eigenvectors.t() * centered;

        return true;

    } catch (std::exception const & e) {
        std::cerr << "PCAOperation::fitTransform failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

bool PCAOperation::transform(
        arma::mat const & features,
        arma::mat & result) {
    if (!_impl->trained) {
        std::cerr << "PCAOperation::transform: Model not fitted.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "PCAOperation::transform: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->n_features) {
        std::cerr << "PCAOperation::transform: Feature dimension mismatch — "
                  << "expected " << _impl->n_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        // Center
        arma::mat centered = features.each_col() - _impl->mean;
        // Scale (if fitted with scaling)
        if (_impl->scaled) {
            centered.each_col() /= _impl->stddev;
        }
        // Project
        result = _impl->eigenvectors.t() * centered;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "PCAOperation::transform failed: " << e.what() << "\n";
        return false;
    }
}

std::size_t PCAOperation::outputDimensions() const {
    return _impl->trained ? _impl->n_components : 0;
}

std::vector<double> PCAOperation::explainedVarianceRatio() const {
    if (!_impl->trained || _impl->total_variance <= 0.0) {
        return {};
    }

    std::vector<double> ratios(_impl->eigenvalues.n_elem);
    for (std::size_t i = 0; i < _impl->eigenvalues.n_elem; ++i) {
        ratios[i] = _impl->eigenvalues(i) / _impl->total_variance;
    }
    return ratios;
}

bool PCAOperation::save(std::ostream & out) const {
    if (!_impl->trained) {
        return false;
    }

    try {
        cereal::BinaryOutputArchive ar(out);

        ar(cereal::make_nvp("n_components", _impl->n_components));
        ar(cereal::make_nvp("n_features", _impl->n_features));
        ar(cereal::make_nvp("scaled", _impl->scaled));
        ar(cereal::make_nvp("total_variance", _impl->total_variance));

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

        // Stddev (if scaled)
        if (_impl->scaled) {
            auto const std_n = _impl->stddev.n_elem;
            ar(cereal::make_nvp("stddev_n", std_n));
            std::vector<double> std_data(_impl->stddev.memptr(),
                                         _impl->stddev.memptr() + std_n);
            ar(cereal::make_nvp("stddev", std_data));
        }

        return out.good();

    } catch (std::exception const & e) {
        std::cerr << "PCAOperation::save failed: " << e.what() << "\n";
        return false;
    }
}

bool PCAOperation::load(std::istream & in) {
    try {
        cereal::BinaryInputArchive ar(in);

        std::size_t n_components = 0;
        std::size_t n_features = 0;
        bool scaled = false;
        double total_variance = 0.0;

        ar(cereal::make_nvp("n_components", n_components));
        ar(cereal::make_nvp("n_features", n_features));
        ar(cereal::make_nvp("scaled", scaled));
        ar(cereal::make_nvp("total_variance", total_variance));

        // Eigenvectors
        std::size_t ev_rows = 0;
        std::size_t ev_cols = 0;
        ar(cereal::make_nvp("ev_rows", ev_rows));
        ar(cereal::make_nvp("ev_cols", ev_cols));
        std::vector<double> ev_data;
        ar(cereal::make_nvp("eigenvectors", ev_data));
        if (ev_data.size() != ev_rows * ev_cols) {
            std::cerr << "PCAOperation::load: eigenvector data size mismatch.\n";
            return false;
        }

        // Eigenvalues
        std::size_t val_n = 0;
        ar(cereal::make_nvp("eigenvalues_n", val_n));
        std::vector<double> val_data;
        ar(cereal::make_nvp("eigenvalues", val_data));
        if (val_data.size() != val_n) {
            std::cerr << "PCAOperation::load: eigenvalue data size mismatch.\n";
            return false;
        }

        // Mean
        std::size_t mean_n = 0;
        ar(cereal::make_nvp("mean_n", mean_n));
        std::vector<double> mean_data;
        ar(cereal::make_nvp("mean", mean_data));
        if (mean_data.size() != mean_n) {
            std::cerr << "PCAOperation::load: mean data size mismatch.\n";
            return false;
        }

        // Stddev
        arma::vec stddev;
        if (scaled) {
            std::size_t std_n = 0;
            ar(cereal::make_nvp("stddev_n", std_n));
            std::vector<double> std_data;
            ar(cereal::make_nvp("stddev", std_data));
            if (std_data.size() != std_n) {
                std::cerr << "PCAOperation::load: stddev data size mismatch.\n";
                return false;
            }
            stddev = arma::vec(std_data.data(), std_n);
        }

        // Commit
        _impl->eigenvectors = arma::mat(ev_data.data(), ev_rows, ev_cols);
        _impl->eigenvalues = arma::vec(val_data.data(), val_n);
        _impl->mean = arma::vec(mean_data.data(), mean_n);
        _impl->stddev = stddev;
        _impl->n_components = n_components;
        _impl->n_features = n_features;
        _impl->scaled = scaled;
        _impl->total_variance = total_variance;
        _impl->trained = true;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "PCAOperation::load failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

}// namespace MLCore
