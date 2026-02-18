#include "LogisticRegressionOperation.hpp"

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

using LRModel = mlpack::LogisticRegression<arma::mat>;

struct LogisticRegressionOperation::Impl {
    std::unique_ptr<LRModel> model;
    std::size_t num_classes = 0;
    std::size_t num_features = 0;
    bool trained = false;
};

// ============================================================================
// Construction / destruction / move
// ============================================================================

LogisticRegressionOperation::LogisticRegressionOperation()
    : _impl(std::make_unique<Impl>())
{
}

LogisticRegressionOperation::~LogisticRegressionOperation() = default;

LogisticRegressionOperation::LogisticRegressionOperation(LogisticRegressionOperation &&) noexcept = default;
LogisticRegressionOperation & LogisticRegressionOperation::operator=(LogisticRegressionOperation &&) noexcept = default;

// ============================================================================
// Identity & metadata
// ============================================================================

std::string LogisticRegressionOperation::getName() const
{
    return "Logistic Regression";
}

MLTaskType LogisticRegressionOperation::getTaskType() const
{
    return MLTaskType::BinaryClassification;
}

std::unique_ptr<MLModelParametersBase> LogisticRegressionOperation::getDefaultParameters() const
{
    return std::make_unique<LogisticRegressionParameters>();
}

// ============================================================================
// Training
// ============================================================================

bool LogisticRegressionOperation::train(
    arma::mat const & features,
    arma::Row<std::size_t> const & labels,
    MLModelParametersBase const * params)
{
    // Validate inputs
    if (features.empty()) {
        std::cerr << "LogisticRegressionOperation::train: Feature matrix is empty.\n";
        return false;
    }
    if (labels.empty()) {
        std::cerr << "LogisticRegressionOperation::train: Labels are empty.\n";
        return false;
    }
    if (features.n_cols != labels.n_elem) {
        std::cerr << "LogisticRegressionOperation::train: Feature columns ("
                  << features.n_cols << ") != label count ("
                  << labels.n_elem << ").\n";
        return false;
    }

    // mlpack LogisticRegression is binary-only: labels must be 0 or 1
    auto const max_label = arma::max(labels);
    if (max_label > 1) {
        std::cerr << "LogisticRegressionOperation::train: Labels contain values > 1. "
                  << "Logistic Regression only supports binary classification (0/1).\n";
        return false;
    }

    // Extract parameters (use defaults if nullptr or wrong type)
    auto const * lr_params = dynamic_cast<LogisticRegressionParameters const *>(params);
    LogisticRegressionParameters defaults;
    if (!lr_params) {
        lr_params = &defaults;
    }

    double const lambda = std::max(lr_params->lambda, 0.0);
    std::size_t const max_iterations = lr_params->max_iterations;

    try {
        // Configure L-BFGS optimizer with user-specified max iterations
        ens::L_BFGS optimizer;
        optimizer.MaxIterations() = max_iterations;

        // mlpack::LogisticRegression trains on construction with optimizer
        _impl->model = std::make_unique<LRModel>(
            features,
            labels,
            optimizer,
            lambda);

        _impl->num_classes = 2; // LogisticRegression is always binary
        _impl->num_features = features.n_rows;
        _impl->trained = true;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "LogisticRegressionOperation::train failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// Prediction
// ============================================================================

bool LogisticRegressionOperation::predict(
    arma::mat const & features,
    arma::Row<std::size_t> & predictions)
{
    if (!_impl->trained || !_impl->model) {
        std::cerr << "LogisticRegressionOperation::predict: Model not trained.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "LogisticRegressionOperation::predict: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->num_features) {
        std::cerr << "LogisticRegressionOperation::predict: Feature dimension mismatch — "
                  << "expected " << _impl->num_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        _impl->model->Classify(features, predictions);
        return true;
    } catch (std::exception const & e) {
        std::cerr << "LogisticRegressionOperation::predict failed: " << e.what() << "\n";
        return false;
    }
}

bool LogisticRegressionOperation::predictProbabilities(
    arma::mat const & features,
    arma::mat & probabilities)
{
    if (!_impl->trained || !_impl->model) {
        std::cerr << "LogisticRegressionOperation::predictProbabilities: Model not trained.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "LogisticRegressionOperation::predictProbabilities: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->num_features) {
        std::cerr << "LogisticRegressionOperation::predictProbabilities: Feature dimension mismatch — "
                  << "expected " << _impl->num_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        arma::Row<std::size_t> predictions;
        _impl->model->Classify(features, predictions, probabilities);
        return true;
    } catch (std::exception const & e) {
        std::cerr << "LogisticRegressionOperation::predictProbabilities failed: " << e.what() << "\n";
        return false;
    }
}

// ============================================================================
// Serialization
// ============================================================================

bool LogisticRegressionOperation::save(std::ostream & out) const
{
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
        std::cerr << "LogisticRegressionOperation::save failed: " << e.what() << "\n";
        return false;
    }
}

bool LogisticRegressionOperation::load(std::istream & in)
{
    try {
        cereal::BinaryInputArchive ar(in);

        std::size_t num_classes = 0;
        std::size_t num_features = 0;
        ar(cereal::make_nvp("num_classes", num_classes));
        ar(cereal::make_nvp("num_features", num_features));

        auto model = std::make_unique<LRModel>();
        ar(cereal::make_nvp("model", *model));

        _impl->model = std::move(model);
        _impl->num_classes = num_classes;
        _impl->num_features = num_features;
        _impl->trained = true;
        return true;
    } catch (std::exception const & e) {
        std::cerr << "LogisticRegressionOperation::load failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// Query / introspection
// ============================================================================

bool LogisticRegressionOperation::isTrained() const
{
    return _impl->trained;
}

std::size_t LogisticRegressionOperation::numClasses() const
{
    return _impl->trained ? _impl->num_classes : 0;
}

std::size_t LogisticRegressionOperation::numFeatures() const
{
    return _impl->trained ? _impl->num_features : 0;
}

} // namespace MLCore
