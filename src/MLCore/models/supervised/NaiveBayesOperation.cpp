/**
 * @file NaiveBayesOperation.cpp
 * @brief Implementation of Naive Bayes classification via mlpack
 */

#include "NaiveBayesOperation.hpp"

#include "mlpack.hpp"

#include <algorithm>
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace MLCore {

// ============================================================================
// Pimpl — hides mlpack types from header
// ============================================================================

using NBModel = mlpack::NaiveBayesClassifier<>;

struct NaiveBayesOperation::Impl {
    std::unique_ptr<NBModel> model;
    std::size_t num_classes = 0;
    std::size_t num_features = 0;
    bool trained = false;
};

// ============================================================================
// Construction / destruction / move
// ============================================================================

NaiveBayesOperation::NaiveBayesOperation()
    : _impl(std::make_unique<Impl>()) {
}

NaiveBayesOperation::~NaiveBayesOperation() = default;

NaiveBayesOperation::NaiveBayesOperation(NaiveBayesOperation &&) noexcept = default;
NaiveBayesOperation & NaiveBayesOperation::operator=(NaiveBayesOperation &&) noexcept = default;

// ============================================================================
// Identity & metadata
// ============================================================================

std::string NaiveBayesOperation::getName() const {
    return "Naive Bayes";
}

MLTaskType NaiveBayesOperation::getTaskType() const {
    return MLTaskType::MultiClassClassification;
}

std::unique_ptr<MLModelParametersBase> NaiveBayesOperation::getDefaultParameters() const {
    return std::make_unique<NaiveBayesParameters>();
}

// ============================================================================
// Training
// ============================================================================

bool NaiveBayesOperation::train(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        MLModelParametersBase const * params) {
    // Validate inputs
    if (features.empty()) {
        std::cerr << "NaiveBayesOperation::train: Feature matrix is empty.\n";
        return false;
    }
    if (labels.empty()) {
        std::cerr << "NaiveBayesOperation::train: Labels are empty.\n";
        return false;
    }
    if (features.n_cols != labels.n_elem) {
        std::cerr << "NaiveBayesOperation::train: Feature columns ("
                  << features.n_cols << ") != label count ("
                  << labels.n_elem << ").\n";
        return false;
    }

    // Extract parameters (use defaults if nullptr or wrong type)
    auto const * nb_params = dynamic_cast<NaiveBayesParameters const *>(params);
    NaiveBayesParameters defaults;
    if (!nb_params) {
        nb_params = &defaults;
    }

    // Determine number of classes from labels
    std::size_t const detected_classes = static_cast<std::size_t>(arma::max(labels)) + 1;
    std::size_t const num_classes = std::max(detected_classes, std::size_t{2});

    try {
        // Add epsilon smoothing to the feature variances to avoid zero-variance issues.
        // We create a copy to apply smoothing without modifying the caller's data.
        arma::mat smoothed_features = features;
        if (nb_params->epsilon > 0.0) {
            // Add small noise to prevent degenerate Gaussian distributions
            // This is analogous to sklearn's var_smoothing parameter
            smoothed_features += nb_params->epsilon * arma::randn(arma::size(features));
        }

        // mlpack::NaiveBayesClassifier trains on construction
        _impl->model = std::make_unique<NBModel>(
                smoothed_features,
                labels,
                num_classes);

        _impl->num_classes = num_classes;
        _impl->num_features = features.n_rows;
        _impl->trained = true;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "NaiveBayesOperation::train failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// Prediction
// ============================================================================

bool NaiveBayesOperation::predict(
        arma::mat const & features,
        arma::Row<std::size_t> & predictions) {
    if (!_impl->trained || !_impl->model) {
        std::cerr << "NaiveBayesOperation::predict: Model not trained.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "NaiveBayesOperation::predict: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->num_features) {
        std::cerr << "NaiveBayesOperation::predict: Feature dimension mismatch — "
                  << "expected " << _impl->num_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        _impl->model->Classify(features, predictions);
        return true;
    } catch (std::exception const & e) {
        std::cerr << "NaiveBayesOperation::predict failed: " << e.what() << "\n";
        return false;
    }
}

bool NaiveBayesOperation::predictProbabilities(
        arma::mat const & features,
        arma::mat & probabilities) {
    if (!_impl->trained || !_impl->model) {
        std::cerr << "NaiveBayesOperation::predictProbabilities: Model not trained.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "NaiveBayesOperation::predictProbabilities: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->num_features) {
        std::cerr << "NaiveBayesOperation::predictProbabilities: Feature dimension mismatch — "
                  << "expected " << _impl->num_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        arma::Row<std::size_t> predictions;
        _impl->model->Classify(features, predictions, probabilities);
        return true;
    } catch (std::exception const & e) {
        std::cerr << "NaiveBayesOperation::predictProbabilities failed: " << e.what() << "\n";
        return false;
    }
}

// ============================================================================
// Serialization
// ============================================================================

bool NaiveBayesOperation::save(std::ostream & out) const {
    if (!_impl->trained || !_impl->model) {
        return false;
    }

    try {
        // Write metadata first
        cereal::BinaryOutputArchive ar(out);
        ar(cereal::make_nvp("num_classes", _impl->num_classes));
        ar(cereal::make_nvp("num_features", _impl->num_features));
        // Serialize the mlpack model using mlpack's built-in cereal support
        ar(cereal::make_nvp("model", *_impl->model));
        return out.good();
    } catch (std::exception const & e) {
        std::cerr << "NaiveBayesOperation::save failed: " << e.what() << "\n";
        return false;
    }
}

bool NaiveBayesOperation::load(std::istream & in) {
    try {
        cereal::BinaryInputArchive ar(in);

        std::size_t num_classes = 0;
        std::size_t num_features = 0;
        ar(cereal::make_nvp("num_classes", num_classes));
        ar(cereal::make_nvp("num_features", num_features));

        auto model = std::make_unique<NBModel>();
        ar(cereal::make_nvp("model", *model));

        _impl->model = std::move(model);
        _impl->num_classes = num_classes;
        _impl->num_features = num_features;
        _impl->trained = true;
        return true;
    } catch (std::exception const & e) {
        std::cerr << "NaiveBayesOperation::load failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// Query / introspection
// ============================================================================

bool NaiveBayesOperation::isTrained() const {
    return _impl->trained;
}

std::size_t NaiveBayesOperation::numClasses() const {
    return _impl->trained ? _impl->num_classes : 0;
}

std::size_t NaiveBayesOperation::numFeatures() const {
    return _impl->trained ? _impl->num_features : 0;
}

}// namespace MLCore
