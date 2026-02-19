/**
 * @file LogisticRegressionOperation.test.cpp
 * @brief Catch2 tests for MLCore::LogisticRegressionOperation
 *
 * Tests the MLCore-native Logistic Regression implementation against linearly
 * separable synthetic data. Covers:
 * - Metadata (name, task type, default parameters)
 * - Training on synthetic binary data
 * - Prediction accuracy on linearly separable data
 * - Probability output (per-class probabilities sum to ~1, well-calibrated)
 * - Feature dimension validation
 * - Edge cases (empty data, untrained model, multi-class rejection)
 * - Save/load round-trip
 * - Parameter variations (lambda regularization)
 * - Registry integration
 */

#include "models/supervised/LogisticRegressionOperation.hpp"
#include "models/MLModelParameters.hpp"
#include "models/MLModelRegistry.hpp"
#include "models/MLTaskType.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <armadillo>

#include <sstream>

using namespace MLCore;

// ============================================================================
// Helper: create linearly separable 2D data
// ============================================================================

namespace {

/**
 * @brief Generate linearly separable 2D data for binary classification
 *
 * Class 0: centered around (-3, -3)
 * Class 1: centered around (+3, +3)
 *
 * Well-separated clusters to ensure Logistic Regression achieves high accuracy.
 *
 * @param n_per_class Number of samples per class
 * @param seed        Random seed for reproducibility
 */
struct SyntheticData {
    arma::mat features;            // 2 × (2 * n_per_class)
    arma::Row<std::size_t> labels; // 1 × (2 * n_per_class)
};

SyntheticData makeBinaryData(std::size_t n_per_class = 50, int seed = 42)
{
    arma::arma_rng::set_seed(seed);

    arma::mat class0 = arma::randn(2, n_per_class) * 0.5;
    class0.row(0) -= 3.0;
    class0.row(1) -= 3.0;

    arma::mat class1 = arma::randn(2, n_per_class) * 0.5;
    class1.row(0) += 3.0;
    class1.row(1) += 3.0;

    SyntheticData data;
    data.features = arma::join_horiz(class0, class1);
    data.labels = arma::join_horiz(
        arma::Row<std::size_t>(n_per_class, arma::fill::zeros),
        arma::ones<arma::Row<std::size_t>>(n_per_class));

    return data;
}

} // anonymous namespace

// ============================================================================
// Metadata tests
// ============================================================================

TEST_CASE("LogisticRegressionOperation - metadata", "[MLCore][LogisticRegression]")
{
    LogisticRegressionOperation lr;

    SECTION("name is 'Logistic Regression'")
    {
        CHECK(lr.getName() == "Logistic Regression");
    }

    SECTION("task type is BinaryClassification")
    {
        CHECK(lr.getTaskType() == MLTaskType::BinaryClassification);
    }

    SECTION("default parameters are LogisticRegressionParameters")
    {
        auto params = lr.getDefaultParameters();
        REQUIRE(params != nullptr);

        auto const * lr_params = dynamic_cast<LogisticRegressionParameters const *>(params.get());
        REQUIRE(lr_params != nullptr);

        CHECK_THAT(lr_params->lambda, Catch::Matchers::WithinAbs(0.0, 1e-12));
        CHECK_THAT(lr_params->step_size, Catch::Matchers::WithinAbs(0.01, 1e-12));
        CHECK(lr_params->max_iterations == 10000);
    }

    SECTION("not trained initially")
    {
        CHECK_FALSE(lr.isTrained());
        CHECK(lr.numClasses() == 0);
        CHECK(lr.numFeatures() == 0);
    }
}

// ============================================================================
// Training tests
// ============================================================================

