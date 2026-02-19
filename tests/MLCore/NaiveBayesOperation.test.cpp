/**
 * @file NaiveBayesOperation.test.cpp
 * @brief Catch2 tests for MLCore::NaiveBayesOperation
 *
 * Tests the MLCore-native Naive Bayes implementation against linearly
 * separable synthetic data. Covers:
 * - Metadata (name, task type, default parameters)
 * - Training on synthetic data
 * - Prediction accuracy on linearly separable data
 * - Probability output (per-class probabilities sum to ~1)
 * - Feature dimension validation
 * - Edge cases (empty data, untrained model)
 * - Save/load round-trip
 * - Multi-class classification
 * - Registry integration
 */

#include "models/supervised/NaiveBayesOperation.hpp"
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
 * Well-separated clusters to ensure Naive Bayes achieves high accuracy
 * (Gaussian assumption is well-suited here).
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

/**
 * @brief Generate 3-class data (well-separated clusters)
 *
 * Class 0: centered around (-4, 0)
 * Class 1: centered around (+4, 0)
 * Class 2: centered around (0, +4)
 */
SyntheticData makeMultiClassData(std::size_t n_per_class = 40, int seed = 42)
{
    arma::arma_rng::set_seed(seed);

    arma::mat class0 = arma::randn(2, n_per_class) * 0.3;
    class0.row(0) -= 4.0;

    arma::mat class1 = arma::randn(2, n_per_class) * 0.3;
    class1.row(0) += 4.0;

    arma::mat class2 = arma::randn(2, n_per_class) * 0.3;
    class2.row(1) += 4.0;

    SyntheticData data;
    data.features = arma::join_horiz(arma::join_horiz(class0, class1), class2);

    arma::Row<std::size_t> labels0(n_per_class, arma::fill::zeros);
    arma::Row<std::size_t> labels1(n_per_class);
    labels1.fill(1);
    arma::Row<std::size_t> labels2(n_per_class);
    labels2.fill(2);
    data.labels = arma::join_horiz(arma::join_horiz(labels0, labels1), labels2);

    return data;
}

} // anonymous namespace

// ============================================================================
// Metadata tests
// ============================================================================

TEST_CASE("NaiveBayesOperation - metadata", "[MLCore][NaiveBayes]")
{
    NaiveBayesOperation nb;

    SECTION("name is 'Naive Bayes'")
    {
        CHECK(nb.getName() == "Naive Bayes");
    }

    SECTION("task type is MultiClassClassification")
    {
        CHECK(nb.getTaskType() == MLTaskType::MultiClassClassification);
    }

    SECTION("default parameters are NaiveBayesParameters")
    {
        auto params = nb.getDefaultParameters();
        REQUIRE(params != nullptr);

        auto const * nb_params = dynamic_cast<NaiveBayesParameters const *>(params.get());
        REQUIRE(nb_params != nullptr);

        CHECK_THAT(nb_params->epsilon,
                   Catch::Matchers::WithinAbs(1e-9, 1e-12));
    }

    SECTION("not trained initially")
    {
        CHECK_FALSE(nb.isTrained());
        CHECK(nb.numClasses() == 0);
        CHECK(nb.numFeatures() == 0);
    }
}

// ============================================================================
// Training tests
// ============================================================================

