/**
 * @file ICAOperation.cpp
 * @brief Implementation of ICA via mlpack's RADICAL algorithm
 */

#include "ICAOperation.hpp"

#include "mlpack.hpp"

#include <cassert>
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <iostream>
#include <sstream>

namespace MLCore {

struct ICAOperation::Impl {
    arma::mat unmixing_matrix;///< Square unmixing matrix W (features × features)
    std::size_t n_features = 0;
    bool trained = false;
};

ICAOperation::ICAOperation()
    : _impl(std::make_unique<Impl>()) {
}

ICAOperation::~ICAOperation() = default;

ICAOperation::ICAOperation(ICAOperation &&) noexcept = default;
ICAOperation & ICAOperation::operator=(ICAOperation &&) noexcept = default;

std::string ICAOperation::getName() const {
    return "ICA";
}

std::unique_ptr<MLModelParametersBase> ICAOperation::getDefaultParameters() const {
    return std::make_unique<ICAParameters>();
}

bool ICAOperation::isTrained() const {
    return _impl->trained;
}

std::size_t ICAOperation::numFeatures() const {
    return _impl->trained ? _impl->n_features : 0;
}

bool ICAOperation::fitTransform(
        arma::mat const & features,
        MLModelParametersBase const * params,
        arma::mat & result) {
    if (features.empty()) {
        std::cerr << "ICAOperation::fitTransform: Feature matrix is empty.\n";
        return false;
    }

    if (features.n_cols < 2) {
        std::cerr << "ICAOperation::fitTransform: Need at least 2 observations.\n";
        return false;
    }

    auto const * ica_params = dynamic_cast<ICAParameters const *>(params);
    ICAParameters const defaults;
    if (!ica_params) {
        ica_params = &defaults;
    }

    try {
        mlpack::Radical radical(
                ica_params->noise_std_dev,
                ica_params->replicates,
                ica_params->angles,
                ica_params->sweeps,
                ica_params->m);

        arma::mat unmixing;
        arma::mat independent;
        radical.Apply(features, independent, unmixing);

        _impl->unmixing_matrix = std::move(unmixing);
        _impl->n_features = features.n_rows;
        _impl->trained = true;

        result = std::move(independent);
        return true;

    } catch (std::exception const & e) {
        std::cerr << "ICAOperation::fitTransform failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

bool ICAOperation::transform(
        arma::mat const & features,
        arma::mat & result) {
    if (!_impl->trained) {
        std::cerr << "ICAOperation::transform: Model not fitted.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "ICAOperation::transform: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->n_features) {
        std::cerr << "ICAOperation::transform: Feature dimension mismatch — "
                  << "expected " << _impl->n_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        result = _impl->unmixing_matrix * features;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "ICAOperation::transform failed: " << e.what() << "\n";
        return false;
    }
}

std::size_t ICAOperation::outputDimensions() const {
    return _impl->trained ? _impl->n_features : 0;
}

std::vector<double> ICAOperation::explainedVarianceRatio() const {
    // Explained variance is not defined for ICA
    return {};
}

bool ICAOperation::save(std::ostream & out) const {
    if (!_impl->trained) {
        return false;
    }

    try {
        cereal::BinaryOutputArchive ar(out);

        ar(cereal::make_nvp("n_features", _impl->n_features));

        // Unmixing matrix
        auto const w_rows = _impl->unmixing_matrix.n_rows;
        auto const w_cols = _impl->unmixing_matrix.n_cols;
        ar(cereal::make_nvp("w_rows", w_rows));
        ar(cereal::make_nvp("w_cols", w_cols));
        std::vector<double> w_data(
                _impl->unmixing_matrix.memptr(),
                _impl->unmixing_matrix.memptr() + w_rows * w_cols);
        ar(cereal::make_nvp("unmixing_matrix", w_data));

        return out.good();

    } catch (std::exception const & e) {
        std::cerr << "ICAOperation::save failed: " << e.what() << "\n";
        return false;
    }
}

bool ICAOperation::load(std::istream & in) {
    try {
        cereal::BinaryInputArchive ar(in);

        std::size_t n_features = 0;
        ar(cereal::make_nvp("n_features", n_features));

        std::size_t w_rows = 0;
        std::size_t w_cols = 0;
        ar(cereal::make_nvp("w_rows", w_rows));
        ar(cereal::make_nvp("w_cols", w_cols));
        std::vector<double> w_data;
        ar(cereal::make_nvp("unmixing_matrix", w_data));
        if (w_data.size() != w_rows * w_cols) {
            std::cerr << "ICAOperation::load: unmixing matrix data size mismatch.\n";
            return false;
        }

        _impl->unmixing_matrix = arma::mat(w_data.data(), w_rows, w_cols);
        _impl->n_features = n_features;
        _impl->trained = true;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "ICAOperation::load failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

}// namespace MLCore