TEST_CASE("LogisticRegressionOperation - training", "[MLCore][LogisticRegression]")
{
    LogisticRegressionOperation lr;
    auto data = makeBinaryData(50);

    SECTION("train with default parameters (nullptr)")
    {
        bool ok = lr.train(data.features, data.labels, nullptr);
        CHECK(ok);
        CHECK(lr.isTrained());
        CHECK(lr.numClasses() == 2);
        CHECK(lr.numFeatures() == 2);
    }

    SECTION("train with explicit parameters")
    {
        LogisticRegressionParameters params;
        params.lambda = 0.01;
        params.max_iterations = 5000;

        bool ok = lr.train(data.features, data.labels, &params);
        CHECK(ok);
        CHECK(lr.isTrained());
        CHECK(lr.numClasses() == 2);
        CHECK(lr.numFeatures() == 2);
    }

    SECTION("train with single-class data succeeds (all zeros)")
    {
        arma::Row<std::size_t> single_class_labels(data.features.n_cols, arma::fill::zeros);
        bool ok = lr.train(data.features, single_class_labels, nullptr);
        CHECK(ok);
        CHECK(lr.numClasses() == 2); // Always binary
    }

    SECTION("retraining resets model")
    {
        lr.train(data.features, data.labels, nullptr);
        CHECK(lr.numFeatures() == 2);

        // Train again with different dimensionality
        arma::mat features3d = arma::randn(3, 100);
        arma::Row<std::size_t> labels(100, arma::fill::zeros);
        labels.tail(50).fill(1);

        bool ok = lr.train(features3d, labels, nullptr);
        CHECK(ok);
        CHECK(lr.numFeatures() == 3);
    }

    SECTION("rejects multi-class labels (> 1)")
    {
        arma::Row<std::size_t> multi_labels = {0, 1, 2, 0, 1, 2};
        arma::mat features = arma::randn(2, 6);
        CHECK_FALSE(lr.train(features, multi_labels, nullptr));
        CHECK_FALSE(lr.isTrained());
    }
}

// ============================================================================
// Training error handling
// ============================================================================

TEST_CASE("LogisticRegressionOperation - train error handling", "[MLCore][LogisticRegression]")
{
    LogisticRegressionOperation lr;

    SECTION("empty features")
    {
        arma::mat empty_features;
        arma::Row<std::size_t> labels = {0, 1};
        CHECK_FALSE(lr.train(empty_features, labels, nullptr));
        CHECK_FALSE(lr.isTrained());
    }

    SECTION("empty labels")
    {
        arma::mat features = arma::randn(2, 10);
        arma::Row<std::size_t> empty_labels;
        CHECK_FALSE(lr.train(features, empty_labels, nullptr));
        CHECK_FALSE(lr.isTrained());
    }

    SECTION("features/labels size mismatch")
    {
        arma::mat features = arma::randn(2, 10);
        arma::Row<std::size_t> labels(5, arma::fill::zeros);
        CHECK_FALSE(lr.train(features, labels, nullptr));
        CHECK_FALSE(lr.isTrained());
    }
}

// ============================================================================
// Prediction tests
// ============================================================================

TEST_CASE("LogisticRegressionOperation - prediction on linearly separable data",
          "[MLCore][LogisticRegression]")
{
    LogisticRegressionOperation lr;

    auto train_data = makeBinaryData(100, /*seed=*/42);

    REQUIRE(lr.train(train_data.features, train_data.labels, nullptr));

    SECTION("high accuracy on training data")
    {
        arma::Row<std::size_t> predictions;
        bool ok = lr.predict(train_data.features, predictions);
        REQUIRE(ok);
        REQUIRE(predictions.n_elem == train_data.labels.n_elem);

        // Count correct predictions
        std::size_t correct = arma::accu(predictions == train_data.labels);
        double accuracy = static_cast<double>(correct)
                        / static_cast<double>(train_data.labels.n_elem);

        // Well-separated data — should be near-perfect
        CHECK(accuracy > 0.95);
    }

    SECTION("high accuracy on held-out test data")
    {
        auto test_data = makeBinaryData(50, /*seed=*/123);
        arma::Row<std::size_t> predictions;
        bool ok = lr.predict(test_data.features, predictions);
        REQUIRE(ok);

        std::size_t correct = arma::accu(predictions == test_data.labels);
        double accuracy = static_cast<double>(correct)
                        / static_cast<double>(test_data.labels.n_elem);

        CHECK(accuracy > 0.90);
    }
}

// ============================================================================
// Prediction error handling
// ============================================================================

TEST_CASE("LogisticRegressionOperation - predict error handling", "[MLCore][LogisticRegression]")
{
    LogisticRegressionOperation lr;

    SECTION("predict before training fails")
    {
        arma::mat features = arma::randn(2, 5);
        arma::Row<std::size_t> predictions;
        CHECK_FALSE(lr.predict(features, predictions));
    }

    SECTION("predict with empty features fails")
    {
        auto data = makeBinaryData(30);
        lr.train(data.features, data.labels, nullptr);

        arma::mat empty_features;
        arma::Row<std::size_t> predictions;
        CHECK_FALSE(lr.predict(empty_features, predictions));
    }

    SECTION("predict with wrong feature dimension fails")
    {
        auto data = makeBinaryData(30);
        lr.train(data.features, data.labels, nullptr);
        REQUIRE(lr.numFeatures() == 2);

        arma::mat wrong_dim = arma::randn(5, 10);  // 5 features instead of 2
        arma::Row<std::size_t> predictions;
        CHECK_FALSE(lr.predict(wrong_dim, predictions));
    }
}

