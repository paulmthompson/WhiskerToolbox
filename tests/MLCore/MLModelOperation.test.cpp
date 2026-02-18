/**
 * @file MLModelOperation.test.cpp
 * @brief Tests for MLModelOperation interface, MLTaskType, MLModelParameters,
 *        and MLModelRegistry
 *
 * Uses lightweight stub implementations to test the abstract interface contract,
 * registry lookup/creation, and parameter defaults — no actual mlpack models.
 */

#include "models/MLModelOperation.hpp"
#include "models/MLModelParameters.hpp"
#include "models/MLModelRegistry.hpp"
#include "models/MLTaskType.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <armadillo>

#include <memory>
#include <sstream>
#include <string>

using namespace MLCore;

// ============================================================================
// Stub model implementations for testing
// ============================================================================

namespace {

/**
 * @brief Minimal supervised classifier stub — always predicts label 0
 */
class StubClassifier : public MLModelOperation {
public:
    [[nodiscard]] std::string getName() const override { return "Stub Classifier"; }
    [[nodiscard]] MLTaskType getTaskType() const override { return MLTaskType::BinaryClassification; }

    [[nodiscard]] std::unique_ptr<MLModelParametersBase> getDefaultParameters() const override
    {
        return std::make_unique<RandomForestParameters>();
    }

    bool train(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        MLModelParametersBase const * /*params*/) override
    {
        if (features.n_cols != labels.n_elem) return false;
        _num_features = features.n_rows;
        _num_classes = arma::max(labels) + 1;
        _trained = true;
        return true;
    }

    bool predict(
        arma::mat const & features,
        arma::Row<std::size_t> & predictions) override
    {
        if (!_trained) return false;
        predictions.zeros(features.n_cols);
        return true;
    }

    bool predictProbabilities(
        arma::mat const & features,
        arma::mat & probabilities) override
    {
        if (!_trained) return false;
        probabilities.ones(_num_classes, features.n_cols);
        probabilities.row(0).fill(0.9);
        if (_num_classes > 1) {
            probabilities.row(1).fill(0.1);
        }
        return true;
    }

    [[nodiscard]] bool isTrained() const override { return _trained; }
    [[nodiscard]] std::size_t numClasses() const override { return _trained ? _num_classes : 0; }
    [[nodiscard]] std::size_t numFeatures() const override { return _trained ? _num_features : 0; }

    bool save(std::ostream & out) const override
    {
        if (!_trained) return false;
        out << _num_features << " " << _num_classes;
        return out.good();
    }

    bool load(std::istream & in) override
    {
        in >> _num_features >> _num_classes;
        if (!in.fail()) {
            _trained = true;
            return true;
        }
        return false;
    }

private:
    bool _trained = false;
    std::size_t _num_features = 0;
    std::size_t _num_classes = 0;
};

/**
 * @brief Minimal multi-class classifier stub
 */
class StubMultiClassifier : public MLModelOperation {
public:
    [[nodiscard]] std::string getName() const override { return "Stub Multi-Classifier"; }
    [[nodiscard]] MLTaskType getTaskType() const override { return MLTaskType::MultiClassClassification; }

    [[nodiscard]] std::unique_ptr<MLModelParametersBase> getDefaultParameters() const override
    {
        return std::make_unique<NaiveBayesParameters>();
    }

    bool train(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        MLModelParametersBase const * /*params*/) override
    {
        if (features.n_cols != labels.n_elem) return false;
        _num_features = features.n_rows;
        _num_classes = arma::max(labels) + 1;
        _trained = true;
        return true;
    }

    bool predict(
        arma::mat const & features,
        arma::Row<std::size_t> & predictions) override
    {
        if (!_trained) return false;
        predictions.zeros(features.n_cols);
        return true;
    }

    [[nodiscard]] bool isTrained() const override { return _trained; }
    [[nodiscard]] std::size_t numClasses() const override { return _trained ? _num_classes : 0; }
    [[nodiscard]] std::size_t numFeatures() const override { return _trained ? _num_features : 0; }

private:
    bool _trained = false;
    std::size_t _num_features = 0;
    std::size_t _num_classes = 0;
};

/**
 * @brief Minimal clustering stub — assigns round-robin cluster IDs
 */
class StubClusterer : public MLModelOperation {
public:
    [[nodiscard]] std::string getName() const override { return "Stub Clusterer"; }
    [[nodiscard]] MLTaskType getTaskType() const override { return MLTaskType::Clustering; }

    [[nodiscard]] std::unique_ptr<MLModelParametersBase> getDefaultParameters() const override
    {
        return std::make_unique<KMeansParameters>();
    }

