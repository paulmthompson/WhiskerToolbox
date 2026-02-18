/**
 * @file RandomForestOperation.test.cpp
 * @brief Catch2 tests for MLCore::RandomForestOperation
 *
 * Tests the MLCore-native Random Forest implementation against linearly
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

#include "models/supervised/RandomForestOperation.hpp"
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
 * Class 0: centered around (-2, -2)
 * Class 1: centered around (+2, +2)
 *
 * @param n_per_class Number of samples per class
 * @param seed        Random seed for reproducibility
 */
struct SyntheticData {
    arma::mat features;          // 2 × (2 * n_per_class)
    arma::Row<std::size_t> labels; // 1 × (2 * n_per_class)
};

SyntheticData makeBinaryData(std::size_t n_per_class = 50, int seed = 42)
{
    arma::arma_rng::set_seed(seed);

    arma::mat class0 = arma::randn(2, n_per_class) * 0.5;
    class0.row(0) -= 2.0;
    class0.row(1) -= 2.0;

    arma::mat class1 = arma::randn(2, n_per_class) * 0.5;
    class1.row(0) += 2.0;
    class1.row(1) += 2.0;

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
 * Class 0: centered around (-3, 0)
 * Class 1: centered around (+3, 0)
 * Class 2: centered around (0, +3)
 */
SyntheticData makeMultiClassData(std::size_t n_per_class = 40, int seed = 42)
{
    arma::arma_rng::set_seed(seed);

    arma::mat class0 = arma::randn(2, n_per_class) * 0.3;
    class0.row(0) -= 3.0;

    arma::mat class1 = arma::randn(2, n_per_class) * 0.3;
    class1.row(0) += 3.0;

    arma::mat class2 = arma::randn(2, n_per_class) * 0.3;
    class2.row(1) += 3.0;

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

TEST_CASE("RandomForestOperation - metadata", "[MLCore][RandomForest]")
{
    RandomForestOperation rf;

    SECTION("name is 'Random Forest'")
    {
        CHECK(rf.getName() == "Random Forest");
    }

    SECTION("task type is MultiClassClassification")
    {
        CHECK(rf.getTaskType() == MLTaskType::MultiClassClassification);
    }

    SECTION("default parameters are RandomForestParameters")
    {
        auto params = rf.getDefaultParameters();
        REQUIRE(params != nullptr);

        auto const * rf_params = dynamic_cast<RandomForestParameters const *>(params.get());
        REQUIRE(rf_params != nullptr);

        CHECK(rf_params->num_trees == 10);
        CHECK(rf_params->minimum_leaf_size == 1);
        CHECK(rf_params->maximum_depth == 0);
        CHECK_THAT(rf_params->minimum_gain_split,
                   Catch::Matchers::WithinAbs(1e-7, 1e-10));
    }

    SECTION("not trained initially")
    {
        CHECK_FALSE(rf.isTrained());
        CHECK(rf.numClasses() == 0);
        CHECK(rf.numFeatures() == 0);
    }
}

// ============================================================================
// Training tests
// ============================================================================

TEST_CASE("RandomForestOperation - training", "[MLCore][RandomForest]")
{
    RandomForestOperation rf;
    auto data = makeBinaryData(50);

    SECTION("train with default parameters (nullptr)")
    {
        bool ok = rf.train(data.features, data.labels, nullptr);
        CHECK(ok);
        CHECK(rf.isTrained());
        CHECK(rf.numClasses() == 2);
        CHECK(rf.numFeatures() == 2);
    }

    SECTION("train with explicit parameters")
    {
        RandomForestParameters params;
        params.num_trees = 20;
        params.minimum_leaf_size = 3;
        params.maximum_depth = 5;

        bool ok = rf.train(data.features, data.labels, &params);
        CHECK(ok);
        CHECK(rf.isTrained());
        CHECK(rf.numClasses() == 2);
        CHECK(rf.numFeatures() == 2);
    }

    SECTION("train with single-class data still succeeds (numClasses forced to 2)")
    {
        arma::Row<std::size_t> single_class_labels(data.features.n_cols, arma::fill::zeros);
        bool ok = rf.train(data.features, single_class_labels, nullptr);
        CHECK(ok);
        CHECK(rf.numClasses() == 2);
    }

    SECTION("retraining resets model")
    {
        rf.train(data.features, data.labels, nullptr);
        CHECK(rf.numFeatures() == 2);

        // Train again with different dimensionality
        arma::mat features3d = arma::randn(3, 100);
        arma::Row<std::size_t> labels(100, arma::fill::zeros);
        labels.tail(50).fill(1);

        bool ok = rf.train(features3d, labels, nullptr);
        CHECK(ok);
        CHECK(rf.numFeatures() == 3);
    }
}

// ============================================================================
// Training error handling
// ============================================================================

TEST_CASE("RandomForestOperation - train error handling", "[MLCore][RandomForest]")
{
    RandomForestOperation rf;

    SECTION("empty features")
    {
        arma::mat empty_features;
        arma::Row<std::size_t> labels = {0, 1};
        CHECK_FALSE(rf.train(empty_features, labels, nullptr));
        CHECK_FALSE(rf.isTrained());
    }

    SECTION("empty labels")
    {
        arma::mat features = arma::randn(2, 10);
        arma::Row<std::size_t> empty_labels;
        CHECK_FALSE(rf.train(features, empty_labels, nullptr));
        CHECK_FALSE(rf.isTrained());
    }

    SECTION("features/labels size mismatch")
    {
        arma::mat features = arma::randn(2, 10);
        arma::Row<std::size_t> labels(5, arma::fill::zeros);
        CHECK_FALSE(rf.train(features, labels, nullptr));
        CHECK_FALSE(rf.isTrained());
    }
}

// ============================================================================
// Prediction tests
// ============================================================================

TEST_CASE("RandomForestOperation - prediction on linearly separable data",
          "[MLCore][RandomForest]")
{
    RandomForestOperation rf;

    auto train_data = makeBinaryData(100, /*seed=*/42);
    RandomForestParameters params;
    params.num_trees = 50;

    REQUIRE(rf.train(train_data.features, train_data.labels, &params));

    SECTION("high accuracy on training data")
    {
        arma::Row<std::size_t> predictions;
        bool ok = rf.predict(train_data.features, predictions);
        REQUIRE(ok);
        REQUIRE(predictions.n_elem == train_data.labels.n_elem);

        // Count correct predictions
        std::size_t correct = arma::accu(predictions == train_data.labels);
        double accuracy = static_cast<double>(correct)
                        / static_cast<double>(train_data.labels.n_elem);

        // Linearly separable data — should be near-perfect
        CHECK(accuracy > 0.95);
    }

    SECTION("high accuracy on held-out test data")
    {
        auto test_data = makeBinaryData(50, /*seed=*/123);
        arma::Row<std::size_t> predictions;
        bool ok = rf.predict(test_data.features, predictions);
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

TEST_CASE("RandomForestOperation - predict error handling", "[MLCore][RandomForest]")
{
    RandomForestOperation rf;

    SECTION("predict before training fails")
    {
        arma::mat features = arma::randn(2, 5);
        arma::Row<std::size_t> predictions;
        CHECK_FALSE(rf.predict(features, predictions));
    }

    SECTION("predict with empty features fails")
    {
        auto data = makeBinaryData(30);
        rf.train(data.features, data.labels, nullptr);

        arma::mat empty_features;
        arma::Row<std::size_t> predictions;
        CHECK_FALSE(rf.predict(empty_features, predictions));
    }

    SECTION("predict with wrong feature dimension fails")
    {
        auto data = makeBinaryData(30);
        rf.train(data.features, data.labels, nullptr);
        REQUIRE(rf.numFeatures() == 2);

        arma::mat wrong_dim = arma::randn(5, 10);  // 5 features instead of 2
        arma::Row<std::size_t> predictions;
        CHECK_FALSE(rf.predict(wrong_dim, predictions));
    }
}

// ============================================================================
// Probability output
// ============================================================================

TEST_CASE("RandomForestOperation - probability output", "[MLCore][RandomForest]")
{
    RandomForestOperation rf;

    auto data = makeBinaryData(100, /*seed=*/42);
    RandomForestParameters params;
    params.num_trees = 50;

    REQUIRE(rf.train(data.features, data.labels, &params));

    SECTION("probabilities have correct dimensions")
    {
        arma::mat probs;
        bool ok = rf.predictProbabilities(data.features, probs);
        REQUIRE(ok);

        CHECK(probs.n_rows == rf.numClasses());
        CHECK(probs.n_cols == data.features.n_cols);
    }

    SECTION("probabilities sum to approximately 1 per sample")
    {
        arma::mat probs;
        REQUIRE(rf.predictProbabilities(data.features, probs));

        arma::rowvec col_sums = arma::sum(probs, 0);
        for (std::size_t i = 0; i < col_sums.n_elem; ++i) {
            CHECK_THAT(col_sums(i), Catch::Matchers::WithinAbs(1.0, 1e-6));
        }
    }

    SECTION("probabilities are in [0, 1]")
    {
        arma::mat probs;
        REQUIRE(rf.predictProbabilities(data.features, probs));

        CHECK(probs.min() >= 0.0);
        CHECK(probs.max() <= 1.0 + 1e-10);
    }

    SECTION("high-confidence predictions on well-separated data")
    {
        // For clearly class-0 samples (first half), class-0 probability should be high
        arma::mat class0_features = data.features.cols(0, 49);
        arma::mat probs;
        REQUIRE(rf.predictProbabilities(class0_features, probs));

        // Average probability for the correct class should be > 0.7
        double avg_correct_prob = arma::mean(probs.row(0));
        CHECK(avg_correct_prob > 0.7);
    }

    SECTION("probability error handling - not trained")
    {
        RandomForestOperation untrained;
        arma::mat probs;
        CHECK_FALSE(untrained.predictProbabilities(data.features, probs));
    }

    SECTION("probability error handling - empty features")
    {
        arma::mat empty_features;
        arma::mat probs;
        CHECK_FALSE(rf.predictProbabilities(empty_features, probs));
    }

    SECTION("probability error handling - wrong dimension")
    {
        arma::mat wrong_dim = arma::randn(5, 10);
        arma::mat probs;
        CHECK_FALSE(rf.predictProbabilities(wrong_dim, probs));
    }
}

// ============================================================================
// Multi-class classification
// ============================================================================

TEST_CASE("RandomForestOperation - multi-class classification", "[MLCore][RandomForest]")
{
    RandomForestOperation rf;

    auto data = makeMultiClassData(60, /*seed=*/42);
    RandomForestParameters params;
    params.num_trees = 50;

    REQUIRE(rf.train(data.features, data.labels, &params));

    SECTION("detects 3 classes")
    {
        CHECK(rf.numClasses() == 3);
    }

    SECTION("high accuracy on well-separated 3-class data")
    {
        arma::Row<std::size_t> predictions;
        REQUIRE(rf.predict(data.features, predictions));

        std::size_t correct = arma::accu(predictions == data.labels);
        double accuracy = static_cast<double>(correct)
                        / static_cast<double>(data.labels.n_elem);

        CHECK(accuracy > 0.90);
    }

    SECTION("probabilities have 3 rows")
    {
        arma::mat probs;
        REQUIRE(rf.predictProbabilities(data.features, probs));

        CHECK(probs.n_rows == 3);
        CHECK(probs.n_cols == data.features.n_cols);
    }
}

// ============================================================================
// Save / load round-trip
// ============================================================================

TEST_CASE("RandomForestOperation - save/load round-trip", "[MLCore][RandomForest]")
{
    auto data = makeBinaryData(80, /*seed=*/42);
    RandomForestParameters params;
    params.num_trees = 20;

    // Train original model
    RandomForestOperation original;
    REQUIRE(original.train(data.features, data.labels, &params));

    arma::Row<std::size_t> original_predictions;
    REQUIRE(original.predict(data.features, original_predictions));

    SECTION("save and load produces identical predictions")
    {
        // Save
        std::stringstream ss;
        REQUIRE(original.save(ss));

        // Load into fresh model
        RandomForestOperation loaded;
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

    SECTION("save fails when not trained")
    {
        RandomForestOperation untrained;
        std::stringstream ss;
        CHECK_FALSE(untrained.save(ss));
    }

    SECTION("load from corrupted stream fails")
    {
        std::stringstream ss("garbage data that is not a valid model");
        RandomForestOperation loaded;
        CHECK_FALSE(loaded.load(ss));
        CHECK_FALSE(loaded.isTrained());
    }
}

// ============================================================================
// Parameter variations
// ============================================================================

TEST_CASE("RandomForestOperation - parameter variations", "[MLCore][RandomForest]")
{
    auto data = makeBinaryData(60, /*seed=*/42);

    SECTION("more trees generally does not hurt accuracy")
    {
        RandomForestOperation rf_few;
        RandomForestParameters few_params;
        few_params.num_trees = 5;
        REQUIRE(rf_few.train(data.features, data.labels, &few_params));

        RandomForestOperation rf_many;
        RandomForestParameters many_params;
        many_params.num_trees = 100;
        REQUIRE(rf_many.train(data.features, data.labels, &many_params));

        arma::Row<std::size_t> pred_few, pred_many;
        rf_few.predict(data.features, pred_few);
        rf_many.predict(data.features, pred_many);

        double acc_few = static_cast<double>(arma::accu(pred_few == data.labels))
                       / static_cast<double>(data.labels.n_elem);
        double acc_many = static_cast<double>(arma::accu(pred_many == data.labels))
                        / static_cast<double>(data.labels.n_elem);

        // Both should be high on linearly separable data
        CHECK(acc_few > 0.85);
        CHECK(acc_many > 0.85);
    }

    SECTION("depth-limited tree still classifies")
    {
        RandomForestOperation rf;
        RandomForestParameters params;
        params.num_trees = 30;
        params.maximum_depth = 3;

        REQUIRE(rf.train(data.features, data.labels, &params));

        arma::Row<std::size_t> predictions;
        REQUIRE(rf.predict(data.features, predictions));

        double accuracy = static_cast<double>(arma::accu(predictions == data.labels))
                        / static_cast<double>(data.labels.n_elem);
        CHECK(accuracy > 0.85);
    }

    SECTION("large minimum leaf size still trains")
    {
        RandomForestOperation rf;
        RandomForestParameters params;
        params.num_trees = 20;
        params.minimum_leaf_size = 20;

        REQUIRE(rf.train(data.features, data.labels, &params));
        CHECK(rf.isTrained());
    }
}

// ============================================================================
// Registry integration
// ============================================================================

TEST_CASE("RandomForestOperation - registry integration", "[MLCore][RandomForest]")
{
    MLModelRegistry registry;
    registry.registerModel<RandomForestOperation>();

    SECTION("model is registered")
    {
        CHECK(registry.hasModel("Random Forest"));
    }

    SECTION("found under MultiClassClassification task")
    {
        auto names = registry.getModelNames(MLTaskType::MultiClassClassification);
        CHECK(std::find(names.begin(), names.end(), "Random Forest") != names.end());
    }

    SECTION("create returns a fresh instance")
    {
        auto model = registry.create("Random Forest");
        REQUIRE(model != nullptr);
        CHECK(model->getName() == "Random Forest");
        CHECK_FALSE(model->isTrained());
    }

    SECTION("created instances are independent")
    {
        auto model1 = registry.create("Random Forest");
        auto model2 = registry.create("Random Forest");
        REQUIRE(model1 != nullptr);
        REQUIRE(model2 != nullptr);

        auto data = makeBinaryData(30);
        model1->train(data.features, data.labels, nullptr);

        CHECK(model1->isTrained());
        CHECK_FALSE(model2->isTrained());
    }
}
