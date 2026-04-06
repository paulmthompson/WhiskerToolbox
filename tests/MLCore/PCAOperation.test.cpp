/**
 * @file PCAOperation.test.cpp
 * @brief Catch2 tests for MLCore::PCAOperation
 *
 * Tests the PCA dimensionality reduction implementation against known
 * matrices. Covers:
 * - Metadata (name, task type, default parameters)
 * - fitTransform on identity covariance data
 * - Explained variance ratio correctness
 * - Re-projection via transform()
 * - Row count preservation
 * - Edge cases (empty data, too few observations, n_components > features)
 * - Save/load round-trip
 * - Registry integration
 */

#include "models/unsupervised/PCAOperation.hpp"
#include "models/MLDimReductionOperation.hpp"
#include "models/MLModelParameters.hpp"
#include "models/MLModelRegistry.hpp"
#include "models/MLTaskType.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <armadillo>

#include <sstream>

using namespace MLCore;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Helper: create synthetic 2D data with known PCA structure
// ============================================================================

namespace {

/**
 * @brief Generate 2D data where most variance is along the X axis
 *
 * X ~ N(0, 10²), Y ~ N(0, 1²). PC1 should be ~X, PC2 ~Y.
 * The explained variance ratio of PC1 should be ~0.99.
 *
 * @param n_observations Number of samples
 * @param seed Random seed
 * @return Features in mlpack convention (2 × n_observations)
 */
auto makeHighVarianceXData(std::size_t n_observations, int seed = 42) -> arma::mat {
    arma::arma_rng::set_seed(seed);

    arma::mat data(2, n_observations);
    data.row(0) = arma::randn<arma::rowvec>(n_observations) * 10.0;// high variance
    data.row(1) = arma::randn<arma::rowvec>(n_observations) * 1.0; // low variance
    return data;
}

/**
 * @brief Generate 3D data with known covariance for n_components=2 tests
 *
 * Feature 0: high variance (std=10)
 * Feature 1: medium variance (std=3)
 * Feature 2: low variance (std=0.1)
 *
 * @return Features in mlpack convention (3 × n_observations)
 */
auto make3DData(std::size_t n_observations, int seed = 42) -> arma::mat {
    arma::arma_rng::set_seed(seed);

    arma::mat data(3, n_observations);
    data.row(0) = arma::randn<arma::rowvec>(n_observations) * 10.0;
    data.row(1) = arma::randn<arma::rowvec>(n_observations) * 3.0;
    data.row(2) = arma::randn<arma::rowvec>(n_observations) * 0.1;
    return data;
}

}// namespace

// ============================================================================
// Metadata tests
// ============================================================================

TEST_CASE("PCAOperation metadata", "[MLCore][PCA]") {
    PCAOperation const pca;

    SECTION("name is PCA") {
        REQUIRE(pca.getName() == "PCA");
    }

    SECTION("task type is DimensionalityReduction") {
        REQUIRE(pca.getTaskType() == MLTaskType::DimensionalityReduction);
    }

    SECTION("default parameters") {
        auto params = pca.getDefaultParameters();
        REQUIRE(params != nullptr);
        auto const * pca_params = dynamic_cast<PCAParameters const *>(params.get());
        REQUIRE(pca_params != nullptr);
        REQUIRE(pca_params->n_components == 2);
    }

    SECTION("not trained initially") {
        REQUIRE_FALSE(pca.isTrained());
        REQUIRE(pca.numFeatures() == 0);
        REQUIRE(pca.outputDimensions() == 0);
        REQUIRE(pca.explainedVarianceRatio().empty());
    }
}

// ============================================================================
// fitTransform tests
// ============================================================================

TEST_CASE("PCAOperation fitTransform basic", "[MLCore][PCA]") {
    PCAOperation pca;
    PCAParameters params;
    params.n_components = 1;

    auto data = makeHighVarianceXData(100);

    arma::mat result;
    REQUIRE(pca.fitTransform(data, &params, result));

    SECTION("result has correct shape") {
        REQUIRE(result.n_rows == 1);
        REQUIRE(result.n_cols == 100);
    }

    SECTION("model is trained after fitTransform") {
        REQUIRE(pca.isTrained());
        REQUIRE(pca.numFeatures() == 2);
        REQUIRE(pca.outputDimensions() == 1);
    }

    SECTION("PC1 captures most variance") {
        auto ratios = pca.explainedVarianceRatio();
        REQUIRE(ratios.size() == 1);
        REQUIRE(ratios[0] > 0.9);// X variance >> Y variance
    }
}

TEST_CASE("PCAOperation fitTransform 2 components", "[MLCore][PCA]") {
    PCAOperation pca;
    PCAParameters params;
    params.n_components = 2;

    auto data = makeHighVarianceXData(200);

    arma::mat result;
    REQUIRE(pca.fitTransform(data, &params, result));

    REQUIRE(result.n_rows == 2);
    REQUIRE(result.n_cols == 200);

    auto ratios = pca.explainedVarianceRatio();
    REQUIRE(ratios.size() == 2);
    REQUIRE(ratios[0] > ratios[1]);
    REQUIRE_THAT(ratios[0] + ratios[1], WithinAbs(1.0, 1e-10));
}