    bool fit(
        arma::mat const & features,
        MLModelParametersBase const * params) override
    {
        auto const * km_params = dynamic_cast<KMeansParameters const *>(params);
        _k = km_params ? km_params->k : 3;
        _num_features = features.n_rows;
        _fitted = true;
        return true;
    }

    bool assignClusters(
        arma::mat const & features,
        arma::Row<std::size_t> & assignments) override
    {
        if (!_fitted) return false;
        assignments.set_size(features.n_cols);
        for (arma::uword i = 0; i < features.n_cols; ++i) {
            assignments(i) = i % _k;
        }
        return true;
    }

    [[nodiscard]] bool isTrained() const override { return _fitted; }
    [[nodiscard]] std::size_t numClasses() const override { return _fitted ? _k : 0; }
    [[nodiscard]] std::size_t numFeatures() const override { return _fitted ? _num_features : 0; }

private:
    bool _fitted = false;
    std::size_t _k = 0;
    std::size_t _num_features = 0;
};

} // anonymous namespace

// ============================================================================
// MLTaskType tests
// ============================================================================

TEST_CASE("MLTaskType toString", "[MLCore][MLTaskType]")
{
    CHECK(toString(MLTaskType::BinaryClassification) == "Binary Classification");
    CHECK(toString(MLTaskType::MultiClassClassification) == "Multi-Class Classification");
    CHECK(toString(MLTaskType::Clustering) == "Clustering");
}

// ============================================================================
// MLModelParameters tests
// ============================================================================

TEST_CASE("MLModelParametersBase is polymorphic", "[MLCore][MLModelParameters]")
{
    std::unique_ptr<MLModelParametersBase> base = std::make_unique<RandomForestParameters>();
    REQUIRE(base != nullptr);
    auto * rf = dynamic_cast<RandomForestParameters *>(base.get());
    REQUIRE(rf != nullptr);
    CHECK(rf->num_trees == 10);
    CHECK(rf->minimum_leaf_size == 1);
    CHECK(rf->maximum_depth == 0);
}

TEST_CASE("RandomForestParameters defaults", "[MLCore][MLModelParameters]")
{
    RandomForestParameters params;
    CHECK(params.num_trees == 10);
    CHECK(params.minimum_leaf_size == 1);
    CHECK_THAT(params.minimum_gain_split, Catch::Matchers::WithinRel(1e-7, 1e-10));
    CHECK(params.maximum_depth == 0);
}

TEST_CASE("NaiveBayesParameters defaults", "[MLCore][MLModelParameters]")
{
    NaiveBayesParameters params;
    CHECK_THAT(params.epsilon, Catch::Matchers::WithinRel(1e-9, 1e-10));
}

TEST_CASE("LogisticRegressionParameters defaults", "[MLCore][MLModelParameters]")
{
    LogisticRegressionParameters params;
    CHECK_THAT(params.lambda, Catch::Matchers::WithinAbs(0.0, 1e-15));
    CHECK_THAT(params.step_size, Catch::Matchers::WithinRel(0.01, 1e-10));
    CHECK(params.max_iterations == 10000);
}

TEST_CASE("KMeansParameters defaults", "[MLCore][MLModelParameters]")
{
    KMeansParameters params;
    CHECK(params.k == 3);
    CHECK(params.max_iterations == 1000);
    CHECK_FALSE(params.seed.has_value());
}

TEST_CASE("DBSCANParameters defaults", "[MLCore][MLModelParameters]")
{
    DBSCANParameters params;
    CHECK_THAT(params.epsilon, Catch::Matchers::WithinRel(1.0, 1e-10));
    CHECK(params.min_points == 5);
}

TEST_CASE("GMMParameters defaults", "[MLCore][MLModelParameters]")
{
    GMMParameters params;
    CHECK(params.k == 3);
    CHECK(params.max_iterations == 300);
    CHECK_FALSE(params.seed.has_value());
}

// ============================================================================
// MLModelOperation — supervised workflow tests
// ============================================================================

TEST_CASE("StubClassifier identity and metadata", "[MLCore][MLModelOperation]")
{
    StubClassifier clf;
    CHECK(clf.getName() == "Stub Classifier");
    CHECK(clf.getTaskType() == MLTaskType::BinaryClassification);
    CHECK_FALSE(clf.isTrained());
    CHECK(clf.numClasses() == 0);
    CHECK(clf.numFeatures() == 0);
}

TEST_CASE("StubClassifier default parameters", "[MLCore][MLModelOperation]")
{
    StubClassifier clf;
    auto params = clf.getDefaultParameters();
    REQUIRE(params != nullptr);
    auto * rf = dynamic_cast<RandomForestParameters *>(params.get());
    REQUIRE(rf != nullptr);
    CHECK(rf->num_trees == 10);
}

