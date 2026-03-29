/**
 * @file TSNEOperation.cpp
 * @brief Implementation of t-SNE dimensionality reduction via tapkee
 */

#include "TSNEOperation.hpp"

#include <tapkee/tapkee.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iostream>

namespace MLCore {

// ============================================================================
// Pimpl — hides Eigen / tapkee headers from consumers
// ============================================================================

struct TSNEOperation::Impl {
    std::size_t n_components = 0;
    std::size_t n_features = 0;
    bool trained = false;
};

// ============================================================================
// Special members
// ============================================================================

TSNEOperation::TSNEOperation()
    : _impl(std::make_unique<Impl>()) {
}

TSNEOperation::~TSNEOperation() = default;

TSNEOperation::TSNEOperation(TSNEOperation &&) noexcept = default;
TSNEOperation & TSNEOperation::operator=(TSNEOperation &&) noexcept = default;

// ============================================================================
// Identity
// ============================================================================

std::string TSNEOperation::getName() const {
    return "t-SNE";
}

std::unique_ptr<MLModelParametersBase> TSNEOperation::getDefaultParameters() const {
    return std::make_unique<TSNEParameters>();
}

bool TSNEOperation::isTrained() const {
    return _impl->trained;
}

std::size_t TSNEOperation::numFeatures() const {
    return _impl->trained ? _impl->n_features : 0;
}

// ============================================================================
// fitTransform — Armadillo → Eigen → tapkee → Eigen → Armadillo
// ============================================================================

bool TSNEOperation::fitTransform(
        arma::mat const & features,
        MLModelParametersBase const * params,
        arma::mat & result) {

    if (features.empty()) {
        std::cerr << "TSNEOperation::fitTransform: Feature matrix is empty.\n";
        return false;
    }

    auto const * tsne_params = dynamic_cast<TSNEParameters const *>(params);
    TSNEParameters const defaults;
    if (!tsne_params) {
        tsne_params = &defaults;
    }

    std::size_t const n_components = tsne_params->n_components;
    double const perplexity = tsne_params->perplexity;
    double const theta = tsne_params->theta;

    if (n_components == 0) {
        std::cerr << "TSNEOperation::fitTransform: n_components must be > 0.\n";
        return false;
    }
    // t-SNE needs at least a few points to form meaningful neighborhoods
    if (features.n_cols < 4) {
        std::cerr << "TSNEOperation::fitTransform: Need at least 4 observations, got "
                  << features.n_cols << ".\n";
        return false;
    }

    // Clamp perplexity to valid range: must be < n_obs / 3
    double effective_perplexity = perplexity;
    double const max_perplexity = static_cast<double>(features.n_cols) / 3.0;
    if (effective_perplexity >= max_perplexity) {
        effective_perplexity = std::max(1.0, max_perplexity - 1.0);
    }

    try {
        // ---- Convert Armadillo (col-major) → Eigen (col-major) ----
        // Both store column-major, so the memory layout is compatible.
        // features is (n_features × n_observations).
        auto const n_feat = static_cast<Eigen::Index>(features.n_rows);
        auto const n_obs = static_cast<Eigen::Index>(features.n_cols);

        Eigen::MatrixXd eigen_data(n_feat, n_obs);
        std::copy(features.memptr(),
                  features.memptr() + features.n_elem,
                  eigen_data.data());

        // ---- Run tapkee t-SNE ----
        // sne_theta > 0 engages Barnes-Hut approximation internally
        tapkee::TapkeeOutput output = tapkee::with((
                                                           tapkee::method = tapkee::tDistributedStochasticNeighborEmbedding,
                                                           tapkee::target_dimension = static_cast<tapkee::IndexType>(n_components),
                                                           tapkee::sne_perplexity = effective_perplexity,
                                                           tapkee::sne_theta = theta))
                                              .embedUsing(eigen_data);

        // ---- Convert Eigen embedding → Armadillo ----
        // output.embedding is (n_observations × n_components)
        auto const out_rows = output.embedding.rows();// n_observations
        auto const out_cols = output.embedding.cols();// n_components

        // We need (n_components × n_observations) to match mlpack convention
        Eigen::MatrixXd const transposed = output.embedding.transpose();

        result.set_size(static_cast<arma::uword>(out_cols),
                        static_cast<arma::uword>(out_rows));
        std::copy(transposed.data(),
                  transposed.data() + transposed.size(),
                  result.memptr());

        _impl->n_components = n_components;
        _impl->n_features = features.n_rows;
        _impl->trained = true;

        return true;

    } catch (std::exception const & e) {
        std::cerr << "TSNEOperation::fitTransform failed: " << e.what() << "\n";
        _impl->trained = false;
        return false;
    }
}

// ============================================================================
// transform — not supported for t-SNE
// ============================================================================

bool TSNEOperation::transform(
        arma::mat const & /*features*/,
        arma::mat & /*result*/) {
    std::cerr << "TSNEOperation::transform: t-SNE does not support out-of-sample projection.\n";
    return false;
}

// ============================================================================
// Accessors
// ============================================================================

std::size_t TSNEOperation::outputDimensions() const {
    return _impl->trained ? _impl->n_components : 0;
}

std::vector<double> TSNEOperation::explainedVarianceRatio() const {
    // Explained variance is not defined for t-SNE
    return {};
}

}// namespace MLCore