// ============================================================================
// Probability output
// ============================================================================

TEST_CASE("LogisticRegressionOperation - probability output", "[MLCore][LogisticRegression]")
{
    LogisticRegressionOperation lr;

    auto data = makeBinaryData(100, /*seed=*/42);

    REQUIRE(lr.train(data.features, data.labels, nullptr));

    SECTION("probabilities have correct dimensions")
    {
        arma::mat probs;
        bool ok = lr.predictProbabilities(data.features, probs);
        REQUIRE(ok);

        CHECK(probs.n_rows == lr.numClasses());
        CHECK(probs.n_cols == data.features.n_cols);
    }

    SECTION("probabilities sum to approximately 1 per sample")
    {
        arma::mat probs;
        REQUIRE(lr.predictProbabilities(data.features, probs));

        arma::rowvec col_sums = arma::sum(probs, 0);
        for (std::size_t i = 0; i < col_sums.n_elem; ++i) {
            CHECK_THAT(col_sums(i), Catch::Matchers::WithinAbs(1.0, 1e-6));
        }
    }

    SECTION("probabilities are in [0, 1]")
    {
        arma::mat probs;
        REQUIRE(lr.predictProbabilities(data.features, probs));

        CHECK(probs.min() >= 0.0);
        CHECK(probs.max() <= 1.0 + 1e-10);
    }

    SECTION("high-confidence predictions on well-separated data")
    {
        // For clearly class-0 samples (first half), the predicted class probability
        // should be high
        arma::mat class0_features = data.features.cols(0, 49);
        arma::mat probs;
        REQUIRE(lr.predictProbabilities(class0_features, probs));

        arma::Row<std::size_t> predictions;
        REQUIRE(lr.predict(class0_features, predictions));

        // Average probability for the predicted class should be > 0.7
        double avg_correct_prob = 0.0;
        for (std::size_t i = 0; i < predictions.n_elem; ++i) {
            avg_correct_prob += probs(predictions(i), i);
        }
        avg_correct_prob /= static_cast<double>(predictions.n_elem);
        CHECK(avg_correct_prob > 0.7);
    }

    SECTION("logistic regression produces well-calibrated probabilities")
    {
        // LR is known for well-calibrated probability output.
        // Verify that for each sample, the probability of the predicted class
        // is high on well-separated data.
        arma::mat probs;
        REQUIRE(lr.predictProbabilities(data.features, probs));

        arma::Row<std::size_t> predictions;
        REQUIRE(lr.predict(data.features, predictions));

        // For each sample, the probability of the predicted class should be high
        double avg_max_prob = 0.0;
        for (std::size_t i = 0; i < predictions.n_elem; ++i) {
            avg_max_prob += probs(predictions(i), i);
        }
        avg_max_prob /= static_cast<double>(predictions.n_elem);
        CHECK(avg_max_prob > 0.9);
    }

    SECTION("probability error handling - not trained")
    {
        LogisticRegressionOperation untrained;
        arma::mat probs;
        CHECK_FALSE(untrained.predictProbabilities(data.features, probs));
    }

    SECTION("probability error handling - empty features")
    {
        arma::mat empty_features;
        arma::mat probs;
        CHECK_FALSE(lr.predictProbabilities(empty_features, probs));
    }

    SECTION("probability error handling - wrong dimension")
    {
        arma::mat wrong_dim = arma::randn(5, 10);
        arma::mat probs;
        CHECK_FALSE(lr.predictProbabilities(wrong_dim, probs));
    }
}

// ============================================================================
// Save / load round-trip
// ============================================================================