TEST_CASE("StubClassifier train and predict", "[MLCore][MLModelOperation]")
{
    StubClassifier clf;

    // Create toy data: 3 features, 10 observations
    arma::mat features(3, 10, arma::fill::randn);
    arma::Row<std::size_t> labels = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1};

    SECTION("predict before training fails")
    {
        arma::Row<std::size_t> predictions;
        CHECK_FALSE(clf.predict(features, predictions));
    }

    SECTION("train succeeds with matching dimensions")
    {
        CHECK(clf.train(features, labels, nullptr));
        CHECK(clf.isTrained());
        CHECK(clf.numClasses() == 2);
        CHECK(clf.numFeatures() == 3);
    }

    SECTION("train fails with mismatched dimensions")
    {
        arma::Row<std::size_t> bad_labels = {0, 1, 0}; // only 3 labels for 10 obs
        CHECK_FALSE(clf.train(features, bad_labels, nullptr));
        CHECK_FALSE(clf.isTrained());
    }

    SECTION("predict after training succeeds")
    {
        clf.train(features, labels, nullptr);

        arma::Row<std::size_t> predictions;
        CHECK(clf.predict(features, predictions));
        CHECK(predictions.n_elem == 10);
    }

    SECTION("predictProbabilities after training")
    {
        clf.train(features, labels, nullptr);

        arma::mat probs;
        CHECK(clf.predictProbabilities(features, probs));
        CHECK(probs.n_rows == 2);    // num_classes
        CHECK(probs.n_cols == 10);   // num_observations
    }
}

TEST_CASE("StubClassifier save and load round-trip", "[MLCore][MLModelOperation]")
{
    StubClassifier clf;
    arma::mat features(4, 20, arma::fill::randn);
    arma::Row<std::size_t> labels(20);
    labels.fill(0);
    labels.subvec(10, 19).fill(1);

    clf.train(features, labels, nullptr);
    REQUIRE(clf.isTrained());

    // Save
    std::stringstream ss;
    CHECK(clf.save(ss));

    // Reset stream position for reading
    ss.seekg(0);

    // Load into new instance
    StubClassifier clf2;
    CHECK_FALSE(clf2.isTrained());
    CHECK(clf2.load(ss));
    CHECK(clf2.isTrained());
    CHECK(clf2.numFeatures() == 4);
    CHECK(clf2.numClasses() == 2);
}

TEST_CASE("StubClassifier save before training fails", "[MLCore][MLModelOperation]")
{
    StubClassifier clf;
    std::stringstream ss;
    CHECK_FALSE(clf.save(ss));
}

// ============================================================================
// MLModelOperation — unsupervised workflow tests
// ============================================================================

TEST_CASE("StubClusterer identity and metadata", "[MLCore][MLModelOperation]")
{
    StubClusterer clust;
    CHECK(clust.getName() == "Stub Clusterer");
    CHECK(clust.getTaskType() == MLTaskType::Clustering);
    CHECK_FALSE(clust.isTrained());
    CHECK(clust.numClasses() == 0);
    CHECK(clust.numFeatures() == 0);
}

TEST_CASE("StubClusterer fit and assign", "[MLCore][MLModelOperation]")
{
    StubClusterer clust;
    arma::mat features(2, 12, arma::fill::randn);

    SECTION("assignClusters before fit fails")
    {
        arma::Row<std::size_t> assignments;
        CHECK_FALSE(clust.assignClusters(features, assignments));
    }

    SECTION("fit with default params")
    {
        auto params = clust.getDefaultParameters();
        CHECK(clust.fit(features, params.get()));
        CHECK(clust.isTrained());
        CHECK(clust.numClasses() == 3);  // default K=3
        CHECK(clust.numFeatures() == 2);
    }

    SECTION("fit with custom K")
    {
        KMeansParameters params;
        params.k = 5;
        CHECK(clust.fit(features, &params));
        CHECK(clust.numClasses() == 5);
    }

    SECTION("assignClusters after fit")
    {
        auto params = clust.getDefaultParameters();
        clust.fit(features, params.get());

        arma::Row<std::size_t> assignments;
        CHECK(clust.assignClusters(features, assignments));
        CHECK(assignments.n_elem == 12);
        // Stub assigns round-robin
        CHECK(assignments(0) == 0);
        CHECK(assignments(1) == 1);
        CHECK(assignments(2) == 2);
        CHECK(assignments(3) == 0);
    }
}

// ============================================================================
// MLModelOperation — default method behavior
// ============================================================================

