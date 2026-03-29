/**
 * @file RobustPCAOperation.test.cpp
 * @brief Catch2 tests for RobustPCAOperation (ROSL wrapper)
 */

#include "MLCore/models/unsupervised/RobustPCAOperation.hpp"

#include "MLCore/models/MLModelParameters.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <armadillo>
#include <cstddef>
#include <sstream>

using namespace MLCore;

// ============================================================================
// Helpers
// ============================================================================

namespace {

/// Create a low-rank matrix with sparse corruption.
/// Base: rank-2 matrix (50 features x 30 observations) + sparse outliers.
arma::mat makeLowRankWithOutliers() {
    arma::arma_rng::set_seed(42);

    constexpr arma::uword n_feat = 50;
    constexpr arma::uword n_obs = 30;
    constexpr arma::uword true_rank = 2;

    // Low-rank component
    arma::mat const U(n_feat, true_rank, arma::fill::randn);
    arma::mat const V(true_rank, n_obs, arma::fill::randn);
    arma::mat data = U * V;

    // Add sparse corruption (5% of entries)
    for (arma::uword i = 0; i < n_feat * n_obs / 20; ++i) {
        auto const r = static_cast<arma::uword>(arma::randu() * (n_feat - 1));
        auto const c = static_cast<arma::uword>(arma::randu() * (n_obs - 1));
        data(r, c) += 20.0 * (arma::randu() - 0.5);
    }
    return data;
}

/// Simple clean low-rank data without outliers
arma::mat makeCleanData() {
    arma::arma_rng::set_seed(123);
    constexpr arma::uword n_feat = 10;
    constexpr arma::uword n_obs = 20;
    return arma::mat(n_feat, n_obs, arma::fill::randn);
}

}// namespace

// ============================================================================
// Metadata
// ============================================================================

TEST_CASE("RobustPCAOperation metadata", "[MLCore][RobustPCA]") {
    RobustPCAOperation const rpca;

    REQUIRE(rpca.getName() == "Robust PCA");
    REQUIRE(rpca.getTaskType() == MLTaskType::DimensionalityReduction);
    REQUIRE_FALSE(rpca.isTrained());
    REQUIRE(rpca.numFeatures() == 0);
    REQUIRE(rpca.outputDimensions() == 0);
    REQUIRE(rpca.explainedVarianceRatio().empty());

    auto defaults = rpca.getDefaultParameters();
    REQUIRE(defaults != nullptr);
    auto * p = dynamic_cast<RobustPCAParameters *>(defaults.get());
    REQUIRE(p != nullptr);
    REQUIRE(p->n_components == 2);
    REQUIRE(p->lambda == 0.0);
    REQUIRE(p->tol == 1e-5);
    REQUIRE(p->max_iter == 100);
}

// ============================================================================
// Basic fitTransform
// ============================================================================

TEST_CASE("RobustPCAOperation fitTransform produces correct shape", "[MLCore][RobustPCA]") {
    RobustPCAOperation rpca;
    auto data = makeCleanData();// 10 x 20

    RobustPCAParameters params;
    params.n_components = 3;

    arma::mat result;
    bool const ok = rpca.fitTransform(data, &params, result);

    REQUIRE(ok);
    REQUIRE(result.n_rows == 3);
    REQUIRE(result.n_cols == 20);
    REQUIRE(rpca.isTrained());
    REQUIRE(rpca.numFeatures() == 10);
    REQUIRE(rpca.outputDimensions() == 3);
}

TEST_CASE("RobustPCAOperation fitTransform with outlier data", "[MLCore][RobustPCA]") {
    RobustPCAOperation rpca;
    auto data = makeLowRankWithOutliers();// 50 x 30

    RobustPCAParameters params;
    params.n_components = 2;

    arma::mat result;
    bool const ok = rpca.fitTransform(data, &params, result);

    REQUIRE(ok);
    REQUIRE(result.n_rows == 2);
    REQUIRE(result.n_cols == 30);
}

// ============================================================================
// Explained variance
// ============================================================================

TEST_CASE("RobustPCAOperation explained variance sums to <= 1", "[MLCore][RobustPCA]") {
    RobustPCAOperation rpca;
    auto data = makeCleanData();

    RobustPCAParameters params;
    params.n_components = 3;

    arma::mat result;
    rpca.fitTransform(data, &params, result);

    auto const variance = rpca.explainedVarianceRatio();
    REQUIRE(variance.size() == 3);

    double total = 0.0;
    for (auto v: variance) {
        REQUIRE(v >= 0.0);
        total += v;
    }
    REQUIRE(total <= 1.0 + 1e-6);

    // Components should be in descending order of variance
    for (std::size_t i = 1; i < variance.size(); ++i) {
        REQUIRE(variance[i] <= variance[i - 1] + 1e-10);
    }
}

