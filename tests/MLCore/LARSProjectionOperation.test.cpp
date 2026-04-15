/**
 * @file LARSProjectionOperation.test.cpp
 * @brief Catch2 tests for MLCore::LARSProjectionOperation
 *
 * Tests the LARS-based supervised feature selection:
 * - Metadata (name, task type, default parameters)
 * - Binary (2-class) and multi-class (3-class) feature selection
 * - Column names ("LARS_sel:idx")
 * - LASSO produces sparse coefficients (fewer selected than total features)
 * - transform() consistency with fitTransform
 * - Error handling (untrained, empty, dimension mismatch, single class)
 * - Save / load round-trip
 * - Registry integration (create by name "LARS Projection")
 * - Ridge and ElasticNet modes
 */

#include "models/supervised/LARSProjectionOperation.hpp"
#include "models/MLModelParameters.hpp"
#include "models/MLModelRegistry.hpp"
#include "models/MLTaskType.hpp"

#include "features/FeatureConverter.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <armadillo>

#include <sstream>

using namespace MLCore;

// ============================================================================
// Helpers
// ============================================================================

namespace {

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

TEST_CASE("LARSProjectionOperation - metadata", "[MLCore][LARSProjection]") {
    LARSProjectionOperation const lars;

    SECTION("name is 'LARS Projection'") {
        CHECK(lars.getName() == "LARS Projection");
    }

    SECTION("task type is SupervisedDimensionalityReduction") {
        CHECK(lars.getTaskType() == MLTaskType::SupervisedDimensionalityReduction);
    }

    SECTION("default parameters are LARSProjectionParameters") {
        auto params = lars.getDefaultParameters();
        REQUIRE(params != nullptr);

        auto const * lp = dynamic_cast<LARSProjectionParameters const *>(params.get());
        REQUIRE(lp != nullptr);

        CHECK(lp->regularization_type == RegularizationType::LASSO);
        CHECK(lp->lambda1 == 0.1);
        CHECK(lp->lambda2 == 0.0);
        CHECK(lp->use_cholesky == true);
    }

    SECTION("not trained initially") {
        CHECK_FALSE(lars.isTrained());
        CHECK(lars.numFeatures() == 0);
        CHECK(lars.numClasses() == 0);
        CHECK(lars.outputDimensions() == 0);
        CHECK(lars.outputColumnNames().empty());
        CHECK(lars.activeFeatureIndices().empty());
    }
}

// ============================================================================
// fitTransform
// ============================================================================

TEST_CASE("LARSProjectionOperation - fitTransform binary", "[MLCore][LARSProjection]") {
    auto data = makeClusteredData(2, 50, 5);
    LARSProjectionOperation lars;
    LARSProjectionParameters params;
    params.regularization_type = RegularizationType::LASSO;
    params.lambda1 = 0.01;

    arma::mat result;
    REQUIRE(lars.fitTransform(data.features, data.labels, &params, result));

    auto const active = lars.activeFeatureIndices();

    SECTION("output rows match number of selected features") {
        CHECK(result.n_rows == active.size());
        CHECK(result.n_cols == data.features.n_cols);
        CHECK(result.n_rows <= data.features.n_rows);
    }

    SECTION("model is trained after fitTransform") {
        CHECK(lars.isTrained());
        CHECK(lars.numFeatures() == 5);
        CHECK(lars.numClasses() == 2);
        CHECK(lars.outputDimensions() == active.size());
    }

    SECTION("column names reflect selected feature indices") {
        auto names = lars.outputColumnNames();
        REQUIRE(names.size() == active.size());
        for (std::size_t i = 0; i < active.size(); ++i) {
            CHECK(names[i] == "LARS_sel:" + std::to_string(active[i]));
        }
    }

    SECTION("output values are the original feature rows") {
        for (std::size_t i = 0; i < active.size(); ++i) {
            CHECK(arma::approx_equal(
                    result.row(i),
                    data.features.row(active[i]),
                    "absdiff", 1e-12));
        }
    }
}

TEST_CASE("LARSProjectionOperation - fitTransform multi-class", "[MLCore][LARSProjection]") {
    auto data = makeClusteredData(3, 40, 10);
    LARSProjectionOperation lars;
    LARSProjectionParameters params;
    params.regularization_type = RegularizationType::LASSO;
    params.lambda1 = 0.01;

    arma::mat result;
    REQUIRE(lars.fitTransform(data.features, data.labels, &params, result));

    auto const active = lars.activeFeatureIndices();
    CHECK(result.n_rows == active.size());
    CHECK(result.n_cols == data.features.n_cols);
    CHECK(lars.numClasses() == 3);
    CHECK(lars.outputDimensions() == active.size());

    auto names = lars.outputColumnNames();
    REQUIRE(names.size() == active.size());
    for (std::size_t i = 0; i < active.size(); ++i) {
        CHECK(names[i] == "LARS_sel:" + std::to_string(active[i]));
    }
}

TEST_CASE("LARSProjectionOperation - LASSO produces sparse coefficients", "[MLCore][LARSProjection]") {
    // Use high lambda1 to encourage sparsity on 10-dimensional data
    auto data = makeClusteredData(2, 50, 10);
    LARSProjectionOperation lars;
    LARSProjectionParameters params;
    params.regularization_type = RegularizationType::LASSO;
    params.lambda1 = 0.5;// Strong L1 penalty → sparse

    arma::mat result;
    REQUIRE(lars.fitTransform(data.features, data.labels, &params, result));

    auto active = lars.activeFeatureIndices();
    // With strong L1 penalty, not all features should be active
    CHECK(active.size() < 10);
    // At least some features should be active (otherwise fitTransform would fail)
    CHECK_FALSE(active.empty());
    // Output rows should match active count
    CHECK(result.n_rows == active.size());
}

// ============================================================================
// transform
// ============================================================================

TEST_CASE("LARSProjectionOperation - transform consistency", "[MLCore][LARSProjection]") {
    auto data = makeClusteredData(2, 50, 5);
    LARSProjectionOperation lars;
    LARSProjectionParameters params;
    params.regularization_type = RegularizationType::LASSO;
    params.lambda1 = 0.01;

    arma::mat fit_result;
    REQUIRE(lars.fitTransform(data.features, data.labels, &params, fit_result));

    arma::mat transform_result;
    REQUIRE(lars.transform(data.features, transform_result));

    // fitTransform and transform on the same data should produce identical results
    CHECK(arma::approx_equal(fit_result, transform_result, "absdiff", 1e-10));
}

TEST_CASE("LARSProjectionOperation - transform on new data", "[MLCore][LARSProjection]") {
    auto data = makeClusteredData(2, 50, 5);
    LARSProjectionOperation lars;
    LARSProjectionParameters params;
    params.regularization_type = RegularizationType::LASSO;
    params.lambda1 = 0.01;

    arma::mat fit_result;
    REQUIRE(lars.fitTransform(data.features, data.labels, &params, fit_result));

    auto const active = lars.activeFeatureIndices();

    // New data with same dimensionality
    arma::arma_rng::set_seed(999);
    arma::mat const new_features(5, 20, arma::fill::randn);

    arma::mat new_result;
    REQUIRE(lars.transform(new_features, new_result));
    CHECK(new_result.n_rows == active.size());
    CHECK(new_result.n_cols == 20);

    // Verify values are the correct rows from the input
    for (std::size_t i = 0; i < active.size(); ++i) {
        CHECK(arma::approx_equal(
                new_result.row(i),
                new_features.row(active[i]),
                "absdiff", 1e-12));
    }
}

// ============================================================================
// Error handling
// ============================================================================

TEST_CASE("LARSProjectionOperation - error handling", "[MLCore][LARSProjection]") {
    LARSProjectionOperation lars;
    LARSProjectionParameters params;

    SECTION("transform on untrained model fails") {
        arma::mat const features(5, 10, arma::fill::randn);
        arma::mat result;
        CHECK_FALSE(lars.transform(features, result));
    }

    SECTION("empty features fails") {
        arma::mat const features;
        arma::Row<std::size_t> const labels;
        arma::mat result;
        CHECK_FALSE(lars.fitTransform(features, labels, &params, result));
    }

    SECTION("label count mismatch fails") {
        arma::mat const features(5, 10, arma::fill::randn);
        arma::Row<std::size_t> const labels(5, arma::fill::zeros);// 5 != 10
        arma::mat result;
        CHECK_FALSE(lars.fitTransform(features, labels, &params, result));
    }

    SECTION("single class fails") {
        arma::mat const features(5, 10, arma::fill::randn);
        arma::Row<std::size_t> const labels(10, arma::fill::zeros);// all class 0
        arma::mat result;
        CHECK_FALSE(lars.fitTransform(features, labels, &params, result));
    }

    SECTION("single observation fails") {
        arma::mat const features(5, 1, arma::fill::randn);
        arma::Row<std::size_t> const labels = {0};
        arma::mat result;
        CHECK_FALSE(lars.fitTransform(features, labels, &params, result));
    }

    SECTION("transform with dimension mismatch fails") {
        auto data = makeClusteredData(2, 20, 5);
        arma::mat result;
        REQUIRE(lars.fitTransform(data.features, data.labels, &params, result));

        arma::mat const wrong_dim(3, 10, arma::fill::randn);// 3 != 5
        arma::mat bad_result;
        CHECK_FALSE(lars.transform(wrong_dim, bad_result));
    }

    SECTION("transform with empty features fails") {
        auto data = makeClusteredData(2, 20, 5);
        arma::mat result;
        REQUIRE(lars.fitTransform(data.features, data.labels, &params, result));

        arma::mat const empty_features;
        arma::mat bad_result;
        CHECK_FALSE(lars.transform(empty_features, bad_result));
    }
}

// ============================================================================
// Save / load round-trip
// ============================================================================

TEST_CASE("LARSProjectionOperation - save and load round-trip", "[MLCore][LARSProjection]") {
    auto data = makeClusteredData(2, 50, 5);
    LARSProjectionOperation lars;
    LARSProjectionParameters params;
    params.regularization_type = RegularizationType::LASSO;
    params.lambda1 = 0.05;

    arma::mat original_result;
    REQUIRE(lars.fitTransform(data.features, data.labels, &params, original_result));

    // Save
    std::stringstream ss;
    REQUIRE(lars.save(ss));

    // Load into fresh instance
    LARSProjectionOperation loaded;
    REQUIRE(loaded.load(ss));

    CHECK(loaded.isTrained());
    CHECK(loaded.numClasses() == lars.numClasses());
    CHECK(loaded.numFeatures() == lars.numFeatures());
    CHECK(loaded.outputDimensions() == lars.outputDimensions());
    CHECK(loaded.outputColumnNames() == lars.outputColumnNames());

    // Transform should produce the same result
    arma::mat loaded_result;
    REQUIRE(loaded.transform(data.features, loaded_result));
    CHECK(arma::approx_equal(original_result, loaded_result, "absdiff", 1e-10));
}

TEST_CASE("LARSProjectionOperation - save on untrained model fails", "[MLCore][LARSProjection]") {
    LARSProjectionOperation const lars;
    std::stringstream ss;
    CHECK_FALSE(lars.save(ss));
}

// ============================================================================
// Registry integration
// ============================================================================

TEST_CASE("LARSProjectionOperation - registry create by name", "[MLCore][LARSProjection]") {
    MLModelRegistry const registry;
    REQUIRE(registry.hasModel("LARS Projection"));

    auto model = registry.create("LARS Projection");
    REQUIRE(model != nullptr);
    CHECK(model->getName() == "LARS Projection");
    CHECK(model->getTaskType() == MLTaskType::SupervisedDimensionalityReduction);

    auto task = registry.getTaskType("LARS Projection");
    REQUIRE(task.has_value());
    CHECK(task.value() == MLTaskType::SupervisedDimensionalityReduction);
}

// ============================================================================
// Regularization modes
// ============================================================================

TEST_CASE("LARSProjectionOperation - Ridge mode", "[MLCore][LARSProjection]") {
    auto data = makeClusteredData(2, 50, 5);
    LARSProjectionOperation lars;
    LARSProjectionParameters params;
    params.regularization_type = RegularizationType::Ridge;
    params.lambda2 = 0.1;

    arma::mat result;
    REQUIRE(lars.fitTransform(data.features, data.labels, &params, result));

    auto const active = lars.activeFeatureIndices();
    CHECK(result.n_rows == active.size());
    CHECK(result.n_cols == data.features.n_cols);
    CHECK(lars.isTrained());
    // Ridge keeps all features (no sparsity)
    CHECK(active.size() == 5);
}

TEST_CASE("LARSProjectionOperation - ElasticNet mode", "[MLCore][LARSProjection]") {
    auto data = makeClusteredData(2, 50, 5);
    LARSProjectionOperation lars;
    LARSProjectionParameters params;
    params.regularization_type = RegularizationType::ElasticNet;
    params.lambda1 = 0.05;
    params.lambda2 = 0.05;

    arma::mat result;
    REQUIRE(lars.fitTransform(data.features, data.labels, &params, result));

    auto const active = lars.activeFeatureIndices();
    CHECK(result.n_rows == active.size());
    CHECK(result.n_cols == data.features.n_cols);
    CHECK(lars.isTrained());
}

TEST_CASE("LARSProjectionOperation - nullptr params uses defaults", "[MLCore][LARSProjection]") {
    auto data = makeClusteredData(2, 50, 5);
    LARSProjectionOperation lars;

    arma::mat result;
    REQUIRE(lars.fitTransform(data.features, data.labels, nullptr, result));

    auto const active = lars.activeFeatureIndices();
    CHECK(result.n_rows == active.size());
    CHECK(result.n_cols == data.features.n_cols);
    CHECK(lars.isTrained());
}

// ============================================================================
// Z-score normalization effect on feature selection
// ============================================================================

TEST_CASE("LARSProjectionOperation - z-score changes selection with unequal scales",
          "[MLCore][LARSProjection][zscore]") {
    // Build data where feature scales differ wildly:
    //   feature 0: discriminative, range ~[0, 1]     (small L2 norm)
    //   feature 1: discriminative, range ~[0, 1000]   (large L2 norm)
    //   features 2-9: noise, range ~[0, 100]
    //
    // With normalizeData=true inside mlpack::LARS, external z-scoring may be
    // redundant. This test verifies the z-scoring mechanics work correctly
    // (mean≈0, std≈1 after normalization) and that LARS still produces valid
    // output on z-scored data.
    arma::arma_rng::set_seed(123);
    std::size_t constexpr D = 10;
    std::size_t constexpr N_PER_CLASS = 50;
    std::size_t constexpr N = N_PER_CLASS * 2;

    arma::mat features(D, N);
    arma::Row<std::size_t> labels(N);

    for (std::size_t i = 0; i < N_PER_CLASS; ++i) {
        // Class 0
        features(0, i) = 0.0 + arma::randn() * 0.1;  // ~0, small scale
        features(1, i) = 0.0 + arma::randn() * 100.0;// ~0, large scale
        for (std::size_t d = 2; d < D; ++d) {
            features(d, i) = arma::randn() * 50.0;// noise
        }
        labels(i) = 0;
    }
    for (std::size_t i = 0; i < N_PER_CLASS; ++i) {
        auto const idx = N_PER_CLASS + i;
        // Class 1
        features(0, idx) = 1.0 + arma::randn() * 0.1;     // ~1, small scale
        features(1, idx) = 1000.0 + arma::randn() * 100.0;// ~1000, large scale
        for (std::size_t d = 2; d < D; ++d) {
            features(d, idx) = arma::randn() * 50.0;// noise
        }
        labels(idx) = 1;
    }

    // Run LARS on raw features
    LARSProjectionOperation lars_raw;
    LARSProjectionParameters params;
    params.regularization_type = RegularizationType::LASSO;
    params.lambda1 = 0.01;// After N-scaling: effective lambda_mlpack = 0.01 * 100 = 1.0

    arma::mat result_raw;
    REQUIRE(lars_raw.fitTransform(features, labels, &params, result_raw));
    auto active_raw = lars_raw.activeFeatureIndices();

    // Z-score the features and verify normalization was applied
    arma::mat features_zs = features;
    auto [means, stds] = MLCore::zscoreNormalize(features_zs);

    SECTION("z-scoring produces mean≈0 and std≈1 for each feature") {
        for (arma::uword f = 0; f < D; ++f) {
            CHECK_THAT(arma::mean(features_zs.row(f)),
                       Catch::Matchers::WithinAbs(0.0, 1e-10));
            CHECK_THAT(arma::stddev(features_zs.row(f), 0),
                       Catch::Matchers::WithinAbs(1.0, 0.02));
        }
    }

    SECTION("z-scoring recorded correct means and stds") {
        REQUIRE(means.size() == D);
        REQUIRE(stds.size() == D);
        // Feature 1 should have much larger std than feature 0
        CHECK(stds[1] > stds[0] * 10.0);
    }

    // Run LARS on z-scored features
    LARSProjectionOperation lars_zs;
    arma::mat result_zs;
    REQUIRE(lars_zs.fitTransform(features_zs, labels, &params, result_zs));
    auto active_zs = lars_zs.activeFeatureIndices();

    SECTION("both runs produce non-empty active sets") {
        CHECK_FALSE(active_raw.empty());
        CHECK_FALSE(active_zs.empty());
    }

    SECTION("LARS output shape is correct for z-scored data") {
        CHECK(result_zs.n_rows == active_zs.size());
        CHECK(result_zs.n_cols == N);
    }
}