TEST_CASE("Supervised model defaults for unsupervised methods", "[MLCore][MLModelOperation]")
{
    StubClassifier clf;
    arma::mat features(2, 5, arma::fill::randn);

    // Supervised model should return false for unsupervised methods
    CHECK_FALSE(clf.fit(features, nullptr));
    arma::Row<std::size_t> assignments;
    CHECK_FALSE(clf.assignClusters(features, assignments));
}

TEST_CASE("Unsupervised model defaults for supervised methods", "[MLCore][MLModelOperation]")
{
    StubClusterer clust;
    arma::mat features(2, 5, arma::fill::randn);
    arma::Row<std::size_t> labels = {0, 1, 0, 1, 0};

    // Unsupervised model should return false for supervised methods
    CHECK_FALSE(clust.train(features, labels, nullptr));
    arma::Row<std::size_t> predictions;
    CHECK_FALSE(clust.predict(features, predictions));
    arma::mat probs;
    CHECK_FALSE(clust.predictProbabilities(features, probs));
}

TEST_CASE("Default save/load return false", "[MLCore][MLModelOperation]")
{
    StubClusterer clust;
    std::stringstream ss;
    CHECK_FALSE(clust.save(ss));
    CHECK_FALSE(clust.load(ss));
}

// ============================================================================
// MLModelRegistry tests
// ============================================================================

TEST_CASE("MLModelRegistry has built-in models on construction", "[MLCore][MLModelRegistry]")
{
    MLModelRegistry registry;
    // Built-in models are registered in the constructor (Phase 2)
    CHECK(registry.size() >= 1);
    CHECK(registry.hasModel("Random Forest"));
}

TEST_CASE("MLModelRegistry registerModel by factory", "[MLCore][MLModelRegistry]")
{
    MLModelRegistry registry;
    auto const initial_size = registry.size();

    registry.registerModel(
        "Test Classifier",
        MLTaskType::BinaryClassification,
        []() { return std::make_unique<StubClassifier>(); });

    CHECK(registry.size() == initial_size + 1);
    CHECK(registry.hasModel("Test Classifier"));
    CHECK_FALSE(registry.hasModel("Nonexistent"));
}

TEST_CASE("MLModelRegistry registerModel template", "[MLCore][MLModelRegistry]")
{
    MLModelRegistry registry;
    auto const initial_size = registry.size();

    registry.registerModel<StubClassifier>();
    registry.registerModel<StubClusterer>();
    registry.registerModel<StubMultiClassifier>();

    CHECK(registry.size() == initial_size + 3);
    CHECK(registry.hasModel("Stub Classifier"));
    CHECK(registry.hasModel("Stub Clusterer"));
    CHECK(registry.hasModel("Stub Multi-Classifier"));
}

TEST_CASE("MLModelRegistry getAllModelNames", "[MLCore][MLModelRegistry]")
{
    MLModelRegistry registry;
    auto const initial_size = registry.size();

    registry.registerModel<StubClassifier>();
    registry.registerModel<StubClusterer>();

    auto names = registry.getAllModelNames();
    REQUIRE(names.size() == initial_size + 2);
    // Stub models are at the end (after built-in models)
    CHECK(std::find(names.begin(), names.end(), "Stub Classifier") != names.end());
    CHECK(std::find(names.begin(), names.end(), "Stub Clusterer") != names.end());
}

TEST_CASE("MLModelRegistry getModelNames filters by task", "[MLCore][MLModelRegistry]")
{
    MLModelRegistry registry;
    registry.registerModel<StubClassifier>();
    registry.registerModel<StubMultiClassifier>();
    registry.registerModel<StubClusterer>();

    SECTION("BinaryClassification")
    {
        auto names = registry.getModelNames(MLTaskType::BinaryClassification);
        REQUIRE(names.size() == 1);
        CHECK(names[0] == "Stub Classifier");
    }

    SECTION("MultiClassClassification")
    {
        auto names = registry.getModelNames(MLTaskType::MultiClassClassification);
        // Built-in "Random Forest" is also MultiClassClassification
        CHECK(std::find(names.begin(), names.end(), "Stub Multi-Classifier") != names.end());
    }

    SECTION("Clustering")
    {
        auto names = registry.getModelNames(MLTaskType::Clustering);
        REQUIRE(names.size() == 1);
        CHECK(names[0] == "Stub Clusterer");
    }
}

TEST_CASE("MLModelRegistry getTaskType", "[MLCore][MLModelRegistry]")
{
    MLModelRegistry registry;
    registry.registerModel<StubClassifier>();

    auto task = registry.getTaskType("Stub Classifier");
    REQUIRE(task.has_value());
    CHECK(*task == MLTaskType::BinaryClassification);

    auto missing = registry.getTaskType("Nonexistent");
    CHECK_FALSE(missing.has_value());
}