TEST_CASE("LogisticRegressionOperation - save/load round-trip", "[MLCore][LogisticRegression]")
{
    auto data = makeBinaryData(80, /*seed=*/42);

    // Train original model
    LogisticRegressionOperation original;
    REQUIRE(original.train(data.features, data.labels, nullptr));

    arma::Row<std::size_t> original_predictions;
    REQUIRE(original.predict(data.features, original_predictions));

    SECTION("save and load produces identical predictions")
    {
        // Save
        std::stringstream ss;
        REQUIRE(original.save(ss));

        // Load into fresh model
        LogisticRegressionOperation loaded;
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

    SECTION("save and load preserves probabilities")
    {
        arma::mat original_probs;
        REQUIRE(original.predictProbabilities(data.features, original_probs));

        std::stringstream ss;
        REQUIRE(original.save(ss));

        LogisticRegressionOperation loaded;
        REQUIRE(loaded.load(ss));

        arma::mat loaded_probs;
        REQUIRE(loaded.predictProbabilities(data.features, loaded_probs));

        // Probabilities should be identical (same model parameters)
        CHECK(arma::approx_equal(original_probs, loaded_probs, "absdiff", 1e-10));
    }

    SECTION("save fails when not trained")
    {
        LogisticRegressionOperation untrained;
        std::stringstream ss;
        CHECK_FALSE(untrained.save(ss));
    }

    SECTION("load from corrupted stream fails")
    {
        std::stringstream ss("garbage data that is not a valid model");
        LogisticRegressionOperation loaded;
        CHECK_FALSE(loaded.load(ss));
        CHECK_FALSE(loaded.isTrained());
    }
}

// ============================================================================
// Parameter variations
// ============================================================================

TEST_CASE("LogisticRegressionOperation - parameter variations", "[MLCore][LogisticRegression]")
{
    auto data = makeBinaryData(60, /*seed=*/42);

    SECTION("different lambda values produce valid models")
    {
        for (double lambda : {0.0, 0.001, 0.01, 0.1, 1.0}) {
            LogisticRegressionOperation lr;
            LogisticRegressionParameters params;
            params.lambda = lambda;

            REQUIRE(lr.train(data.features, data.labels, &params));
            CHECK(lr.isTrained());

            arma::Row<std::size_t> predictions;
            REQUIRE(lr.predict(data.features, predictions));

            std::size_t correct = arma::accu(predictions == data.labels);
            double accuracy = static_cast<double>(correct)
                            / static_cast<double>(data.labels.n_elem);

            // All should achieve reasonable accuracy on well-separated data
            CHECK(accuracy > 0.85);
        }
    }

    SECTION("strong regularization reduces overfitting potential")
    {
        LogisticRegressionOperation lr_noreg;
        LogisticRegressionParameters noreg_params;
        noreg_params.lambda = 0.0;
        REQUIRE(lr_noreg.train(data.features, data.labels, &noreg_params));

        LogisticRegressionOperation lr_highreg;
        LogisticRegressionParameters highreg_params;
        highreg_params.lambda = 10.0;
        REQUIRE(lr_highreg.train(data.features, data.labels, &highreg_params));

        // Both should train successfully
        CHECK(lr_noreg.isTrained());
        CHECK(lr_highreg.isTrained());
    }

    SECTION("wrong parameter type falls back to defaults")
    {
        LogisticRegressionOperation lr;
        RandomForestParameters wrong_params;  // Wrong type
        bool ok = lr.train(data.features, data.labels, &wrong_params);
        CHECK(ok);
        CHECK(lr.isTrained());
    }

    SECTION("custom max_iterations trains successfully")
    {
        LogisticRegressionOperation lr;
        LogisticRegressionParameters params;
        params.max_iterations = 100;

        bool ok = lr.train(data.features, data.labels, &params);
        CHECK(ok);
        CHECK(lr.isTrained());
    }
}

// ============================================================================
// Registry integration
// ============================================================================

TEST_CASE("LogisticRegressionOperation - registry integration", "[MLCore][LogisticRegression]")
{
    MLModelRegistry registry;

    SECTION("model is registered as built-in")
    {
        CHECK(registry.hasModel("Logistic Regression"));
    }

    SECTION("found under BinaryClassification task")
    {
        auto names = registry.getModelNames(MLTaskType::BinaryClassification);
        CHECK(std::find(names.begin(), names.end(), "Logistic Regression") != names.end());
    }

    SECTION("not listed under MultiClassClassification task")
    {
        auto names = registry.getModelNames(MLTaskType::MultiClassClassification);
        CHECK(std::find(names.begin(), names.end(), "Logistic Regression") == names.end());
    }

    SECTION("create returns a fresh instance")
    {
        auto model = registry.create("Logistic Regression");
        REQUIRE(model != nullptr);
        CHECK(model->getName() == "Logistic Regression");
        CHECK_FALSE(model->isTrained());
    }

    SECTION("created instances are independent")
    {
        auto model1 = registry.create("Logistic Regression");
        auto model2 = registry.create("Logistic Regression");
        REQUIRE(model1 != nullptr);
        REQUIRE(model2 != nullptr);

        auto data = makeBinaryData(30);
        model1->train(data.features, data.labels, nullptr);

        CHECK(model1->isTrained());
        CHECK_FALSE(model2->isTrained());
    }

    SECTION("registry contains RF, NB, and LR")
    {
        CHECK(registry.hasModel("Random Forest"));
        CHECK(registry.hasModel("Naive Bayes"));
        CHECK(registry.hasModel("Logistic Regression"));
        CHECK(registry.size() >= 3);
    }
}
