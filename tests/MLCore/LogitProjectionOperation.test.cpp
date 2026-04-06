/**
 * @file LogitProjectionOperation.test.cpp
 * @brief Catch2 tests for MLCore::LogitProjectionOperation
 *
 * Tests the supervised dimensionality reduction via logit extraction:
 * - Metadata (name, task type, default parameters)
 * - Binary (2-class) and multi-class (3-class) fitTransform output shape
 * - Class separation quality (correct-class logit dominates)
 * - Column names ("Logit:0", "Logit:1", ...)
 * - transform() on new data (consistent with fitTransform)
 * - Error handling (untrained, empty, dimension mismatch, single class)
 * - Save / load round-trip (logits identical after reload)
 * - Registry integration (create by name "Logit Projection")
 */

#include "models/supervised/LogitProjectionOperation.hpp"
#include "models/MLModelParameters.hpp"
#include "models/MLModelRegistry.hpp"
#include "models/MLTaskType.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <armadillo>

#include <sstream>

using namespace MLCore;

// ============================================================================
// Helpers
// ============================================================================

namespace {

/// Well-separated circular clusters, returns features (D×N) and labels (1×N).
struct SyntheticData {
    arma::mat features;
    arma::Row<std::size_t> labels;
    std::size_t num_classes;
};

SyntheticData makeCircularData(std::size_t num_classes = 3,
                               std::size_t n_per_class = 60,
                               int seed = 42) {
    arma::arma_rng::set_seed(seed);
    double const radius = 6.0;
    double const noise = 0.3;
    std::size_t const total = num_classes * n_per_class;

    arma::mat features(2, total);
    arma::Row<std::size_t> labels(total);

    for (std::size_t c = 0; c < num_classes; ++c) {
        double const angle = 2.0 * arma::datum::pi * static_cast<double>(c) / static_cast<double>(num_classes);
        double const cx = radius * std::cos(angle);
        double const cy = radius * std::sin(angle);

        for (std::size_t i = 0; i < n_per_class; ++i) {
            std::size_t const idx = c * n_per_class + i;
            features(0, idx) = cx + arma::randn() * noise;
            features(1, idx) = cy + arma::randn() * noise;
            labels(idx) = c;
        }
    }
    return {features, labels, num_classes};
}

}// anonymous namespace

// ============================================================================
// Metadata
// ============================================================================

TEST_CASE("LogitProjectionOperation - metadata", "[MLCore][LogitProjection]") {
    LogitProjectionOperation const lp;

    SECTION("name is 'Logit Projection'") {
        CHECK(lp.getName() == "Logit Projection");
    }

    SECTION("task type is SupervisedDimensionalityReduction") {
        CHECK(lp.getTaskType() == MLTaskType::SupervisedDimensionalityReduction);
    }

    SECTION("default parameters are LogitProjectionParameters") {
        auto params = lp.getDefaultParameters();
        REQUIRE(params != nullptr);

        auto const * lp_params =
                dynamic_cast<LogitProjectionParameters const *>(params.get());
        REQUIRE(lp_params != nullptr);

        CHECK_THAT(lp_params->lambda,
                   Catch::Matchers::WithinAbs(0.0001, 1e-12));
        CHECK(lp_params->max_iterations == 10000);
    }

    SECTION("untrained state") {
        CHECK(lp.isTrained() == false);
        CHECK(lp.outputDimensions() == 0);
        CHECK(lp.numClasses() == 0);
        CHECK(lp.numFeatures() == 0);
        CHECK(lp.outputColumnNames().empty());
    }
}

// ============================================================================
// Binary (2-class) fitTransform
// ============================================================================