TEST_CASE("PCAOperation reduce 3D to 2D", "[MLCore][PCA]") {
    PCAOperation pca;
    PCAParameters params;
    params.n_components = 2;

    auto data = make3DData(300);

    arma::mat result;
    REQUIRE(pca.fitTransform(data, &params, result));

    REQUIRE(result.n_rows == 2);
    REQUIRE(result.n_cols == 300);
    REQUIRE(pca.outputDimensions() == 2);
    REQUIRE(pca.numFeatures() == 3);

    auto ratios = pca.explainedVarianceRatio();
    REQUIRE(ratios.size() == 2);
    // PC1 + PC2 should capture almost all variance
    REQUIRE(ratios[0] + ratios[1] > 0.99);
}

// ============================================================================
// transform (re-projection) tests
// ============================================================================

TEST_CASE("PCAOperation transform reprojects new data", "[MLCore][PCA]") {
    PCAOperation pca;
    PCAParameters params;
    params.n_components = 2;

    auto train_data = makeHighVarianceXData(200);
    arma::mat train_result;
    REQUIRE(pca.fitTransform(train_data, &params, train_result));

    auto test_data = makeHighVarianceXData(50, 99);// different seed

    arma::mat test_result;
    REQUIRE(pca.transform(test_data, test_result));

    REQUIRE(test_result.n_rows == 2);
    REQUIRE(test_result.n_cols == 50);
}

TEST_CASE("PCAOperation transform fails when not fitted", "[MLCore][PCA]") {
    PCAOperation pca;

    auto const data = arma::randn<arma::mat>(3, 10);
    arma::mat result;
    REQUIRE_FALSE(pca.transform(data, result));
}

TEST_CASE("PCAOperation transform rejects wrong feature count", "[MLCore][PCA]") {
    PCAOperation pca;
    PCAParameters params;
    params.n_components = 1;

    auto data = makeHighVarianceXData(100);// 2 features
    arma::mat result;
    REQUIRE(pca.fitTransform(data, &params, result));

    arma::mat wrong_features(5, 10);// 5 features, but model expects 2
    wrong_features.fill(1.0);
    REQUIRE_FALSE(pca.transform(wrong_features, result));
}

// ============================================================================
// Edge cases
// ============================================================================

TEST_CASE("PCAOperation rejects empty data", "[MLCore][PCA]") {
    PCAOperation pca;
    PCAParameters params;

    arma::mat const empty_data;
    arma::mat result;
    REQUIRE_FALSE(pca.fitTransform(empty_data, &params, result));
    REQUIRE_FALSE(pca.isTrained());
}

TEST_CASE("PCAOperation rejects fewer than 2 observations", "[MLCore][PCA]") {
    PCAOperation pca;
    PCAParameters params;

    arma::mat const one_obs(3, 1, arma::fill::ones);
    arma::mat result;
    REQUIRE_FALSE(pca.fitTransform(one_obs, &params, result));
}

TEST_CASE("PCAOperation rejects n_components > features", "[MLCore][PCA]") {
    PCAOperation pca;
    PCAParameters params;
    params.n_components = 5;

    arma::mat data(2, 100);// only 2 features
    data.fill(1.0);
    arma::mat result;
    REQUIRE_FALSE(pca.fitTransform(data, &params, result));
}

TEST_CASE("PCAOperation rejects n_components = 0", "[MLCore][PCA]") {
    PCAOperation pca;
    PCAParameters params;
    params.n_components = 0;

    arma::mat data(3, 50);
    data.fill(1.0);
    arma::mat result;
    REQUIRE_FALSE(pca.fitTransform(data, &params, result));
}

// ============================================================================
// Save/Load round-trip
// ============================================================================

TEST_CASE("PCAOperation save and load round-trip", "[MLCore][PCA]") {
    PCAOperation pca;
    PCAParameters params;
    params.n_components = 2;

    auto data = make3DData(200);
    arma::mat result;
    REQUIRE(pca.fitTransform(data, &params, result));

    // Save
    std::ostringstream oss;
    REQUIRE(pca.save(oss));

    // Load into fresh instance
    PCAOperation loaded;
    std::istringstream iss(oss.str());
    REQUIRE(loaded.load(iss));

    REQUIRE(loaded.isTrained());
    REQUIRE(loaded.numFeatures() == 3);
    REQUIRE(loaded.outputDimensions() == 2);

    // Verify transform produces same results
    arma::mat loaded_result;
    REQUIRE(loaded.transform(data, loaded_result));

    REQUIRE(loaded_result.n_rows == result.n_rows);
    REQUIRE(loaded_result.n_cols == result.n_cols);

    // Compare element-wise
    REQUIRE(arma::approx_equal(loaded_result, result, "absdiff", 1e-10));
}

// ============================================================================
// Registry integration
// ============================================================================

TEST_CASE("PCAOperation in MLModelRegistry", "[MLCore][PCA]") {
    MLModelRegistry const registry;

    SECTION("PCA is registered") {
        REQUIRE(registry.hasModel("PCA"));
    }

    SECTION("PCA is listed under DimensionalityReduction task") {
        auto names = registry.getModelNames(MLTaskType::DimensionalityReduction);
        REQUIRE(std::find(names.begin(), names.end(), "PCA") != names.end());
    }

    SECTION("create returns a valid PCAOperation") {
        auto model = registry.create("PCA");
        REQUIRE(model != nullptr);
        REQUIRE(model->getName() == "PCA");
        REQUIRE(model->getTaskType() == MLTaskType::DimensionalityReduction);
    }
}
