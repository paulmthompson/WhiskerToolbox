/**
 * @file SupervisedPCAOperation.test.cpp
 * @brief Catch2 tests for MLCore::SupervisedPCAOperation
 *
 * Tests the supervised PCA dimensionality reduction:
 * - Metadata (name, task type, default parameters)
 * - Binary (2-class) and multi-class (3-class) fitTransform output shape
 * - Eigenvectors capture labeled-subset variance (not full-data variance)
 * - Column names ("SPCA:0", "SPCA:1", ...)
 * - Explained variance ratio sums to <= 1.0
 * - transform() on new data (consistent with fitTransform)
 * - Error handling (untrained, empty, dimension mismatch, single class)
 * - scale=true variant
 * - Save / load round-trip
 * - Registry integration (create by name "Supervised PCA")
 */

#include "models/supervised/SupervisedPCAOperation.hpp"
#include "models/MLModelParameters.hpp"
#include "models/MLModelRegistry.hpp"
#include "models/MLTaskType.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <armadillo>

#include <numeric>
#include <sstream>

using namespace MLCore;

// ============================================================================
// Helpers
// ============================================================================

namespace {

/// Build well-separated clusters. Returns features (D×N) and labels (1×N).
struct SyntheticData {
    arma::mat features;
    arma::Row<std::size_t> labels;
    std::size_t num_classes;
};

SyntheticData makeClusteredData(std::size_t num_classes = 3,
                                std::size_t n_per_class = 40,
                                std::size_t dim = 10,
                                int seed = 42) {
    arma::arma_rng::set_seed(seed);
    std::size_t const total = num_classes * n_per_class;

    arma::mat features(dim, total);
    arma::Row<std::size_t> labels(total);

    for (std::size_t c = 0; c < num_classes; ++c) {
        arma::vec const center = arma::randn<arma::vec>(dim) * 5.0;
        for (std::size_t i = 0; i < n_per_class; ++i) {
            std::size_t const idx = c * n_per_class + i;
            features.col(idx) = center + arma::randn<arma::vec>(dim) * 0.5;
            labels(idx) = c;
        }
    }
    return {features, labels, num_classes};
}

}// anonymous namespace

// ============================================================================
// Metadata
// ============================================================================

TEST_CASE("SupervisedPCAOperation - metadata", "[MLCore][SupervisedPCA]") {
    SupervisedPCAOperation const spca;

    SECTION("name is 'Supervised PCA'") {
        CHECK(spca.getName() == "Supervised PCA");
    }

    SECTION("task type is SupervisedDimensionalityReduction") {
        CHECK(spca.getTaskType() == MLTaskType::SupervisedDimensionalityReduction);
    }

    SECTION("default parameters are SupervisedPCAParameters") {
        auto params = spca.getDefaultParameters();
        REQUIRE(params != nullptr);

        auto const * sp = dynamic_cast<SupervisedPCAParameters const *>(params.get());
        REQUIRE(sp != nullptr);

        CHECK(sp->n_components == 2);
        CHECK(sp->scale == true);
    }

    SECTION("not trained initially") {
        CHECK_FALSE(spca.isTrained());
        CHECK(spca.numFeatures() == 0);
        CHECK(spca.numClasses() == 0);
        CHECK(spca.outputDimensions() == 0);
        CHECK(spca.outputColumnNames().empty());
    }
}

// ============================================================================
// fitTransform
// ============================================================================

TEST_CASE("SupervisedPCAOperation - fitTransform basic", "[MLCore][SupervisedPCA]") {
    auto data = makeClusteredData(3, 40, 10);
    SupervisedPCAOperation spca;
    SupervisedPCAParameters params;
    params.n_components = 3;
    params.scale = true;

    arma::mat result;
    REQUIRE(spca.fitTransform(data.features, data.labels, &params, result));

    SECTION("output has correct shape") {
        CHECK(result.n_rows == 3);
        CHECK(result.n_cols == data.features.n_cols);
    }

    SECTION("model is trained after fitTransform") {
        CHECK(spca.isTrained());
        CHECK(spca.numFeatures() == 10);
        CHECK(spca.numClasses() == 3);
        CHECK(spca.outputDimensions() == 3);
    }

    SECTION("column names follow SPCA:N pattern") {
        auto names = spca.outputColumnNames();
        REQUIRE(names.size() == 3);
        CHECK(names[0] == "SPCA:0");
        CHECK(names[1] == "SPCA:1");
        CHECK(names[2] == "SPCA:2");
    }

    SECTION("explained variance ratios are valid") {
        auto ratios = spca.explainedVarianceRatio();
        REQUIRE(ratios.size() == 3);

        // Each ratio is in [0, 1]
        for (auto r: ratios) {
            CHECK(r >= 0.0);
            CHECK(r <= 1.0);
        }

        // Sum of ratios <= 1.0 (we only take some components)
        double const sum = std::accumulate(ratios.begin(), ratios.end(), 0.0);
        CHECK(sum <= 1.0 + 1e-9);

        // First component >= second (descending variance)
        CHECK(ratios[0] >= ratios[1]);
    }
}