TEST_CASE("LogitProjectionOperation - binary fitTransform", "[MLCore][LogitProjection]") {
    auto data = makeCircularData(2, 60, 42);
    LogitProjectionOperation lp;
    LogitProjectionParameters params;

    arma::mat logits;
    bool const ok = lp.fitTransform(data.features, data.labels, &params, logits);

    SECTION("fitTransform returns true") {
        CHECK(ok == true);
    }

    SECTION("model is trained after fitTransform") {
        REQUIRE(ok);
        CHECK(lp.isTrained() == true);
    }

    SECTION("output shape is (C × N)") {
        REQUIRE(ok);
        CHECK(logits.n_rows == data.num_classes);
        CHECK(logits.n_cols == data.features.n_cols);
    }

    SECTION("outputDimensions returns num_classes after training") {
        REQUIRE(ok);
        CHECK(lp.outputDimensions() == data.num_classes);
    }

    SECTION("column names have correct count") {
        REQUIRE(ok);
        auto const names = lp.outputColumnNames();
        CHECK(names.size() == data.num_classes);
    }

    SECTION("column names follow 'Logit:N' convention") {
        REQUIRE(ok);
        auto const names = lp.outputColumnNames();
        CHECK(names[0] == "Logit:0");
        CHECK(names[1] == "Logit:1");
    }
}

// ============================================================================
// Multi-class (3-class) fitTransform
// ============================================================================

TEST_CASE("LogitProjectionOperation - multi-class fitTransform", "[MLCore][LogitProjection]") {
    auto data = makeCircularData(3, 60, 99);
    LogitProjectionOperation lp;
    LogitProjectionParameters params;

    arma::mat logits;
    bool const ok = lp.fitTransform(data.features, data.labels, &params, logits);

    REQUIRE(ok);

    SECTION("output rows equal num_classes") {
        CHECK(logits.n_rows == data.num_classes);
    }

    SECTION("output cols equal num samples") {
        CHECK(logits.n_cols == data.features.n_cols);
    }

    SECTION("column names: 3 entries Logit:0..2") {
        auto const names = lp.outputColumnNames();
        REQUIRE(names.size() == 3);
        CHECK(names[0] == "Logit:0");
        CHECK(names[1] == "Logit:1");
        CHECK(names[2] == "Logit:2");
    }

    SECTION("correct-class logit dominates for well-separated data") {
        // For each sample, the logit for the true class should exceed the
        // logits for all other classes (argmax = true label).
        std::size_t correct = 0;
        for (std::size_t i = 0; i < data.features.n_cols; ++i) {
            arma::uword pred = 0;
            logits.col(i).max(pred);
            if (pred == data.labels(i)) {
                ++correct;
            }
        }
        double const accuracy = static_cast<double>(correct) / data.features.n_cols;
        CHECK(accuracy > 0.95);
    }
}

// ============================================================================
// transform() without labels
// ============================================================================

TEST_CASE("LogitProjectionOperation - transform matches fitTransform output", "[MLCore][LogitProjection]") {
    auto data = makeCircularData(3, 60, 7);
    LogitProjectionOperation lp;
    LogitProjectionParameters params;

    arma::mat fit_logits;
    REQUIRE(lp.fitTransform(data.features, data.labels, &params, fit_logits));

    arma::mat transform_logits;
    bool const ok = lp.transform(data.features, transform_logits);

    REQUIRE(ok);

    SECTION("output shape matches") {
        CHECK(transform_logits.n_rows == fit_logits.n_rows);
        CHECK(transform_logits.n_cols == fit_logits.n_cols);
    }

    SECTION("logit values are close to fitTransform output") {
        double const max_diff = arma::max(arma::max(arma::abs(transform_logits - fit_logits)));
        CHECK(max_diff < 1e-10);
    }
}

// ============================================================================
// Error handling
// ============================================================================