TEST_CASE("NaiveBayesOperation - training", "[MLCore][NaiveBayes]")
{
    NaiveBayesOperation nb;
    auto data = makeBinaryData(50);

    SECTION("train with default parameters (nullptr)")
    {
        bool ok = nb.train(data.features, data.labels, nullptr);
        CHECK(ok);
        CHECK(nb.isTrained());
        CHECK(nb.numClasses() == 2);
        CHECK(nb.numFeatures() == 2);
    }

    SECTION("train with explicit parameters")
    {
        NaiveBayesParameters params;
        params.epsilon = 1e-6;

        bool ok = nb.train(data.features, data.labels, &params);
        CHECK(ok);
        CHECK(nb.isTrained());
        CHECK(nb.numClasses() == 2);
        CHECK(nb.numFeatures() == 2);
    }

    SECTION("train with zero epsilon succeeds")
    {
        NaiveBayesParameters params;
        params.epsilon = 0.0;

        bool ok = nb.train(data.features, data.labels, &params);
        CHECK(ok);
        CHECK(nb.isTrained());
    }

    SECTION("train with single-class data still succeeds (numClasses forced to 2)")
    {
        arma::Row<std::size_t> single_class_labels(data.features.n_cols, arma::fill::zeros);
        bool ok = nb.train(data.features, single_class_labels, nullptr);
        CHECK(ok);
        CHECK(nb.numClasses() == 2);
    }

    SECTION("retraining resets model")
    {
        nb.train(data.features, data.labels, nullptr);
        CHECK(nb.numFeatures() == 2);

        // Train again with different dimensionality
        arma::mat features3d = arma::randn(3, 100);
        arma::Row<std::size_t> labels(100, arma::fill::zeros);
        labels.tail(50).fill(1);

        bool ok = nb.train(features3d, labels, nullptr);
        CHECK(ok);
        CHECK(nb.numFeatures() == 3);
    }
}

// ============================================================================
// Training error handling
// ============================================================================

TEST_CASE("NaiveBayesOperation - train error handling", "[MLCore][NaiveBayes]")
{
    NaiveBayesOperation nb;

    SECTION("empty features")
    {
        arma::mat empty_features;
        arma::Row<std::size_t> labels = {0, 1};
        CHECK_FALSE(nb.train(empty_features, labels, nullptr));
        CHECK_FALSE(nb.isTrained());
    }

    SECTION("empty labels")
    {
        arma::mat features = arma::randn(2, 10);
        arma::Row<std::size_t> empty_labels;
        CHECK_FALSE(nb.train(features, empty_labels, nullptr));
        CHECK_FALSE(nb.isTrained());
    }

    SECTION("features/labels size mismatch")
    {
        arma::mat features = arma::randn(2, 10);
        arma::Row<std::size_t> labels(5, arma::fill::zeros);
        CHECK_FALSE(nb.train(features, labels, nullptr));
        CHECK_FALSE(nb.isTrained());
    }
}

// ============================================================================
// Prediction tests
// ============================================================================

