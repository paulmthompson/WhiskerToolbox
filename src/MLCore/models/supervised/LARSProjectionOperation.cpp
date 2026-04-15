/**
 * @file LARSProjectionOperation.cpp
 * @brief Implementation of LARS-based supervised feature selection
 *
 * Trains LARS regression models against class labels and identifies
 * which input features have non-zero coefficients (the "active set").
 * The output is the subset of input features that survived
 * regularization — i.e. the features LASSO considers predictive.
 *
 * Binary (C=2): fits a single LARS model.
 * Multi-class (C≥3): fits one-vs-all LARS models and takes the union
 * of active feature sets.
 *
 * Supports LASSO, Ridge, and Elastic Net regularization modes.
 */

#include "LARSProjectionOperation.hpp"

#include "mlpack.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <iostream>
#include <set>

namespace MLCore {

// ============================================================================
// Pimpl
// ============================================================================

struct LARSProjectionOperation::Impl {
    std::size_t num_classes = 0;
    std::size_t num_features = 0;
    bool trained = false;
    std::vector<std::size_t> active_features;///< Sorted indices of non-zero-coefficient features
    std::vector<std::string> col_names;      ///< "LARS_sel:0", "LARS_sel:1", ...
};

// ============================================================================
// Construction / destruction / move
// ============================================================================

LARSProjectionOperation::LARSProjectionOperation()
    : _impl(std::make_unique<Impl>()) {
}

LARSProjectionOperation::~LARSProjectionOperation() = default;

LARSProjectionOperation::LARSProjectionOperation(LARSProjectionOperation &&) noexcept = default;
LARSProjectionOperation & LARSProjectionOperation::operator=(LARSProjectionOperation &&) noexcept = default;

// ============================================================================
// Identity & metadata
// ============================================================================

std::string LARSProjectionOperation::getName() const {
    return "LARS Projection";
}

MLTaskType LARSProjectionOperation::getTaskType() const {
    return MLTaskType::SupervisedDimensionalityReduction;
}

std::unique_ptr<MLModelParametersBase> LARSProjectionOperation::getDefaultParameters() const {
    return std::make_unique<LARSProjectionParameters>();
}

// ============================================================================
// Helper: resolve effective lambda1/lambda2 from RegularizationType
// ============================================================================

namespace {

struct EffectiveLambdas {
    double lambda1;
    double lambda2;
};

EffectiveLambdas resolveRegularization(LARSProjectionParameters const & p) {
    switch (p.regularization_type) {
        case RegularizationType::LASSO:
            return {std::max(p.lambda1, 0.0), 0.0};
        case RegularizationType::Ridge:
            return {0.0, std::max(p.lambda2, 0.0)};
        case RegularizationType::ElasticNet:
            return {std::max(p.lambda1, 0.0), std::max(p.lambda2, 0.0)};
    }
    // Unreachable, but satisfy compiler
    return {std::max(p.lambda1, 0.0), std::max(p.lambda2, 0.0)};
}

}// anonymous namespace

// ============================================================================
// fitTransform
// ============================================================================

bool LARSProjectionOperation::fitTransform(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        MLModelParametersBase const * params,
        arma::mat & result) {

    // ------------------------------------------------------------------
    // Validate inputs
    // ------------------------------------------------------------------
    if (features.empty()) {
        std::cerr << "LARSProjectionOperation::fitTransform: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_cols < 2) {
        std::cerr << "LARSProjectionOperation::fitTransform: Need at least 2 observations.\n";
        return false;
    }
    if (labels.empty()) {
        std::cerr << "LARSProjectionOperation::fitTransform: Labels are empty.\n";
        return false;
    }
    if (features.n_cols != labels.n_elem) {
        std::cerr << "LARSProjectionOperation::fitTransform: Feature columns ("
                  << features.n_cols << ") != label count ("
                  << labels.n_elem << ").\n";
        return false;
    }

    // Detect number of classes
    auto const max_label = arma::max(labels);
    std::size_t const num_classes = max_label + 1;
    if (num_classes < 2) {
        std::cerr << "LARSProjectionOperation::fitTransform: Need at least 2 classes, "
                  << "found " << num_classes << ".\n";
        return false;
    }

    // Extract parameters
    auto const * lars_params = dynamic_cast<LARSProjectionParameters const *>(params);
    LARSProjectionParameters const defaults;
    if (!lars_params) {
        lars_params = &defaults;
    }

    auto const [eff_lambda1, eff_lambda2] = resolveRegularization(*lars_params);
    bool const use_cholesky = lars_params->use_cholesky;
    double const tolerance = lars_params->tolerance;

    std::size_t const D = features.n_rows;
    std::size_t const N = features.n_cols;

    // Scale lambda by N to match the standard 1/(2N) LASSO convention.
    // mlpack minimizes:  (1/2)||y - Xb||^2 + lambda||b||_1
    // Standard (sklearn/glmnet) minimizes: (1/2N)||y - Xb||^2 + lambda||b||_1
    // These are equivalent when lambda_mlpack = lambda_user * N.
    double const scaled_lambda1 = eff_lambda1 * static_cast<double>(N);
    double const scaled_lambda2 = eff_lambda2 * static_cast<double>(N);

    spdlog::debug("[LARS] fitTransform: {} features x {} observations, {} classes",
                  D, N, num_classes);
    spdlog::debug("[LARS] Regularization: type={}, user_lambda1={:.6f}, user_lambda2={:.6f}, "
                  "scaled_lambda1={:.2f}, scaled_lambda2={:.2f}, tolerance={:.2e}, cholesky={}",
                  static_cast<int>(lars_params->regularization_type),
                  eff_lambda1, eff_lambda2,
                  scaled_lambda1, scaled_lambda2, tolerance, use_cholesky);

    // Log feature L2 norms to debug scale sensitivity
    {
        arma::vec feature_norms(D);
        for (arma::uword f = 0; f < D; ++f) {
            feature_norms(f) = arma::norm(features.row(f), 2);
        }
        spdlog::debug("[LARS] Feature L2 norms: min={:.4f}, max={:.4f}, mean={:.4f}, "
                      "std={:.4f}",
                      arma::min(feature_norms), arma::max(feature_norms),
                      arma::mean(feature_norms), arma::stddev(feature_norms, 0));
    }

    try {
        // ------------------------------------------------------------------
        // Train LARS and collect active (non-zero coefficient) features
        // ------------------------------------------------------------------
        std::set<std::size_t> active_set;

        if (num_classes == 2) {
            // Binary: fit a single model (response = 1 for class 1, 0 for class 0)
            arma::rowvec responses(N, arma::fill::zeros);
            for (std::size_t i = 0; i < N; ++i) {
                if (labels(i) == 1) {
                    responses(i) = 1.0;
                }
            }

            // Log response balance to detect class imbalance
            {
                double const n_pos = arma::accu(responses);
                double const n_neg = static_cast<double>(N) - n_pos;
                double const pos_pct = 100.0 * n_pos / static_cast<double>(N);
                spdlog::debug("[LARS] Binary response: {} positive ({:.1f}%), {} negative ({:.1f}%)",
                              static_cast<std::size_t>(n_pos), pos_pct,
                              static_cast<std::size_t>(n_neg), 100.0 - pos_pct);
                spdlog::debug("[LARS] Response mean={:.6f}, std={:.6f}",
                              arma::mean(responses), arma::stddev(responses, 0));
            }

            mlpack::LARS lars(use_cholesky, scaled_lambda1, scaled_lambda2, tolerance,
                              true,  // fitIntercept
                              false);// normalizeData - we do this externally
            lars.Train(features, responses, true);

            spdlog::debug("[LARS] Binary model: {} active features out of {}",
                          lars.ActiveSet().size(), D);

            for (auto idx: lars.ActiveSet()) {
                active_set.insert(idx);
            }
        } else {
            // Multi-class: one-vs-all, take union of active sets
            for (std::size_t c = 0; c < num_classes; ++c) {
                arma::rowvec responses(N, arma::fill::zeros);
                for (std::size_t i = 0; i < N; ++i) {
                    if (labels(i) == c) {
                        responses(i) = 1.0;
                    }
                }

                mlpack::LARS lars(use_cholesky, scaled_lambda1, scaled_lambda2, tolerance,
                                  true,  // fitIntercept
                                  false);// normalizeData - we do this externally
                lars.Train(features, responses, true);

                spdlog::debug("[LARS] OvA class {}: {} active features", c, lars.ActiveSet().size());

                for (auto idx: lars.ActiveSet()) {
                    active_set.insert(idx);
                }
            }
        }

        if (active_set.empty()) {
            std::cerr << "LARSProjectionOperation::fitTransform: All coefficients were "
                      << "shrunk to zero. Try reducing lambda.\n";
            spdlog::warn("[LARS] All coefficients shrunk to zero. lambda1={:.6f}, lambda2={:.6f}",
                         eff_lambda1, eff_lambda2);
            return false;
        }

        spdlog::debug("[LARS] Active set: {} / {} features selected (sparsity: {:.1f}%)",
                      active_set.size(), D,
                      100.0 * (1.0 - static_cast<double>(active_set.size()) / static_cast<double>(D)));

        // ------------------------------------------------------------------
        // Store fitted state
        // ------------------------------------------------------------------
        _impl->num_classes = num_classes;
        _impl->num_features = D;
        _impl->active_features.assign(active_set.begin(), active_set.end());

        // Generate column names reflecting selected feature indices
        _impl->col_names.resize(_impl->active_features.size());
        for (std::size_t i = 0; i < _impl->active_features.size(); ++i) {
            _impl->col_names[i] =
                    "LARS_sel:" + std::to_string(_impl->active_features[i]);
        }

        _impl->trained = true;

        // ------------------------------------------------------------------
        // Output: subset of input features at selected indices
        // ------------------------------------------------------------------
        arma::uvec idx_vec(_impl->active_features.size());
        for (std::size_t i = 0; i < _impl->active_features.size(); ++i) {
            idx_vec(i) = _impl->active_features[i];
        }
        result = features.rows(idx_vec);

        return true;

    } catch (std::exception const & e) {
        std::cerr << "LARSProjectionOperation::fitTransform failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// transform
// ============================================================================

bool LARSProjectionOperation::transform(
        arma::mat const & features,
        arma::mat & result) {
    if (!_impl->trained) {
        std::cerr << "LARSProjectionOperation::transform: Model not fitted.\n";
        return false;
    }
    if (features.empty()) {
        std::cerr << "LARSProjectionOperation::transform: Feature matrix is empty.\n";
        return false;
    }
    if (features.n_rows != _impl->num_features) {
        std::cerr << "LARSProjectionOperation::transform: Feature dimension mismatch — "
                  << "expected " << _impl->num_features
                  << ", got " << features.n_rows << ".\n";
        return false;
    }

    try {
        arma::uvec idx_vec(_impl->active_features.size());
        for (std::size_t i = 0; i < _impl->active_features.size(); ++i) {
            idx_vec(i) = _impl->active_features[i];
        }
        result = features.rows(idx_vec);
        return true;
    } catch (std::exception const & e) {
        std::cerr << "LARSProjectionOperation::transform failed: " << e.what() << "\n";
        return false;
    }
}

// ============================================================================
// Query / introspection
// ============================================================================

std::size_t LARSProjectionOperation::outputDimensions() const {
    return _impl->trained ? _impl->active_features.size() : 0;
}

std::vector<std::string> LARSProjectionOperation::outputColumnNames() const {
    return _impl->col_names;
}

bool LARSProjectionOperation::isTrained() const {
    return _impl->trained;
}

std::size_t LARSProjectionOperation::numClasses() const {
    return _impl->trained ? _impl->num_classes : 0;
}

std::size_t LARSProjectionOperation::numFeatures() const {
    return _impl->trained ? _impl->num_features : 0;
}

std::vector<std::size_t> LARSProjectionOperation::activeFeatureIndices() const {
    if (!_impl->trained) {
        return {};
    }
    return _impl->active_features;
}

// ============================================================================
// Serialization
// ============================================================================

bool LARSProjectionOperation::save(std::ostream & out) const {
    if (!_impl->trained) {
        return false;
    }
    try {
        cereal::BinaryOutputArchive ar(out);

        ar(cereal::make_nvp("num_classes", _impl->num_classes));
        ar(cereal::make_nvp("num_features", _impl->num_features));
        ar(cereal::make_nvp("col_names", _impl->col_names));
        ar(cereal::make_nvp("active_features", _impl->active_features));

        return out.good();

    } catch (std::exception const & e) {
        std::cerr << "LARSProjectionOperation::save failed: " << e.what() << "\n";
        return false;
    }
}

bool LARSProjectionOperation::load(std::istream & in) {
    try {
        cereal::BinaryInputArchive ar(in);

        std::size_t num_classes = 0;
        std::size_t num_features = 0;
        std::vector<std::string> col_names;
        std::vector<std::size_t> active_features;

        ar(cereal::make_nvp("num_classes", num_classes));
        ar(cereal::make_nvp("num_features", num_features));
        ar(cereal::make_nvp("col_names", col_names));
        ar(cereal::make_nvp("active_features", active_features));

        // Commit all state atomically
        _impl->num_classes = num_classes;
        _impl->num_features = num_features;
        _impl->col_names = std::move(col_names);
        _impl->active_features = std::move(active_features);
        _impl->trained = true;
        return true;

    } catch (std::exception const & e) {
        std::cerr << "LARSProjectionOperation::load failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

}// namespace MLCore
