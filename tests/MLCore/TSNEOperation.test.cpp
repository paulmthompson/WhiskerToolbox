/**
 * @file TSNEOperation.test.cpp
 * @brief Catch2 tests for TSNEOperation (tapkee t-SNE wrapper)
 */

#include "MLCore/models/unsupervised/TSNEOperation.hpp"

#include "MLCore/models/MLModelParameters.hpp"

#include <catch2/catch_test_macros.hpp>

#include <armadillo>
#include <cstddef>

using namespace MLCore;

// ============================================================================
// Helpers
// ============================================================================

namespace {

/// Create a simple test matrix: 3 features × 30 observations
/// with separable clusters for t-SNE to find.
arma::mat makeClusteredData() {
    constexpr arma::uword n_feat = 3;
    constexpr arma::uword n_obs = 30;
    arma::mat data(n_feat, n_obs, arma::fill::zeros);

    // Cluster A: first 15 points near (0,0,0)
    for (arma::uword i = 0; i < 15; ++i) {
        data(0, i) = 0.1 * static_cast<double>(i);
        data(1, i) = 0.1 * static_cast<double>(i);
        data(2, i) = 0.1 * static_cast<double>(i);
    }
    // Cluster B: next 15 points near (10,10,10)
    for (arma::uword i = 15; i < 30; ++i) {
        data(0, i) = 10.0 + 0.1 * static_cast<double>(i - 15);
        data(1, i) = 10.0 + 0.1 * static_cast<double>(i - 15);
        data(2, i) = 10.0 + 0.1 * static_cast<double>(i - 15);
    }
    return data;
}

}// namespace

// ============================================================================
// Metadata
// ============================================================================

TEST_CASE("TSNEOperation metadata", "[MLCore][t-SNE]") {
    TSNEOperation tsne;

    REQUIRE(tsne.getName() == "t-SNE");
    REQUIRE(tsne.getTaskType() == MLTaskType::DimensionalityReduction);
    REQUIRE_FALSE(tsne.isTrained());
    REQUIRE(tsne.numFeatures() == 0);
    REQUIRE(tsne.outputDimensions() == 0);
    REQUIRE(tsne.explainedVarianceRatio().empty());

    auto defaults = tsne.getDefaultParameters();
    REQUIRE(defaults != nullptr);
    auto * p = dynamic_cast<TSNEParameters *>(defaults.get());
    REQUIRE(p != nullptr);
    REQUIRE(p->n_components == 2);
    REQUIRE(p->perplexity == 30.0);
    REQUIRE(p->theta == 0.5);
}

// ============================================================================
// Basic fitTransform
// ============================================================================

TEST_CASE("TSNEOperation fitTransform produces correct shape", "[MLCore][t-SNE]") {
    TSNEOperation tsne;
    auto data = makeClusteredData();// 3 × 30

    TSNEParameters params;
    params.n_components = 2;
    params.perplexity = 5.0;// small perplexity for small dataset

    arma::mat result;
    bool const ok = tsne.fitTransform(data, &params, result);

    REQUIRE(ok);
    REQUIRE(result.n_rows == 2);
    REQUIRE(result.n_cols == 30);
    REQUIRE(tsne.isTrained());
    REQUIRE(tsne.numFeatures() == 3);
    REQUIRE(tsne.outputDimensions() == 2);
}

// ============================================================================
// transform() is unsupported
// ============================================================================

TEST_CASE("TSNEOperation transform returns false", "[MLCore][t-SNE]") {
    TSNEOperation tsne;
    auto data = makeClusteredData();

    TSNEParameters params;
    params.n_components = 2;
    params.perplexity = 5.0;

    arma::mat result;
    tsne.fitTransform(data, &params, result);

    arma::mat new_result;
    REQUIRE_FALSE(tsne.transform(data, new_result));
}

// ============================================================================
// Edge cases
// ============================================================================

TEST_CASE("TSNEOperation rejects empty data", "[MLCore][t-SNE]") {
    TSNEOperation tsne;
    arma::mat const empty;
    TSNEParameters params;

    arma::mat result;
    REQUIRE_FALSE(tsne.fitTransform(empty, &params, result));
    REQUIRE_FALSE(tsne.isTrained());
}

TEST_CASE("TSNEOperation rejects too few observations", "[MLCore][t-SNE]") {
    TSNEOperation tsne;
    arma::mat const small(3, 2, arma::fill::randu);// only 2 observations
    TSNEParameters params;
    params.perplexity = 1.0;

    arma::mat result;
    REQUIRE_FALSE(tsne.fitTransform(small, &params, result));
}

TEST_CASE("TSNEOperation rejects zero n_components", "[MLCore][t-SNE]") {
    TSNEOperation tsne;
    auto data = makeClusteredData();
    TSNEParameters params;
    params.n_components = 0;

    arma::mat result;
    REQUIRE_FALSE(tsne.fitTransform(data, &params, result));
}

TEST_CASE("TSNEOperation clamps excessive perplexity", "[MLCore][t-SNE]") {
    TSNEOperation tsne;
    auto data = makeClusteredData();// 30 observations

    TSNEParameters params;
    params.n_components = 2;
    params.perplexity = 100.0;// too high for 30 points

    arma::mat result;
    // Should succeed — implementation clamps perplexity internally
    bool const ok = tsne.fitTransform(data, &params, result);
    REQUIRE(ok);
    REQUIRE(result.n_rows == 2);
    REQUIRE(result.n_cols == 30);
}

TEST_CASE("TSNEOperation works with default parameters", "[MLCore][t-SNE]") {
    TSNEOperation tsne;

    // 50 observations to ensure default perplexity=30 is valid
    arma::mat data(5, 100, arma::fill::randu);

    arma::mat result;
    bool const ok = tsne.fitTransform(data, nullptr, result);
    REQUIRE(ok);
    REQUIRE(result.n_rows == 2);
    REQUIRE(result.n_cols == 100);
}