// ============================================================================
// transform (projection of new data)
// ============================================================================

TEST_CASE("RobustPCAOperation transform projects new data", "[MLCore][RobustPCA]") {
    RobustPCAOperation rpca;
    auto data = makeCleanData();// 10 x 20

    RobustPCAParameters params;
    params.n_components = 2;

    arma::mat result;
    rpca.fitTransform(data, &params, result);

    // Project new data with same feature count
    arma::mat const new_data(10, 5, arma::fill::randn);
    arma::mat projected;
    bool const ok = rpca.transform(new_data, projected);

    REQUIRE(ok);
    REQUIRE(projected.n_rows == 2);
    REQUIRE(projected.n_cols == 5);
}

TEST_CASE("RobustPCAOperation transform rejects dimension mismatch", "[MLCore][RobustPCA]") {
    RobustPCAOperation rpca;
    auto data = makeCleanData();// 10 features

    RobustPCAParameters params;
    params.n_components = 2;

    arma::mat result;
    rpca.fitTransform(data, &params, result);

    arma::mat const wrong_dim(7, 5, arma::fill::randn);// wrong feature count
    arma::mat projected;
    REQUIRE_FALSE(rpca.transform(wrong_dim, projected));
}

TEST_CASE("RobustPCAOperation transform before fit returns false", "[MLCore][RobustPCA]") {
    RobustPCAOperation rpca;
    arma::mat const data(10, 5, arma::fill::randn);
    arma::mat projected;
    REQUIRE_FALSE(rpca.transform(data, projected));
}

// ============================================================================
// Edge cases
// ============================================================================

TEST_CASE("RobustPCAOperation rejects empty data", "[MLCore][RobustPCA]") {
    RobustPCAOperation rpca;
    arma::mat const empty;
    RobustPCAParameters params;

    arma::mat result;
    REQUIRE_FALSE(rpca.fitTransform(empty, &params, result));
    REQUIRE_FALSE(rpca.isTrained());
}

TEST_CASE("RobustPCAOperation rejects too few observations", "[MLCore][RobustPCA]") {
    RobustPCAOperation rpca;
    arma::mat const small(5, 1, arma::fill::randu);// only 1 observation
    RobustPCAParameters params;

    arma::mat result;
    REQUIRE_FALSE(rpca.fitTransform(small, &params, result));
}

TEST_CASE("RobustPCAOperation rejects zero n_components", "[MLCore][RobustPCA]") {
    RobustPCAOperation rpca;
    auto data = makeCleanData();
    RobustPCAParameters params;
    params.n_components = 0;

    arma::mat result;
    REQUIRE_FALSE(rpca.fitTransform(data, &params, result));
}

TEST_CASE("RobustPCAOperation rejects n_components > features", "[MLCore][RobustPCA]") {
    RobustPCAOperation rpca;
    arma::mat const data(3, 20, arma::fill::randn);// only 3 features
    RobustPCAParameters params;
    params.n_components = 5;// more than features

    arma::mat result;
    REQUIRE_FALSE(rpca.fitTransform(data, &params, result));
}

TEST_CASE("RobustPCAOperation works with default parameters (nullptr)", "[MLCore][RobustPCA]") {
    RobustPCAOperation rpca;
    auto data = makeCleanData();

    arma::mat result;
    bool const ok = rpca.fitTransform(data, nullptr, result);
    REQUIRE(ok);
    REQUIRE(result.n_rows == 2);// default n_components
    REQUIRE(result.n_cols == 20);
}

// ============================================================================
// Serialization
// ============================================================================

TEST_CASE("RobustPCAOperation save/load round-trip", "[MLCore][RobustPCA]") {
    RobustPCAOperation rpca;
    auto data = makeCleanData();

    RobustPCAParameters params;
    params.n_components = 3;

    arma::mat result;
    rpca.fitTransform(data, &params, result);

    // Save
    std::stringstream ss;
    REQUIRE(rpca.save(ss));

    // Load into fresh instance
    RobustPCAOperation rpca2;
    REQUIRE(rpca2.load(ss));
    REQUIRE(rpca2.isTrained());
    REQUIRE(rpca2.numFeatures() == 10);
    REQUIRE(rpca2.outputDimensions() == 3);

    // Project same data and compare
    arma::mat result2;
    REQUIRE(rpca2.transform(data, result2));
    REQUIRE(result2.n_rows == 3);
    REQUIRE(result2.n_cols == 20);

    // Results should match the original projection
    REQUIRE(arma::approx_equal(result, result2, "absdiff", 1e-10));
}
