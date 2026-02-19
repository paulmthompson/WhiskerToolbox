/**
 * @file RandomForestOperation.cpp
 * @brief Implementation of Random Forest classification via mlpack
 */

#include "RandomForestOperation.hpp"

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

using RFModel = mlpack::RandomForest<
    mlpack::GiniGain,
    mlpack::MultipleRandomDimensionSelect>;

struct RandomForestOperation::Impl {
    std::unique_ptr<RFModel> model;
    std::size_t num_classes = 0;
    std::size_t num_features = 0;
    bool trained = false;
};

// ============================================================================
// Construction / destruction / move
// ============================================================================

RandomForestOperation::RandomForestOperation()
    : _impl(std::make_unique<Impl>())
{
}

RandomForestOperation::~RandomForestOperation() = default;

RandomForestOperation::RandomForestOperation(RandomForestOperation &&) noexcept = default;
RandomForestOperation & RandomForestOperation::operator=(RandomForestOperation &&) noexcept = default;

// ============================================================================
// Identity & metadata
// ============================================================================

std::string RandomForestOperation::getName() const
{
    return "Random Forest";
}

MLTaskType RandomForestOperation::getTaskType() const
{
    return MLTaskType::MultiClassClassification;
}

std::unique_ptr<MLModelParametersBase> RandomForestOperation::getDefaultParameters() const
{
    return std::make_unique<RandomForestParameters>();
}

// ============================================================================
// Training
// ============================================================================

bool RandomForestOperation::train(
    arma::mat const & features,
    arma::Row<std::size_t> const & labels,
    MLModelParametersBase const * params)
{
    // Validate inputs
    if (features.empty()) {
        std::cerr << "RandomForestOperation::train: Feature matrix is empty.\n";
        return false;
    }
    if (labels.empty()) {
        std::cerr << "RandomForestOperation::train: Labels are empty.\n";
        return false;
    }
    if (features.n_cols != labels.n_elem) {
        std::cerr << "RandomForestOperation::train: Feature columns ("
                  << features.n_cols << ") != label count ("
                  << labels.n_elem << ").\n";
        return false;
    }

    // Extract parameters (use defaults if nullptr or wrong type)
    auto const * rf_params = dynamic_cast<RandomForestParameters const *>(params);
    RandomForestParameters defaults;
    if (!rf_params) {
        rf_params = &defaults;
    }

    // Determine number of classes from labels
    std::size_t const detected_classes = static_cast<std::size_t>(arma::max(labels)) + 1;
    std::size_t const num_classes = std::max(detected_classes, std::size_t{2});

    // Map parameters
    int const num_trees = std::max(rf_params->num_trees, 1);
    int const min_leaf = std::max(rf_params->minimum_leaf_size, 1);
    int const max_depth = std::max(rf_params->maximum_depth, 0);

    try {
        // mlpack::RandomForest trains on construction
        _impl->model = std::make_unique<RFModel>(
            features,
            labels,
            num_classes,
            num_trees,
            min_leaf,
            /* minimumGainSplit = */ rf_params->minimum_gain_split,
            max_depth);

        _impl->num_classes = num_classes;
        _impl->num_features = features.n_rows;
        _impl->trained = true;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "RandomForestOperation::train failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// Prediction
// ============================================================================

bool RandomForestOperation::predict(
    arma::mat const & features,
    arma::Row<std::size_t> & predictions)
{
    if (!_impl->trained || !_impl->model) {
        std::cerr << "RandomForestOperation::predict: Model not trained.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "RandomForestOperation::predict: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->num_features) {
        std::cerr << "RandomForestOperation::predict: Feature dimension mismatch — "
                  << "expected " << _impl->num_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        _impl->model->Classify(features, predictions);
        return true;
    } catch (std::exception const & e) {
        std::cerr << "RandomForestOperation::predict failed: " << e.what() << "\n";
        return false;
    }
}

bool RandomForestOperation::predictProbabilities(
    arma::mat const & features,
    arma::mat & probabilities)
{
    if (!_impl->trained || !_impl->model) {
        std::cerr << "RandomForestOperation::predictProbabilities: Model not trained.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "RandomForestOperation::predictProbabilities: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->num_features) {
        std::cerr << "RandomForestOperation::predictProbabilities: Feature dimension mismatch — "
                  << "expected " << _impl->num_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        arma::Row<std::size_t> predictions;
        _impl->model->Classify(features, predictions, probabilities);
        return true;
    } catch (std::exception const & e) {
        std::cerr << "RandomForestOperation::predictProbabilities failed: " << e.what() << "\n";
        return false;
    }
}

// ============================================================================
// Serialization
// ============================================================================

bool RandomForestOperation::save(std::ostream & out) const
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
        std::cerr << "RandomForestOperation::save failed: " << e.what() << "\n";
        return false;
    }
}

bool RandomForestOperation::load(std::istream & in)
{
    try {
        cereal::BinaryInputArchive ar(in);

        std::size_t num_classes = 0;
        std::size_t num_features = 0;
        ar(cereal::make_nvp("num_classes", num_classes));
        ar(cereal::make_nvp("num_features", num_features));

        auto model = std::make_unique<RFModel>();
        ar(cereal::make_nvp("model", *model));

        _impl->model = std::move(model);
        _impl->num_classes = num_classes;
        _impl->num_features = num_features;
        _impl->trained = true;
        return true;
    } catch (std::exception const & e) {
        std::cerr << "RandomForestOperation::load failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// Query / introspection
// ============================================================================

bool RandomForestOperation::isTrained() const
{
    return _impl->trained;
}

std::size_t RandomForestOperation::numClasses() const
{
    return _impl->trained ? _impl->num_classes : 0;
}

std::size_t RandomForestOperation::numFeatures() const
{
    return _impl->trained ? _impl->num_features : 0;
}

} // namespace MLCore