TEST_CASE("MLModelRegistry create returns fresh instance", "[MLCore][MLModelRegistry]")
{
    MLModelRegistry registry;
    registry.registerModel<StubClassifier>();

    auto model1 = registry.create("Stub Classifier");
    auto model2 = registry.create("Stub Classifier");

    REQUIRE(model1 != nullptr);
    REQUIRE(model2 != nullptr);
    CHECK(model1.get() != model2.get()); // different instances
    CHECK(model1->getName() == "Stub Classifier");
    CHECK_FALSE(model1->isTrained());
    CHECK_FALSE(model2->isTrained());
}

TEST_CASE("MLModelRegistry create returns nullptr for unknown name", "[MLCore][MLModelRegistry]")
{
    MLModelRegistry registry;
    auto model = registry.create("Nonexistent");
    CHECK(model == nullptr);
}

TEST_CASE("MLModelRegistry replace existing model", "[MLCore][MLModelRegistry]")
{
    MLModelRegistry registry;
    registry.registerModel<StubClassifier>();
    auto const size_after_add = registry.size();

    CHECK(registry.getTaskType("Stub Classifier") == MLTaskType::BinaryClassification);

    // Re-register with same name but different task (contrived but tests replacement)
    registry.registerModel(
        "Stub Classifier",
        MLTaskType::MultiClassClassification,
        []() { return std::make_unique<StubClassifier>(); });

    CHECK(registry.size() == size_after_add); // still same, not +1
    CHECK(registry.getTaskType("Stub Classifier") == MLTaskType::MultiClassClassification);
}

TEST_CASE("MLModelRegistry created models are independent", "[MLCore][MLModelRegistry]")
{
    MLModelRegistry registry;
    registry.registerModel<StubClassifier>();

    auto model1 = registry.create("Stub Classifier");
    auto model2 = registry.create("Stub Classifier");

    arma::mat features(2, 10, arma::fill::randn);
    arma::Row<std::size_t> labels = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1};

    // Train model1 only
    model1->train(features, labels, nullptr);

    CHECK(model1->isTrained());
    CHECK_FALSE(model2->isTrained()); // model2 is unaffected
}

// ============================================================================
// End-to-end workflow: registry → create → train → predict
// ============================================================================

TEST_CASE("End-to-end supervised workflow via registry", "[MLCore][MLModelRegistry][integration]")
{
    MLModelRegistry registry;
    registry.registerModel<StubClassifier>();

    // 1. Look up model
    auto model = registry.create("Stub Classifier");
    REQUIRE(model != nullptr);

    // 2. Get default parameters
    auto params = model->getDefaultParameters();
    REQUIRE(params != nullptr);

    // 3. Train
    arma::mat train_features(3, 20, arma::fill::randn);
    arma::Row<std::size_t> train_labels(20);
    train_labels.subvec(0, 9).fill(0);
    train_labels.subvec(10, 19).fill(1);

    CHECK(model->train(train_features, train_labels, params.get()));
    CHECK(model->isTrained());
    CHECK(model->numClasses() == 2);
    CHECK(model->numFeatures() == 3);

    // 4. Predict
    arma::mat test_features(3, 5, arma::fill::randn);
    arma::Row<std::size_t> predictions;
    CHECK(model->predict(test_features, predictions));
    CHECK(predictions.n_elem == 5);

    // 5. Probabilities
    arma::mat probs;
    CHECK(model->predictProbabilities(test_features, probs));
    CHECK(probs.n_rows == 2);
    CHECK(probs.n_cols == 5);
}

TEST_CASE("End-to-end unsupervised workflow via registry", "[MLCore][MLModelRegistry][integration]")
{
    MLModelRegistry registry;
    registry.registerModel<StubClusterer>();

    // 1. Look up model
    auto model = registry.create("Stub Clusterer");
    REQUIRE(model != nullptr);

    // 2. Configure params
    KMeansParameters params;
    params.k = 4;

    // 3. Fit
    arma::mat features(5, 100, arma::fill::randn);
    CHECK(model->fit(features, &params));
    CHECK(model->isTrained());
    CHECK(model->numClasses() == 4);
    CHECK(model->numFeatures() == 5);

    // 4. Assign clusters
    arma::Row<std::size_t> assignments;
    CHECK(model->assignClusters(features, assignments));
    CHECK(assignments.n_elem == 100);

    // Verify assignments are in [0, K)
    CHECK(arma::max(assignments) <= 3);
}