TEST_CASE("LogitProjectionOperation - error handling", "[MLCore][LogitProjection]") {
    LogitProjectionOperation lp;
    LogitProjectionParameters params;

    SECTION("transform fails when untrained") {
        arma::mat const features(2, 10, arma::fill::randn);
        arma::mat result;
        CHECK(lp.transform(features, result) == false);
    }

    SECTION("fitTransform fails with empty features") {
        arma::mat const empty_features;
        arma::Row<std::size_t> const labels(10, arma::fill::zeros);
        arma::mat result;
        CHECK(lp.fitTransform(empty_features, labels, &params, result) == false);
    }

    SECTION("fitTransform fails with empty labels") {
        arma::mat const features(2, 10, arma::fill::randn);
        arma::Row<std::size_t> const empty_labels;
        arma::mat result;
        CHECK(lp.fitTransform(features, empty_labels, &params, result) == false);
    }

    SECTION("fitTransform fails with size mismatch") {
        arma::mat const features(2, 10, arma::fill::randn);
        arma::Row<std::size_t> const labels(5, arma::fill::zeros);
        arma::mat result;
        CHECK(lp.fitTransform(features, labels, &params, result) == false);
    }

    SECTION("fitTransform fails with single class") {
        arma::mat const features(2, 10, arma::fill::randn);
        arma::Row<std::size_t> const labels(10, arma::fill::zeros);// all class 0
        arma::mat result;
        CHECK(lp.fitTransform(features, labels, &params, result) == false);
    }

    SECTION("transform fails with dimension mismatch after training") {
        auto data = makeCircularData(2, 30, 1);
        arma::mat logits;
        REQUIRE(lp.fitTransform(data.features, data.labels, &params, logits));

        arma::mat const wrong_dim_features(5, 10, arma::fill::randn);// wrong number of rows
        arma::mat result;
        CHECK(lp.transform(wrong_dim_features, result) == false);
    }
}

// ============================================================================
// Save / load round-trip
// ============================================================================

TEST_CASE("LogitProjectionOperation - save/load round-trip", "[MLCore][LogitProjection]") {
    auto data = makeCircularData(3, 60, 13);
    LogitProjectionOperation original;
    LogitProjectionParameters params;

    arma::mat original_logits;
    REQUIRE(original.fitTransform(data.features, data.labels, &params, original_logits));

    // Save
    std::ostringstream oss;
    bool const saved = original.save(oss);
    REQUIRE(saved);

    // Load into a new instance
    LogitProjectionOperation loaded;
    std::istringstream iss(oss.str());
    bool const loaded_ok = loaded.load(iss);
    REQUIRE(loaded_ok);

    SECTION("loaded model reports trained") {
        CHECK(loaded.isTrained() == true);
    }

    SECTION("loaded model has correct metadata") {
        CHECK(loaded.outputDimensions() == original.outputDimensions());
        CHECK(loaded.numClasses() == original.numClasses());
        CHECK(loaded.numFeatures() == original.numFeatures());
        CHECK(loaded.outputColumnNames() == original.outputColumnNames());
    }

    SECTION("loaded model transforms identically to original") {
        arma::mat const new_features = data.features;// reuse training data for comparison

        arma::mat orig_result;
        REQUIRE(original.transform(new_features, orig_result));

        arma::mat load_result;
        REQUIRE(loaded.transform(new_features, load_result));

        double const max_diff = arma::max(arma::max(arma::abs(orig_result - load_result)));
        CHECK(max_diff < 1e-10);
    }
}

TEST_CASE("LogitProjectionOperation - save fails when untrained", "[MLCore][LogitProjection]") {
    LogitProjectionOperation const lp;
    std::ostringstream oss;
    CHECK(lp.save(oss) == false);
}

// ============================================================================
// Registry integration
// ============================================================================

TEST_CASE("LogitProjectionOperation - registry integration", "[MLCore][LogitProjection]") {
    MLModelRegistry const registry;

    SECTION("registered under 'Logit Projection'") {
        auto model = registry.create("Logit Projection");
        REQUIRE(model != nullptr);
        CHECK(model->getName() == "Logit Projection");
    }

    SECTION("registry-created model is fully functional") {
        auto model = registry.create("Logit Projection");
        REQUIRE(model != nullptr);

        auto data = makeCircularData(2, 40, 77);
        LogitProjectionParameters params;
        arma::mat const logits;

        bool const ok = model->train(data.features, data.labels, &params);
        CHECK(ok == true);
    }
}
