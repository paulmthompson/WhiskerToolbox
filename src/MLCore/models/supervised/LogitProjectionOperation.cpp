/**
 * @file LogitProjectionOperation.cpp
 * @brief Supervised dimensionality reduction via softmax logit extraction
 */

#include "LogitProjectionOperation.hpp"

#include "mlpack.hpp"

#include <algorithm>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <iostream>
#include <sstream>

namespace MLCore {

// ============================================================================
// Pimpl — hides mlpack types and extracted weight matrices from header
// ============================================================================

using SRModel = mlpack::SoftmaxRegression<arma::mat>;

struct LogitProjectionOperation::Impl {
    /// Trained softmax regression model (used to extract weights)
    std::unique_ptr<SRModel> model;

    /// Extracted weight matrix W: (C × D)
    /// Logits for new data X: result = W * X + bias_broadcast
    arma::mat weight_matrix;

    /// Bias vector b: (C × 1)
    arma::vec bias;

    std::size_t num_classes = 0;
    std::size_t num_features = 0;
    std::vector<std::string> col_names;///< "Logit:0", "Logit:1", ...
    bool trained = false;
};

// ============================================================================
// Construction / destruction / move
// ============================================================================

LogitProjectionOperation::LogitProjectionOperation()
    : _impl(std::make_unique<Impl>()) {
}

LogitProjectionOperation::~LogitProjectionOperation() = default;

LogitProjectionOperation::LogitProjectionOperation(LogitProjectionOperation &&) noexcept = default;
LogitProjectionOperation & LogitProjectionOperation::operator=(LogitProjectionOperation &&) noexcept = default;

// ============================================================================
// Identity & metadata
// ============================================================================

std::string LogitProjectionOperation::getName() const {
    return "Logit Projection";
}

MLTaskType LogitProjectionOperation::getTaskType() const {
    return MLTaskType::SupervisedDimensionalityReduction;
}

std::unique_ptr<MLModelParametersBase> LogitProjectionOperation::getDefaultParameters() const {
    return std::make_unique<LogitProjectionParameters>();
}

// ============================================================================
// FitTransform
// ============================================================================

bool LogitProjectionOperation::fitTransform(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        MLModelParametersBase const * params,
        arma::mat & result) {
    // Validate inputs
    if (features.empty()) {
        std::cerr << "LogitProjectionOperation::fitTransform: Feature matrix is empty.\n";
        return false;
    }
    if (labels.empty()) {
        std::cerr << "LogitProjectionOperation::fitTransform: Labels are empty.\n";
        return false;
    }
    if (features.n_cols != labels.n_elem) {
        std::cerr << "LogitProjectionOperation::fitTransform: Feature columns ("
                  << features.n_cols << ") != label count ("
                  << labels.n_elem << ").\n";
        return false;
    }

    // Detect number of classes
    auto const max_label = arma::max(labels);
    std::size_t const num_classes = max_label + 1;

    if (num_classes < 2) {
        std::cerr << "LogitProjectionOperation::fitTransform: Need at least 2 classes, "
                  << "found " << num_classes << ".\n";
        return false;
    }

    // Extract parameters
    auto const * lp_params = dynamic_cast<LogitProjectionParameters const *>(params);
    LogitProjectionParameters const defaults;
    if (!lp_params) {
        lp_params = &defaults;
    }

    double const lambda = std::max(lp_params->lambda, 0.0);
    std::size_t const max_iterations = lp_params->max_iterations;

    try {
        // Train softmax regression with fitIntercept=true for best expressiveness
        // Parameter layout with fitIntercept=true:
        //   Parameters() is (C, D+1): col 0 = bias, cols 1..D = weights
        ens::L_BFGS optimizer;
        optimizer.MaxIterations() = max_iterations;

        _impl->model = std::make_unique<SRModel>(
                features,
                labels,
                num_classes,
                optimizer,
                lambda,
                true);// fitIntercept = true

        std::size_t const D = features.n_rows;

        // Extract weight matrix and bias
        // Parameters() shape: (C, D+1) — col 0 is bias, cols 1..D are weights
        arma::mat const & params_mat = _impl->model->Parameters();
        _impl->bias = params_mat.col(0);             // (C,)
        _impl->weight_matrix = params_mat.cols(1, D);// (C, D)

        _impl->num_classes = num_classes;
        _impl->num_features = D;
        _impl->trained = true;

        // Generate default column names
        _impl->col_names.resize(num_classes);
        for (std::size_t c = 0; c < num_classes; ++c) {
            _impl->col_names[c] = "Logit:" + std::to_string(c);
        }

        // Compute output for the training data
        result = _impl->weight_matrix * features;
        result.each_col() += _impl->bias;

        return true;

    } catch (std::exception const & e) {
        std::cerr << "LogitProjectionOperation::fitTransform failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// Transform
// ============================================================================

bool LogitProjectionOperation::transform(
        arma::mat const & features,
        arma::mat & result) {
    if (!_impl->trained) {
        std::cerr << "LogitProjectionOperation::transform: Model not trained.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "LogitProjectionOperation::transform: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->num_features) {
        std::cerr << "LogitProjectionOperation::transform: Feature dimension mismatch — "
                  << "expected " << _impl->num_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        result = _impl->weight_matrix * features;
        result.each_col() += _impl->bias;
        return true;
    } catch (std::exception const & e) {
        std::cerr << "LogitProjectionOperation::transform failed: " << e.what() << "\n";
        return false;
    }
}

// ============================================================================
// Query / introspection
// ============================================================================

std::size_t LogitProjectionOperation::outputDimensions() const {
    return _impl->trained ? _impl->num_classes : 0;
}

std::vector<std::string> LogitProjectionOperation::outputColumnNames() const {
    return _impl->col_names;
}

bool LogitProjectionOperation::isTrained() const {
    return _impl->trained;
}

std::size_t LogitProjectionOperation::numClasses() const {
    return _impl->trained ? _impl->num_classes : 0;
}

std::size_t LogitProjectionOperation::numFeatures() const {
    return _impl->trained ? _impl->num_features : 0;
}

// ============================================================================
// Serialization
// ============================================================================

bool LogitProjectionOperation::save(std::ostream & out) const {
    if (!_impl->trained) {
        return false;
    }
    try {
        cereal::BinaryOutputArchive ar(out);
        ar(cereal::make_nvp("num_classes", _impl->num_classes));
        ar(cereal::make_nvp("num_features", _impl->num_features));
        ar(cereal::make_nvp("col_names", _impl->col_names));
        ar(cereal::make_nvp("weight_matrix", _impl->weight_matrix));
        ar(cereal::make_nvp("bias", _impl->bias));
        return out.good();
    } catch (std::exception const & e) {
        std::cerr << "LogitProjectionOperation::save failed: " << e.what() << "\n";
        return false;
    }
}

bool LogitProjectionOperation::load(std::istream & in) {
    try {
        cereal::BinaryInputArchive ar(in);
        std::size_t num_classes = 0;
        std::size_t num_features = 0;
        std::vector<std::string> col_names;
        arma::mat weight_matrix;
        arma::vec bias;

        ar(cereal::make_nvp("num_classes", num_classes));
        ar(cereal::make_nvp("num_features", num_features));
        ar(cereal::make_nvp("col_names", col_names));
        ar(cereal::make_nvp("weight_matrix", weight_matrix));
        ar(cereal::make_nvp("bias", bias));

        _impl->num_classes = num_classes;
        _impl->num_features = num_features;
        _impl->col_names = std::move(col_names);
        _impl->weight_matrix = std::move(weight_matrix);
        _impl->bias = std::move(bias);
        _impl->model.reset();// no mlpack model after load — transform() uses W and b directly
        _impl->trained = true;
        return true;
    } catch (std::exception const & e) {
        std::cerr << "LogitProjectionOperation::load failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

}// namespace MLCore