TEST_CASE("NaiveBayesOperation - prediction on linearly separable data",
          "[MLCore][NaiveBayes]")
{
    NaiveBayesOperation nb;

    auto train_data = makeBinaryData(100, /*seed=*/42);

    REQUIRE(nb.train(train_data.features, train_data.labels, nullptr));

    SECTION("high accuracy on training data")
    {
        arma::Row<std::size_t> predictions;
        bool ok = nb.predict(train_data.features, predictions);
        REQUIRE(ok);
        REQUIRE(predictions.n_elem == train_data.labels.n_elem);

        // Count correct predictions
        std::size_t correct = arma::accu(predictions == train_data.labels);
        double accuracy = static_cast<double>(correct)
                        / static_cast<double>(train_data.labels.n_elem);

        // Well-separated Gaussian data — NB should excel here
        CHECK(accuracy > 0.95);
    }

    SECTION("high accuracy on held-out test data")
    {
        auto test_data = makeBinaryData(50, /*seed=*/123);
        arma::Row<std::size_t> predictions;
        bool ok = nb.predict(test_data.features, predictions);
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

TEST_CASE("NaiveBayesOperation - predict error handling", "[MLCore][NaiveBayes]")
{
    NaiveBayesOperation nb;

    SECTION("predict before training fails")
    {
        arma::mat features = arma::randn(2, 5);
        arma::Row<std::size_t> predictions;
        CHECK_FALSE(nb.predict(features, predictions));
    }

    SECTION("predict with empty features fails")
    {
        auto data = makeBinaryData(30);
        nb.train(data.features, data.labels, nullptr);

        arma::mat empty_features;
        arma::Row<std::size_t> predictions;
        CHECK_FALSE(nb.predict(empty_features, predictions));
    }

    SECTION("predict with wrong feature dimension fails")
    {
        auto data = makeBinaryData(30);
        nb.train(data.features, data.labels, nullptr);
        REQUIRE(nb.numFeatures() == 2);

        arma::mat wrong_dim = arma::randn(5, 10);  // 5 features instead of 2
        arma::Row<std::size_t> predictions;
        CHECK_FALSE(nb.predict(wrong_dim, predictions));
    }
}

// ============================================================================
// Probability output
// ============================================================================

TEST_CASE("NaiveBayesOperation - probability output", "[MLCore][NaiveBayes]")
{
    NaiveBayesOperation nb;

    auto data = makeBinaryData(100, /*seed=*/42);

    REQUIRE(nb.train(data.features, data.labels, nullptr));

    SECTION("probabilities have correct dimensions")
    {
        arma::mat probs;
        bool ok = nb.predictProbabilities(data.features, probs);
        REQUIRE(ok);

        CHECK(probs.n_rows == nb.numClasses());
        CHECK(probs.n_cols == data.features.n_cols);
    }

    SECTION("probabilities sum to approximately 1 per sample")
    {
        arma::mat probs;
        REQUIRE(nb.predictProbabilities(data.features, probs));

        arma::rowvec col_sums = arma::sum(probs, 0);
        for (std::size_t i = 0; i < col_sums.n_elem; ++i) {
            CHECK_THAT(col_sums(i), Catch::Matchers::WithinAbs(1.0, 1e-6));
        }
    }

    SECTION("probabilities are in [0, 1]")
    {
        arma::mat probs;
        REQUIRE(nb.predictProbabilities(data.features, probs));

        CHECK(probs.min() >= 0.0);
        CHECK(probs.max() <= 1.0 + 1e-10);
    }

    SECTION("high-confidence predictions on well-separated data")
    {
        // For clearly class-0 samples (first half), class-0 probability should be high
        arma::mat class0_features = data.features.cols(0, 49);
        arma::mat probs;
        REQUIRE(nb.predictProbabilities(class0_features, probs));

        // Average probability for the correct class should be > 0.7
        double avg_correct_prob = arma::mean(probs.row(0));
        CHECK(avg_correct_prob > 0.7);
    }

    SECTION("probability error handling - not trained")
    {
        NaiveBayesOperation untrained;
        arma::mat probs;
        CHECK_FALSE(untrained.predictProbabilities(data.features, probs));
    }

    SECTION("probability error handling - empty features")
    {
        arma::mat empty_features;
        arma::mat probs;
        CHECK_FALSE(nb.predictProbabilities(empty_features, probs));
    }

    SECTION("probability error handling - wrong dimension")
    {
        arma::mat wrong_dim = arma::randn(5, 10);
        arma::mat probs;
        CHECK_FALSE(nb.predictProbabilities(wrong_dim, probs));
    }
}

// ============================================================================
// Multi-class classification
// ============================================================================

TEST_CASE("NaiveBayesOperation - multi-class classification", "[MLCore][NaiveBayes]")
{
    NaiveBayesOperation nb;

    auto data = makeMultiClassData(60, /*seed=*/42);

    REQUIRE(nb.train(data.features, data.labels, nullptr));

    SECTION("detects 3 classes")
    {
        CHECK(nb.numClasses() == 3);
    }

    SECTION("high accuracy on well-separated 3-class data")
    {
        arma::Row<std::size_t> predictions;
        REQUIRE(nb.predict(data.features, predictions));

        std::size_t correct = arma::accu(predictions == data.labels);
        double accuracy = static_cast<double>(correct)
                        / static_cast<double>(data.labels.n_elem);

        CHECK(accuracy > 0.90);
    }

    SECTION("probabilities have 3 rows")
    {
        arma::mat probs;
        REQUIRE(nb.predictProbabilities(data.features, probs));

        CHECK(probs.n_rows == 3);
        CHECK(probs.n_cols == data.features.n_cols);
    }
}

// ============================================================================
// Save / load round-trip
// ============================================================================

TEST_CASE("NaiveBayesOperation - save/load round-trip", "[MLCore][NaiveBayes]")
{
    auto data = makeBinaryData(80, /*seed=*/42);

    // Train original model
    NaiveBayesOperation original;
    REQUIRE(original.train(data.features, data.labels, nullptr));

    arma::Row<std::size_t> original_predictions;
    REQUIRE(original.predict(data.features, original_predictions));

    SECTION("save and load produces identical predictions")
    {
        // Save
        std::stringstream ss;
        REQUIRE(original.save(ss));

        // Load into fresh model
        NaiveBayesOperation loaded;
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

        NaiveBayesOperation loaded;
        REQUIRE(loaded.load(ss));

        arma::mat loaded_probs;
        REQUIRE(loaded.predictProbabilities(data.features, loaded_probs));

        // Probabilities should be identical (same model parameters)
        CHECK(arma::approx_equal(original_probs, loaded_probs, "absdiff", 1e-10));
    }

    SECTION("save fails when not trained")
    {
        NaiveBayesOperation untrained;
        std::stringstream ss;
        CHECK_FALSE(untrained.save(ss));
    }

    SECTION("load from corrupted stream fails")
    {
        std::stringstream ss("garbage data that is not a valid model");
        NaiveBayesOperation loaded;
        CHECK_FALSE(loaded.load(ss));
        CHECK_FALSE(loaded.isTrained());
    }
}

// ============================================================================
// Parameter variations
// ============================================================================

TEST_CASE("NaiveBayesOperation - parameter variations", "[MLCore][NaiveBayes]")
{
    auto data = makeBinaryData(60, /*seed=*/42);

    SECTION("different epsilon values produce valid models")
    {
        for (double eps : {0.0, 1e-12, 1e-9, 1e-6, 1e-3}) {
            NaiveBayesOperation nb;
            NaiveBayesParameters params;
            params.epsilon = eps;

            REQUIRE(nb.train(data.features, data.labels, &params));
            CHECK(nb.isTrained());

            arma::Row<std::size_t> predictions;
            REQUIRE(nb.predict(data.features, predictions));

            std::size_t correct = arma::accu(predictions == data.labels);
            double accuracy = static_cast<double>(correct)
                            / static_cast<double>(data.labels.n_elem);

            // All should achieve reasonable accuracy on well-separated data
            CHECK(accuracy > 0.85);
        }
    }

    SECTION("wrong parameter type falls back to defaults")
    {
        NaiveBayesOperation nb;
        RandomForestParameters wrong_params;  // Wrong type
        bool ok = nb.train(data.features, data.labels, &wrong_params);
        CHECK(ok);
        CHECK(nb.isTrained());
    }
}

// ============================================================================
// Registry integration
// ============================================================================

TEST_CASE("NaiveBayesOperation - registry integration", "[MLCore][NaiveBayes]")
{
    MLModelRegistry registry;

    SECTION("model is registered as built-in")
    {
        CHECK(registry.hasModel("Naive Bayes"));
    }

    SECTION("found under MultiClassClassification task")
    {
        auto names = registry.getModelNames(MLTaskType::MultiClassClassification);
        CHECK(std::find(names.begin(), names.end(), "Naive Bayes") != names.end());
    }

    SECTION("create returns a fresh instance")
    {
        auto model = registry.create("Naive Bayes");
        REQUIRE(model != nullptr);
        CHECK(model->getName() == "Naive Bayes");
        CHECK_FALSE(model->isTrained());
    }

    SECTION("created instances are independent")
    {
        auto model1 = registry.create("Naive Bayes");
        auto model2 = registry.create("Naive Bayes");
        REQUIRE(model1 != nullptr);
        REQUIRE(model2 != nullptr);

        auto data = makeBinaryData(30);
        model1->train(data.features, data.labels, nullptr);

        CHECK(model1->isTrained());
        CHECK_FALSE(model2->isTrained());
    }

    SECTION("registry contains both Random Forest and Naive Bayes")
    {
        CHECK(registry.hasModel("Random Forest"));
        CHECK(registry.hasModel("Naive Bayes"));
        CHECK(registry.size() >= 2);
    }
}
