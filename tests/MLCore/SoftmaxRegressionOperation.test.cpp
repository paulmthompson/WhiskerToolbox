/**
 * @file SoftmaxRegressionOperation.test.cpp
 * @brief Catch2 tests for MLCore::SoftmaxRegressionOperation
 *
 * Tests the MLCore-native Softmax Regression implementation against
 * multi-class synthetic data. Covers:
 * - Metadata (name, task type, default parameters)
 * - Training on synthetic multi-class data
 * - Prediction accuracy on well-separated data
 * - Probability output (per-class probabilities sum to ~1)
 * - Feature dimension validation
 * - Edge cases (empty data, untrained model, single-class rejection)
 * - Save/load round-trip
 * - Parameter variations (lambda regularization)
 * - Registry integration
 * - Binary (2-class) operation
 */

#include "models/supervised/SoftmaxRegressionOperation.hpp"
#include "models/MLModelParameters.hpp"
#include "models/MLModelRegistry.hpp"
#include "models/MLTaskType.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <armadillo>

#include <sstream>

using namespace MLCore;

// ============================================================================
// Helper: create well-separated multi-class data
// ============================================================================

namespace {

/**
 * @brief Synthetic multi-class data with well-separated 2D clusters
 */
struct SyntheticMultiClassData {
    arma::mat features;           // 2 × N
    arma::Row<std::size_t> labels;// 1 × N
    std::size_t num_classes;
};

/**
 * @brief Generate well-separated 2D clusters for multi-class classification
 *
 * Creates `num_classes` clusters arranged in a circle of radius 5,
 * each with small variance (0.4). This ensures trivial separability
 * for linear classifiers.
 *
 * @param num_classes   Number of classes (≥ 2)
 * @param n_per_class   Number of samples per class
 * @param seed          Random seed for reproducibility
 */
SyntheticMultiClassData makeMultiClassData(
        std::size_t num_classes = 3,
        std::size_t n_per_class = 50,
        int seed = 42) {
    arma::arma_rng::set_seed(seed);

    double const radius = 5.0;
    double const noise = 0.4;

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

/**
 * @brief Generate linearly separable binary (2-class) data
 */
SyntheticMultiClassData makeBinaryData(std::size_t n_per_class = 50, int seed = 42) {
    return makeMultiClassData(2, n_per_class, seed);
}

}// anonymous namespace

// ============================================================================
// Metadata tests
// ============================================================================

TEST_CASE("SoftmaxRegressionOperation - metadata", "[MLCore][SoftmaxRegression]") {
    SoftmaxRegressionOperation const sr;

    SECTION("name is 'Softmax Regression'") {
        CHECK(sr.getName() == "Softmax Regression");
    }

    SECTION("task type is MultiClassClassification") {
        CHECK(sr.getTaskType() == MLTaskType::MultiClassClassification);
    }

    SECTION("default parameters are SoftmaxRegressionParameters") {
        auto params = sr.getDefaultParameters();
        REQUIRE(params != nullptr);

        auto const * sr_params =
                dynamic_cast<SoftmaxRegressionParameters const *>(params.get());
        REQUIRE(sr_params != nullptr);

        CHECK_THAT(sr_params->lambda, Catch::Matchers::WithinAbs(0.0001, 1e-12));
        CHECK(sr_params->max_iterations == 10000);
    }

    SECTION("not trained initially") {
        CHECK_FALSE(sr.isTrained());
        CHECK(sr.numClasses() == 0);
        CHECK(sr.numFeatures() == 0);
    }
}

// ============================================================================
// Training tests — multi-class
// ============================================================================

TEST_CASE("SoftmaxRegressionOperation - multi-class training", "[MLCore][SoftmaxRegression]") {
    SoftmaxRegressionOperation sr;
    auto data = makeMultiClassData(3, 50);

    SECTION("train with default parameters (nullptr)") {
        bool const ok = sr.train(data.features, data.labels, nullptr);
        CHECK(ok);
        CHECK(sr.isTrained());
        CHECK(sr.numClasses() == 3);
        CHECK(sr.numFeatures() == 2);
    }

    SECTION("train with explicit parameters") {
        SoftmaxRegressionParameters params;
        params.lambda = 0.001;
        params.max_iterations = 5000;

        bool const ok = sr.train(data.features, data.labels, &params);
        CHECK(ok);
        CHECK(sr.isTrained());
        CHECK(sr.numClasses() == 3);
        CHECK(sr.numFeatures() == 2);
    }

    SECTION("train with 4 classes") {
        auto data4 = makeMultiClassData(4, 40);
        bool const ok = sr.train(data4.features, data4.labels, nullptr);
        CHECK(ok);
        CHECK(sr.numClasses() == 4);
    }

    SECTION("train with 5 classes") {
        auto data5 = makeMultiClassData(5, 30);
        bool const ok = sr.train(data5.features, data5.labels, nullptr);
        CHECK(ok);
        CHECK(sr.numClasses() == 5);
    }

    SECTION("retraining resets model") {
        sr.train(data.features, data.labels, nullptr);
        CHECK(sr.numFeatures() == 2);
        CHECK(sr.numClasses() == 3);

        // Train again with different dimensionality and class count
        auto data4 = makeMultiClassData(4, 30);
        arma::mat const features3d = arma::randn(3, data4.features.n_cols);
        bool const ok = sr.train(features3d, data4.labels, nullptr);
        CHECK(ok);
        CHECK(sr.numFeatures() == 3);
        CHECK(sr.numClasses() == 4);
    }
}

// ============================================================================
// Training tests — binary
// ============================================================================

TEST_CASE("SoftmaxRegressionOperation - binary training", "[MLCore][SoftmaxRegression]") {
    SoftmaxRegressionOperation sr;
    auto data = makeBinaryData(50);

    SECTION("train with 2 classes succeeds") {
        bool const ok = sr.train(data.features, data.labels, nullptr);
        CHECK(ok);
        CHECK(sr.isTrained());
        CHECK(sr.numClasses() == 2);
        CHECK(sr.numFeatures() == 2);
    }
}

// ============================================================================
// Training error handling
// ============================================================================

TEST_CASE("SoftmaxRegressionOperation - train error handling", "[MLCore][SoftmaxRegression]") {
    SoftmaxRegressionOperation sr;

    SECTION("empty features") {
        arma::mat const empty_features;
        arma::Row<std::size_t> const labels = {0, 1, 2};
        CHECK_FALSE(sr.train(empty_features, labels, nullptr));
        CHECK_FALSE(sr.isTrained());
    }

    SECTION("empty labels") {
        arma::mat const features = arma::randn(2, 10);
        arma::Row<std::size_t> const empty_labels;
        CHECK_FALSE(sr.train(features, empty_labels, nullptr));
        CHECK_FALSE(sr.isTrained());
    }

    SECTION("features/labels size mismatch") {
        arma::mat const features = arma::randn(2, 10);
        arma::Row<std::size_t> const labels(5, arma::fill::zeros);
        CHECK_FALSE(sr.train(features, labels, nullptr));
        CHECK_FALSE(sr.isTrained());
    }

    SECTION("rejects single-class data (all zeros)") {
        arma::mat const features = arma::randn(2, 20);
        arma::Row<std::size_t> const single_class_labels(20, arma::fill::zeros);
        CHECK_FALSE(sr.train(features, single_class_labels, nullptr));
        CHECK_FALSE(sr.isTrained());
    }
}

// ============================================================================
// Prediction tests — multi-class
// ============================================================================

TEST_CASE("SoftmaxRegressionOperation - multi-class prediction",
          "[MLCore][SoftmaxRegression]") {
    SoftmaxRegressionOperation sr;

    auto train_data = makeMultiClassData(3, 100, /*seed=*/42);
    REQUIRE(sr.train(train_data.features, train_data.labels, nullptr));

    SECTION("high accuracy on training data") {
        arma::Row<std::size_t> predictions;
        bool const ok = sr.predict(train_data.features, predictions);
        REQUIRE(ok);
        REQUIRE(predictions.n_elem == train_data.labels.n_elem);

        std::size_t const correct = arma::accu(predictions == train_data.labels);
        double const accuracy = static_cast<double>(correct) / static_cast<double>(train_data.labels.n_elem);

        // Well-separated clusters — should be near-perfect
        CHECK(accuracy > 0.90);
    }

    SECTION("high accuracy on held-out test data") {
        auto test_data = makeMultiClassData(3, 50, /*seed=*/123);
        arma::Row<std::size_t> predictions;
        bool const ok = sr.predict(test_data.features, predictions);
        REQUIRE(ok);

        std::size_t const correct = arma::accu(predictions == test_data.labels);
        double const accuracy = static_cast<double>(correct) / static_cast<double>(test_data.labels.n_elem);

        CHECK(accuracy > 0.85);
    }

    SECTION("predictions are valid class indices") {
        arma::Row<std::size_t> predictions;
        REQUIRE(sr.predict(train_data.features, predictions));

        CHECK(arma::max(predictions) < train_data.num_classes);
    }
}

// ============================================================================
// Prediction tests — binary via softmax
// ============================================================================

TEST_CASE("SoftmaxRegressionOperation - binary prediction via softmax",
          "[MLCore][SoftmaxRegression]") {
    SoftmaxRegressionOperation sr;

    auto data = makeBinaryData(100, /*seed=*/42);
    REQUIRE(sr.train(data.features, data.labels, nullptr));

    SECTION("high accuracy on binary data") {
        arma::Row<std::size_t> predictions;
        REQUIRE(sr.predict(data.features, predictions));

        std::size_t const correct = arma::accu(predictions == data.labels);
        double const accuracy = static_cast<double>(correct) / static_cast<double>(data.labels.n_elem);

        CHECK(accuracy > 0.90);
    }
}

// ============================================================================
// Prediction error handling
// ============================================================================

TEST_CASE("SoftmaxRegressionOperation - predict error handling",
          "[MLCore][SoftmaxRegression]") {
    SoftmaxRegressionOperation sr;

    SECTION("predict before training fails") {
        arma::mat const features = arma::randn(2, 5);
        arma::Row<std::size_t> predictions;
        CHECK_FALSE(sr.predict(features, predictions));
    }

    SECTION("predict with empty features fails") {
        auto data = makeMultiClassData(3, 30);
        sr.train(data.features, data.labels, nullptr);

        arma::mat const empty_features;
        arma::Row<std::size_t> predictions;
        CHECK_FALSE(sr.predict(empty_features, predictions));
    }

    SECTION("predict with wrong feature dimension fails") {
        auto data = makeMultiClassData(3, 30);
        sr.train(data.features, data.labels, nullptr);
        REQUIRE(sr.numFeatures() == 2);

        arma::mat const wrong_dim = arma::randn(5, 10);// 5 features instead of 2
        arma::Row<std::size_t> predictions;
        CHECK_FALSE(sr.predict(wrong_dim, predictions));
    }
}

// ============================================================================
// Probability output
// ============================================================================

TEST_CASE("SoftmaxRegressionOperation - probability output", "[MLCore][SoftmaxRegression]") {
    SoftmaxRegressionOperation sr;

    auto data = makeMultiClassData(3, 100, /*seed=*/42);
    REQUIRE(sr.train(data.features, data.labels, nullptr));

    SECTION("probabilities have correct dimensions") {
        arma::mat probs;
        bool const ok = sr.predictProbabilities(data.features, probs);
        REQUIRE(ok);

        CHECK(probs.n_rows == sr.numClasses());
        CHECK(probs.n_cols == data.features.n_cols);
    }

    SECTION("probabilities sum to approximately 1 per sample") {
        arma::mat probs;
        REQUIRE(sr.predictProbabilities(data.features, probs));

        arma::rowvec col_sums = arma::sum(probs, 0);
        for (std::size_t i = 0; i < col_sums.n_elem; ++i) {
            CHECK_THAT(col_sums(i), Catch::Matchers::WithinAbs(1.0, 1e-6));
        }
    }

    SECTION("probabilities are in [0, 1]") {
        arma::mat probs;
        REQUIRE(sr.predictProbabilities(data.features, probs));

        CHECK(probs.min() >= 0.0);
        CHECK(probs.max() <= 1.0 + 1e-10);
    }

    SECTION("high-confidence predictions on well-separated data") {
        arma::mat probs;
        REQUIRE(sr.predictProbabilities(data.features, probs));

        arma::Row<std::size_t> predictions;
        REQUIRE(sr.predict(data.features, predictions));

        // Average probability for the predicted class should be > 0.7
        double avg_correct_prob = 0.0;
        for (std::size_t i = 0; i < predictions.n_elem; ++i) {
            avg_correct_prob += probs(predictions(i), i);
        }
        avg_correct_prob /= static_cast<double>(predictions.n_elem);
        CHECK(avg_correct_prob > 0.7);
    }

    SECTION("probability error handling - not trained") {
        SoftmaxRegressionOperation untrained;
        arma::mat probs;
        CHECK_FALSE(untrained.predictProbabilities(data.features, probs));
    }

    SECTION("probability error handling - empty features") {
        arma::mat const empty_features;
        arma::mat probs;
        CHECK_FALSE(sr.predictProbabilities(empty_features, probs));
    }

    SECTION("probability error handling - wrong dimension") {
        arma::mat const wrong_dim = arma::randn(5, 10);
        arma::mat probs;
        CHECK_FALSE(sr.predictProbabilities(wrong_dim, probs));
    }
}

// ============================================================================
// Save / load round-trip
// ============================================================================

TEST_CASE("SoftmaxRegressionOperation - save/load round-trip", "[MLCore][SoftmaxRegression]") {
    auto data = makeMultiClassData(3, 80, /*seed=*/42);

    // Train original model
    SoftmaxRegressionOperation original;
    REQUIRE(original.train(data.features, data.labels, nullptr));

    arma::Row<std::size_t> original_predictions;
    REQUIRE(original.predict(data.features, original_predictions));

    SECTION("save and load produces identical predictions") {
        // Save
        std::stringstream ss;
        REQUIRE(original.save(ss));

        // Load into fresh model
        SoftmaxRegressionOperation loaded;
        CHECK_FALSE(loaded.isTrained());
        REQUIRE(loaded.load(ss));

        CHECK(loaded.isTrained());
        CHECK(loaded.numClasses() == original.numClasses());
        CHECK(loaded.numFeatures() == original.numFeatures());

        // Predictions should match
        arma::Row<std::size_t> loaded_predictions;
        REQUIRE(loaded.predict(data.features, loaded_predictions));
        CHECK(arma::all(loaded_predictions == original_predictions));
    }

    SECTION("save and load preserves probabilities") {
        arma::mat original_probs;
        REQUIRE(original.predictProbabilities(data.features, original_probs));

        std::stringstream ss;
        REQUIRE(original.save(ss));

        SoftmaxRegressionOperation loaded;
        REQUIRE(loaded.load(ss));

        arma::mat loaded_probs;
        REQUIRE(loaded.predictProbabilities(data.features, loaded_probs));

        CHECK(arma::approx_equal(original_probs, loaded_probs, "absdiff", 1e-10));
    }

    SECTION("save fails when not trained") {
        SoftmaxRegressionOperation const untrained;
        std::stringstream ss;
        CHECK_FALSE(untrained.save(ss));
    }

    SECTION("load from corrupted stream fails") {
        std::stringstream ss("garbage data that is not a valid model");
        SoftmaxRegressionOperation loaded;
        CHECK_FALSE(loaded.load(ss));
        CHECK_FALSE(loaded.isTrained());
    }
}

// ============================================================================
// Parameter variations
// ============================================================================

TEST_CASE("SoftmaxRegressionOperation - parameter variations", "[MLCore][SoftmaxRegression]") {
    auto data = makeMultiClassData(3, 60, /*seed=*/42);

    SECTION("different lambda values produce valid models") {
        for (double const lambda: {0.0, 0.001, 0.01, 0.1, 1.0}) {
            SoftmaxRegressionOperation sr;
            SoftmaxRegressionParameters params;
            params.lambda = lambda;

            REQUIRE(sr.train(data.features, data.labels, &params));
            CHECK(sr.isTrained());

            arma::Row<std::size_t> predictions;
            REQUIRE(sr.predict(data.features, predictions));

            std::size_t const correct = arma::accu(predictions == data.labels);
            double const accuracy = static_cast<double>(correct) / static_cast<double>(data.labels.n_elem);

            // All should achieve reasonable accuracy on well-separated data
            CHECK(accuracy > 0.80);
        }
    }

    SECTION("wrong parameter type falls back to defaults") {
        SoftmaxRegressionOperation sr;
        RandomForestParameters wrong_params;// Wrong type
        bool const ok = sr.train(data.features, data.labels, &wrong_params);
        CHECK(ok);
        CHECK(sr.isTrained());
    }

    SECTION("custom max_iterations trains successfully") {
        SoftmaxRegressionOperation sr;
        SoftmaxRegressionParameters params;
        params.max_iterations = 100;

        bool const ok = sr.train(data.features, data.labels, &params);
        CHECK(ok);
        CHECK(sr.isTrained());
    }
}

// ============================================================================
// Registry integration
// ============================================================================

TEST_CASE("SoftmaxRegressionOperation - registry integration", "[MLCore][SoftmaxRegression]") {
    MLModelRegistry const registry;

    SECTION("model is registered as built-in") {
        CHECK(registry.hasModel("Softmax Regression"));
    }

    SECTION("found under MultiClassClassification task") {
        auto names = registry.getModelNames(MLTaskType::MultiClassClassification);
        CHECK(std::find(names.begin(), names.end(), "Softmax Regression") != names.end());
    }

    SECTION("not listed under BinaryClassification task") {
        auto names = registry.getModelNames(MLTaskType::BinaryClassification);
        CHECK(std::find(names.begin(), names.end(), "Softmax Regression") == names.end());
    }

    SECTION("not listed under Clustering task") {
        auto names = registry.getModelNames(MLTaskType::Clustering);
        CHECK(std::find(names.begin(), names.end(), "Softmax Regression") == names.end());
    }

    SECTION("create returns a fresh instance") {
        auto model = registry.create("Softmax Regression");
        REQUIRE(model != nullptr);
        CHECK(model->getName() == "Softmax Regression");
        CHECK_FALSE(model->isTrained());
    }

    SECTION("created instances are independent") {
        auto model1 = registry.create("Softmax Regression");
        auto model2 = registry.create("Softmax Regression");
        REQUIRE(model1 != nullptr);
        REQUIRE(model2 != nullptr);

        auto data = makeMultiClassData(3, 30);
        model1->train(data.features, data.labels, nullptr);

        CHECK(model1->isTrained());
        CHECK_FALSE(model2->isTrained());
    }
}