TEST_CASE("SupervisedPCAOperation - fitTransform binary (2-class)", "[MLCore][SupervisedPCA]") {
    auto data = makeClusteredData(2, 50, 5);
    SupervisedPCAOperation spca;
    SupervisedPCAParameters params;
    params.n_components = 2;
    params.scale = false;

    arma::mat result;
    REQUIRE(spca.fitTransform(data.features, data.labels, &params, result));

    CHECK(result.n_rows == 2);
    CHECK(result.n_cols == data.features.n_cols);
    CHECK(spca.numClasses() == 2);
}

TEST_CASE("SupervisedPCAOperation - PCA captures labeled variance", "[MLCore][SupervisedPCA]") {
    // Create a dataset where labeled data has strong variance along dim 0,
    // but the full dataset (if there were other data) would spread differently.
    // Here we verify that eigenvectors are computed from the labeled data.
    arma::arma_rng::set_seed(123);

    // 2D features, 40 observations
    // Class 0: spread along x-axis at y=0
    // Class 1: spread along x-axis at y=5
    std::size_t const N = 40;
    arma::mat features(2, N);
    arma::Row<std::size_t> labels(N);

    for (std::size_t i = 0; i < N / 2; ++i) {
        features(0, i) = arma::randn() * 10.0;// large x variance
        features(1, i) = arma::randn() * 0.1; // small y variance
        labels(i) = 0;
    }
    for (std::size_t i = N / 2; i < N; ++i) {
        features(0, i) = arma::randn() * 10.0;// large x variance
        features(1, i) = 5.0 + arma::randn() * 0.1;
        labels(i) = 1;
    }

    SupervisedPCAOperation spca;
    SupervisedPCAParameters params;
    params.n_components = 2;
    params.scale = false;

    arma::mat result;
    REQUIRE(spca.fitTransform(features, labels, &params, result));

    // First PC should capture most variance
    auto ratios = spca.explainedVarianceRatio();
    REQUIRE(ratios.size() == 2);
    CHECK(ratios[0] > 0.9);// x-axis dominates
}

// ============================================================================
// transform
// ============================================================================

TEST_CASE("SupervisedPCAOperation - transform after fitTransform", "[MLCore][SupervisedPCA]") {
    auto data = makeClusteredData(2, 50, 5);
    SupervisedPCAOperation spca;
    SupervisedPCAParameters params;
    params.n_components = 2;

    arma::mat fit_result;
    REQUIRE(spca.fitTransform(data.features, data.labels, &params, fit_result));

    arma::mat transform_result;
    REQUIRE(spca.transform(data.features, transform_result));

    // fit_result and transform_result should be identical (same data)
    CHECK(arma::approx_equal(fit_result, transform_result, "absdiff", 1e-10));
}

TEST_CASE("SupervisedPCAOperation - transform new data", "[MLCore][SupervisedPCA]") {
    auto data = makeClusteredData(2, 50, 5);
    SupervisedPCAOperation spca;
    SupervisedPCAParameters params;
    params.n_components = 2;

    arma::mat fit_result;
    REQUIRE(spca.fitTransform(data.features, data.labels, &params, fit_result));

    // Project new data
    arma::arma_rng::set_seed(99);
    auto const new_features = arma::randn<arma::mat>(5, 10);
    arma::mat new_result;
    REQUIRE(spca.transform(new_features, new_result));

    CHECK(new_result.n_rows == 2);
    CHECK(new_result.n_cols == 10);
}

// ============================================================================
// Error handling
// ============================================================================

