/**
 * @file SoftmaxRegressionOperation.cpp
 * @brief Implementation of multi-class softmax regression via mlpack
 */

#include "SoftmaxRegressionOperation.hpp"

#include "mlpack.hpp"

#include <algorithm>
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <iostream>
#include <sstream>

namespace MLCore {

// ============================================================================
// Pimpl — hides mlpack types from header
// ============================================================================

using SRModel = mlpack::SoftmaxRegression<arma::mat>;

struct SoftmaxRegressionOperation::Impl {
    std::unique_ptr<SRModel> model;
    std::size_t num_classes = 0;
    std::size_t num_features = 0;
    bool trained = false;
};

// ============================================================================
// Construction / destruction / move
// ============================================================================

SoftmaxRegressionOperation::SoftmaxRegressionOperation()
    : _impl(std::make_unique<Impl>()) {
}

SoftmaxRegressionOperation::~SoftmaxRegressionOperation() = default;

SoftmaxRegressionOperation::SoftmaxRegressionOperation(SoftmaxRegressionOperation &&) noexcept = default;
SoftmaxRegressionOperation & SoftmaxRegressionOperation::operator=(SoftmaxRegressionOperation &&) noexcept = default;

// ============================================================================
// Identity & metadata
// ============================================================================

std::string SoftmaxRegressionOperation::getName() const {
    return "Softmax Regression";
}

MLTaskType SoftmaxRegressionOperation::getTaskType() const {
    return MLTaskType::MultiClassClassification;
}

std::unique_ptr<MLModelParametersBase> SoftmaxRegressionOperation::getDefaultParameters() const {
    return std::make_unique<SoftmaxRegressionParameters>();
}

// ============================================================================
// Training
// ============================================================================

bool SoftmaxRegressionOperation::train(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        MLModelParametersBase const * params) {
    // Validate inputs
    if (features.empty()) {
        std::cerr << "SoftmaxRegressionOperation::train: Feature matrix is empty.\n";
        return false;
    }
    if (labels.empty()) {
        std::cerr << "SoftmaxRegressionOperation::train: Labels are empty.\n";
        return false;
    }
    if (features.n_cols != labels.n_elem) {
        std::cerr << "SoftmaxRegressionOperation::train: Feature columns ("
                  << features.n_cols << ") != label count ("
                  << labels.n_elem << ").\n";
        return false;
    }

    // Determine number of classes from labels
    auto const max_label = arma::max(labels);
    std::size_t const num_classes = max_label + 1;

    if (num_classes < 2) {
        std::cerr << "SoftmaxRegressionOperation::train: Need at least 2 classes, "
                  << "found " << num_classes << ".\n";
        return false;
    }

    // Extract parameters (use defaults if nullptr or wrong type)
    auto const * sr_params = dynamic_cast<SoftmaxRegressionParameters const *>(params);
    SoftmaxRegressionParameters const defaults;
    if (!sr_params) {
        sr_params = &defaults;
    }

    double const lambda = std::max(sr_params->lambda, 0.0);
    std::size_t const max_iterations = sr_params->max_iterations;

    try {
        // Configure L-BFGS optimizer with user-specified max iterations
        ens::L_BFGS optimizer;
        optimizer.MaxIterations() = max_iterations;

        // mlpack::SoftmaxRegression trains on construction with optimizer
        _impl->model = std::make_unique<SRModel>(
                features,
                labels,
                num_classes,
                optimizer,
                lambda,
                false);// fitIntercept

        _impl->num_classes = num_classes;
        _impl->num_features = features.n_rows;
        _impl->trained = true;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "SoftmaxRegressionOperation::train failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// Prediction
// ============================================================================

bool SoftmaxRegressionOperation::predict(
        arma::mat const & features,
        arma::Row<std::size_t> & predictions) {
    if (!_impl->trained || !_impl->model) {
        std::cerr << "SoftmaxRegressionOperation::predict: Model not trained.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "SoftmaxRegressionOperation::predict: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->num_features) {
        std::cerr << "SoftmaxRegressionOperation::predict: Feature dimension mismatch — "
                  << "expected " << _impl->num_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        _impl->model->Classify(features, predictions);
        return true;
    } catch (std::exception const & e) {
        std::cerr << "SoftmaxRegressionOperation::predict failed: " << e.what() << "\n";
        return false;
    }
}

bool SoftmaxRegressionOperation::predictProbabilities(
        arma::mat const & features,
        arma::mat & probabilities) {
    if (!_impl->trained || !_impl->model) {
        std::cerr << "SoftmaxRegressionOperation::predictProbabilities: Model not trained.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "SoftmaxRegressionOperation::predictProbabilities: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->num_features) {
        std::cerr << "SoftmaxRegressionOperation::predictProbabilities: Feature dimension mismatch — "
                  << "expected " << _impl->num_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        arma::Row<std::size_t> predictions;
        _impl->model->Classify(features, predictions, probabilities);
        return true;
    } catch (std::exception const & e) {
        std::cerr << "SoftmaxRegressionOperation::predictProbabilities failed: "
                  << e.what() << "\n";
        return false;
    }
}

// ============================================================================
// Serialization
// ============================================================================

bool SoftmaxRegressionOperation::save(std::ostream & out) const {
    if (!_impl->trained || !_impl->model) {
        return false;
    }

    try {
        cereal::BinaryOutputArchive ar(out);
        ar(cereal::make_nvp("num_classes", _impl->num_classes));
        ar(cereal::make_nvp("num_features", _impl->num_features));
        // Serialize the mlpack model using mlpack's built-in cereal support
        ar(cereal::make_nvp("model", *_impl->model));
        return out.good();
    } catch (std::exception const & e) {
        std::cerr << "SoftmaxRegressionOperation::save failed: " << e.what() << "\n";
        return false;
    }
}

bool SoftmaxRegressionOperation::load(std::istream & in) {
    try {
        cereal::BinaryInputArchive ar(in);

        std::size_t num_classes = 0;
        std::size_t num_features = 0;
        ar(cereal::make_nvp("num_classes", num_classes));
        ar(cereal::make_nvp("num_features", num_features));

        auto model = std::make_unique<SRModel>();
        ar(cereal::make_nvp("model", *model));

        _impl->model = std::move(model);
        _impl->num_classes = num_classes;
        _impl->num_features = num_features;
        _impl->trained = true;
        return true;
    } catch (std::exception const & e) {
        std::cerr << "SoftmaxRegressionOperation::load failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// Query / introspection
// ============================================================================

bool SoftmaxRegressionOperation::isTrained() const {
    return _impl->trained;
}

std::size_t SoftmaxRegressionOperation::numClasses() const {
    return _impl->trained ? _impl->num_classes : 0;
}

std::size_t SoftmaxRegressionOperation::numFeatures() const {
    return _impl->trained ? _impl->num_features : 0;
}

}// namespace MLCore