TEST_CASE("SupervisedPCAOperation - error handling", "[MLCore][SupervisedPCA]") {
    SupervisedPCAOperation spca;

    SECTION("transform before fitTransform fails") {
        auto const dummy_features = arma::randn<arma::mat>(3, 10);
        arma::mat result;
        CHECK_FALSE(spca.transform(dummy_features, result));
    }

    SECTION("empty features") {
        arma::mat const empty;
        arma::Row<std::size_t> const labels = {0, 1};
        arma::mat result;
        CHECK_FALSE(spca.fitTransform(empty, labels, nullptr, result));
    }

    SECTION("label count mismatch") {
        auto const features = arma::randn<arma::mat>(3, 10);
        arma::Row<std::size_t> const labels = {0, 1, 0};// only 3 labels, 10 cols
        arma::mat result;
        CHECK_FALSE(spca.fitTransform(features, labels, nullptr, result));
    }

    SECTION("single class fails") {
        auto const features = arma::randn<arma::mat>(3, 10);
        arma::Row<std::size_t> const labels(10, arma::fill::zeros);
        arma::mat result;
        CHECK_FALSE(spca.fitTransform(features, labels, nullptr, result));
    }

    SECTION("n_components exceeds features") {
        auto const features = arma::randn<arma::mat>(3, 10);
        arma::Row<std::size_t> labels(10);
        for (std::size_t i = 0; i < 10; ++i) labels(i) = i % 2;

        SupervisedPCAParameters params;
        params.n_components = 5;// > 3 features

        arma::mat result;
        CHECK_FALSE(spca.fitTransform(features, labels, &params, result));
    }

    SECTION("dimension mismatch on transform") {
        auto data = makeClusteredData(2, 30, 5);
        SupervisedPCAParameters params;
        params.n_components = 2;

        arma::mat result;
        REQUIRE(spca.fitTransform(data.features, data.labels, &params, result));

        auto const wrong_dim = arma::randn<arma::mat>(3, 10);// 3 != 5
        arma::mat bad_result;
        CHECK_FALSE(spca.transform(wrong_dim, bad_result));
    }
}

// ============================================================================
// Save / Load
// ============================================================================

TEST_CASE("SupervisedPCAOperation - save/load round-trip", "[MLCore][SupervisedPCA]") {
    auto data = makeClusteredData(3, 40, 8);
    SupervisedPCAOperation spca;
    SupervisedPCAParameters params;
    params.n_components = 3;
    params.scale = true;

    arma::mat original_result;
    REQUIRE(spca.fitTransform(data.features, data.labels, &params, original_result));

    // Save
    std::stringstream ss;
    REQUIRE(spca.save(ss));

    // Load into a new instance
    SupervisedPCAOperation loaded;
    REQUIRE(loaded.load(ss));

    CHECK(loaded.isTrained());
    CHECK(loaded.numFeatures() == spca.numFeatures());
    CHECK(loaded.numClasses() == spca.numClasses());
    CHECK(loaded.outputDimensions() == spca.outputDimensions());
    CHECK(loaded.outputColumnNames() == spca.outputColumnNames());

    // Transform should give identical result
    arma::mat loaded_result;
    REQUIRE(loaded.transform(data.features, loaded_result));
    CHECK(arma::approx_equal(original_result, loaded_result, "absdiff", 1e-10));
}

TEST_CASE("SupervisedPCAOperation - save untrained fails", "[MLCore][SupervisedPCA]") {
    SupervisedPCAOperation const spca;
    std::stringstream ss;
    CHECK_FALSE(spca.save(ss));
}

// ============================================================================
// Registry integration
// ============================================================================

TEST_CASE("SupervisedPCAOperation - registry integration", "[MLCore][SupervisedPCA]") {
    MLModelRegistry const registry;

    SECTION("model is registered") {
        CHECK(registry.hasModel("Supervised PCA"));
    }

    SECTION("task type is correct") {
        auto task = registry.getTaskType("Supervised PCA");
        REQUIRE(task.has_value());
        CHECK(*task == MLTaskType::SupervisedDimensionalityReduction);
    }

    SECTION("create returns functional instance") {
        auto model = registry.create("Supervised PCA");
        REQUIRE(model != nullptr);
        CHECK(model->getName() == "Supervised PCA");
        CHECK_FALSE(model->isTrained());
    }

    SECTION("appears in supervised dim reduction model list") {
        auto names = registry.getModelNames(MLTaskType::SupervisedDimensionalityReduction);
        bool found = false;
        for (auto const & n: names) {
            if (n == "Supervised PCA") {
                found = true;
                break;
            }
        }
        CHECK(found);
    }
}
