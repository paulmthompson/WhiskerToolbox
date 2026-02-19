/**
 * @file KMeansOperation.test.cpp
 * @brief Catch2 tests for MLCore::KMeansOperation
 *
 * Tests the MLCore-native K-Means clustering implementation against
 * synthetic data with known cluster structure. Covers:
 * - Metadata (name, task type, default parameters)
 * - Fitting on synthetic clustered data
 * - Cluster assignment accuracy on well-separated clusters
 * - Centroid correctness
 * - Feature dimension validation
 * - Edge cases (empty data, unfitted model, k > n)
 * - Save/load round-trip
 * - Parameter variations (k, max_iterations, seed)
 * - Registry integration
 */

#include "models/unsupervised/KMeansOperation.hpp"
#include "models/MLModelParameters.hpp"
#include "models/MLModelRegistry.hpp"
#include "models/MLTaskType.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <armadillo>

#include <algorithm>
#include <sstream>

using namespace MLCore;

// ============================================================================
// Helper: create well-separated 2D cluster data
// ============================================================================

namespace {

/**
 * @brief Generate well-separated 2D data with known clusters
 *
 * Cluster 0: centered around (-5, -5)
 * Cluster 1: centered around (+5, +5)
 * Cluster 2: centered around (+5, -5)
 *
 * Well-separated clusters so K-Means should recover them perfectly.
 *
 * @param n_per_cluster Number of samples per cluster
 * @param seed          Random seed for reproducibility
 */
struct SyntheticClusterData {
    arma::mat features;                  // 2 × (3 * n_per_cluster)
    arma::Row<std::size_t> true_labels;  // ground truth cluster labels
    std::size_t num_clusters = 3;
};

SyntheticClusterData makeClusterData(std::size_t n_per_cluster = 50, int seed = 42)
{
    arma::arma_rng::set_seed(seed);

    arma::mat cluster0 = arma::randn(2, n_per_cluster) * 0.3;
    cluster0.row(0) -= 5.0;
    cluster0.row(1) -= 5.0;

    arma::mat cluster1 = arma::randn(2, n_per_cluster) * 0.3;
    cluster1.row(0) += 5.0;
    cluster1.row(1) += 5.0;

    arma::mat cluster2 = arma::randn(2, n_per_cluster) * 0.3;
    cluster2.row(0) += 5.0;
    cluster2.row(1) -= 5.0;

    SyntheticClusterData data;
    data.features = arma::join_horiz(cluster0, arma::join_horiz(cluster1, cluster2));
    data.true_labels = arma::join_horiz(
        arma::Row<std::size_t>(n_per_cluster, arma::fill::zeros),
        arma::join_horiz(
            arma::ones<arma::Row<std::size_t>>(n_per_cluster),
            arma::Row<std::size_t>(n_per_cluster, arma::fill::value(2))));

    return data;
}

/**
 * @brief Compute cluster assignment accuracy accounting for label permutation.
 *
 * K-Means cluster labels are arbitrary (cluster 0 in K-Means may correspond to
 * cluster 2 in the ground truth). This function tries all permutations of
 * label mapping for small k and returns the best accuracy.
 */
double permutedAccuracy(
    arma::Row<std::size_t> const & predictions,
    arma::Row<std::size_t> const & truth,
    std::size_t k)
{
    // Build all permutations of [0..k-1]
    std::vector<std::size_t> perm(k);
    std::iota(perm.begin(), perm.end(), 0);

    double best_accuracy = 0.0;
    do {
        std::size_t correct = 0;
        for (std::size_t i = 0; i < predictions.n_elem; ++i) {
            if (perm[predictions(i)] == truth(i)) {
                ++correct;
            }
        }
        double accuracy = static_cast<double>(correct) / static_cast<double>(truth.n_elem);
        best_accuracy = std::max(best_accuracy, accuracy);
    } while (std::next_permutation(perm.begin(), perm.end()));

    return best_accuracy;
}

/**
 * @brief Generate simple 2D data with 2 well-separated clusters
 */
struct SyntheticBinaryClusterData {
    arma::mat features;
    arma::Row<std::size_t> true_labels;
};

SyntheticBinaryClusterData makeBinaryClusterData(
    std::size_t n_per_cluster = 50, int seed = 42)
{
    arma::arma_rng::set_seed(seed);

    arma::mat cluster0 = arma::randn(2, n_per_cluster) * 0.3;
    cluster0.row(0) -= 5.0;
    cluster0.row(1) -= 5.0;

    arma::mat cluster1 = arma::randn(2, n_per_cluster) * 0.3;
    cluster1.row(0) += 5.0;
    cluster1.row(1) += 5.0;

    SyntheticBinaryClusterData data;
    data.features = arma::join_horiz(cluster0, cluster1);
    data.true_labels = arma::join_horiz(
        arma::Row<std::size_t>(n_per_cluster, arma::fill::zeros),
        arma::ones<arma::Row<std::size_t>>(n_per_cluster));

    return data;
}

} // anonymous namespace

// ============================================================================
// Metadata tests
// ============================================================================

TEST_CASE("KMeansOperation - metadata", "[MLCore][KMeans]")
{
    KMeansOperation km;

    SECTION("name is 'K-Means'")
    {
        CHECK(km.getName() == "K-Means");
    }

    SECTION("task type is Clustering")
    {
        CHECK(km.getTaskType() == MLTaskType::Clustering);
    }

    SECTION("default parameters are KMeansParameters")
    {
        auto params = km.getDefaultParameters();
        REQUIRE(params != nullptr);

        auto const * km_params = dynamic_cast<KMeansParameters const *>(params.get());
        REQUIRE(km_params != nullptr);

        CHECK(km_params->k == 3);
        CHECK(km_params->max_iterations == 1000);
        CHECK_FALSE(km_params->seed.has_value());
    }

    SECTION("not trained initially")
    {
        CHECK_FALSE(km.isTrained());
        CHECK(km.numClasses() == 0);
        CHECK(km.numFeatures() == 0);
    }

    SECTION("centroids are empty before fitting")
    {
        CHECK(km.centroids().empty());
    }

    SECTION("supervised methods return false")
    {
        arma::mat features = arma::randn(2, 10);
        arma::Row<std::size_t> labels(10, arma::fill::zeros);
        arma::Row<std::size_t> predictions;
        arma::mat probs;

        CHECK_FALSE(km.train(features, labels, nullptr));
        CHECK_FALSE(km.predict(features, predictions));
        CHECK_FALSE(km.predictProbabilities(features, probs));
    }
}

// ============================================================================
// Fitting tests
// ============================================================================

TEST_CASE("KMeansOperation - fitting", "[MLCore][KMeans]")
{
    KMeansOperation km;
    auto data = makeClusterData(50);

    SECTION("fit with default parameters (nullptr)")
    {
        bool ok = km.fit(data.features, nullptr);
        CHECK(ok);
        CHECK(km.isTrained());
        CHECK(km.numClasses() == 3);  // default k=3
        CHECK(km.numFeatures() == 2);
    }

    SECTION("fit with explicit parameters")
    {
        KMeansParameters params;
        params.k = 3;
        params.max_iterations = 500;
        params.seed = 42;

        bool ok = km.fit(data.features, &params);
        CHECK(ok);
        CHECK(km.isTrained());
        CHECK(km.numClasses() == 3);
        CHECK(km.numFeatures() == 2);
    }

    SECTION("centroids have correct dimensions after fitting")
    {
        KMeansParameters params;
        params.k = 3;
        params.seed = 42;

        REQUIRE(km.fit(data.features, &params));

        auto const & centroids = km.centroids();
        CHECK(centroids.n_rows == 2);  // 2D features
        CHECK(centroids.n_cols == 3);  // 3 clusters
    }

    SECTION("fit with k=2")
    {
        KMeansParameters params;
        params.k = 2;
        params.seed = 42;

        bool ok = km.fit(data.features, &params);
        CHECK(ok);
        CHECK(km.numClasses() == 2);
    }

    SECTION("refitting resets model")
    {
        KMeansParameters params;
        params.k = 3;
        params.seed = 42;
        km.fit(data.features, &params);
        CHECK(km.numClasses() == 3);

        // Refit with different k
        params.k = 2;
        bool ok = km.fit(data.features, &params);
        CHECK(ok);
        CHECK(km.numClasses() == 2);
    }

    SECTION("fit with k=1 (degenerate case)")
    {
        KMeansParameters params;
        params.k = 1;
        params.seed = 42;

        bool ok = km.fit(data.features, &params);
        CHECK(ok);
        CHECK(km.numClasses() == 1);
    }
}

// ============================================================================
// Fit error handling
// ============================================================================

TEST_CASE("KMeansOperation - fit error handling", "[MLCore][KMeans]")
{
    KMeansOperation km;

    SECTION("empty features")
    {
        arma::mat empty_features;
        CHECK_FALSE(km.fit(empty_features, nullptr));
        CHECK_FALSE(km.isTrained());
    }

    SECTION("k = 0")
    {
        arma::mat features = arma::randn(2, 10);
        KMeansParameters params;
        params.k = 0;
        CHECK_FALSE(km.fit(features, &params));
        CHECK_FALSE(km.isTrained());
    }

    SECTION("k > number of observations")
    {
        arma::mat features = arma::randn(2, 5);
        KMeansParameters params;
        params.k = 10;
        CHECK_FALSE(km.fit(features, &params));
        CHECK_FALSE(km.isTrained());
    }
}

// ============================================================================
// Cluster assignment accuracy
// ============================================================================

TEST_CASE("KMeansOperation - cluster assignment on well-separated data",
          "[MLCore][KMeans]")
{
    KMeansOperation km;

    auto data = makeClusterData(50, /*seed=*/42);

    KMeansParameters params;
    params.k = 3;
    params.seed = 42;

    REQUIRE(km.fit(data.features, &params));

    SECTION("high accuracy on fitting data (accounting for label permutation)")
    {
        arma::Row<std::size_t> assignments;
        bool ok = km.assignClusters(data.features, assignments);
        REQUIRE(ok);
        REQUIRE(assignments.n_elem == data.true_labels.n_elem);

        double accuracy = permutedAccuracy(assignments, data.true_labels, 3);
        // Well-separated clusters — should be near-perfect
        CHECK(accuracy > 0.95);
    }

    SECTION("high accuracy on new data from same distribution")
    {
        auto test_data = makeClusterData(30, /*seed=*/123);

        arma::Row<std::size_t> assignments;
        bool ok = km.assignClusters(test_data.features, assignments);
        REQUIRE(ok);

        double accuracy = permutedAccuracy(assignments, test_data.true_labels, 3);
        CHECK(accuracy > 0.90);
    }

    SECTION("k=2 on 2-cluster data achieves high accuracy")
    {
        auto binary_data = makeBinaryClusterData(60, /*seed=*/42);

        KMeansOperation km2;
        KMeansParameters params2;
        params2.k = 2;
        params2.seed = 42;

        REQUIRE(km2.fit(binary_data.features, &params2));

        arma::Row<std::size_t> assignments;
        REQUIRE(km2.assignClusters(binary_data.features, assignments));

        double accuracy = permutedAccuracy(assignments, binary_data.true_labels, 2);
        CHECK(accuracy > 0.95);
    }
}

// ============================================================================
// Cluster assignment error handling
// ============================================================================

TEST_CASE("KMeansOperation - assignClusters error handling", "[MLCore][KMeans]")
{
    KMeansOperation km;

    SECTION("assignClusters before fitting fails")
    {
        arma::mat features = arma::randn(2, 5);
        arma::Row<std::size_t> assignments;
        CHECK_FALSE(km.assignClusters(features, assignments));
    }

    SECTION("assignClusters with empty features fails")
    {
        auto data = makeClusterData(30);
        KMeansParameters params;
        params.k = 3;
        params.seed = 42;
        km.fit(data.features, &params);

        arma::mat empty_features;
        arma::Row<std::size_t> assignments;
        CHECK_FALSE(km.assignClusters(empty_features, assignments));
    }

    SECTION("assignClusters with wrong feature dimension fails")
    {
        auto data = makeClusterData(30);
        KMeansParameters params;
        params.k = 3;
        params.seed = 42;
        km.fit(data.features, &params);
        REQUIRE(km.numFeatures() == 2);

        arma::mat wrong_dim = arma::randn(5, 10);  // 5 features instead of 2
        arma::Row<std::size_t> assignments;
        CHECK_FALSE(km.assignClusters(wrong_dim, assignments));
    }
}

// ============================================================================
// Centroid tests
// ============================================================================

TEST_CASE("KMeansOperation - centroid correctness", "[MLCore][KMeans]")
{
    KMeansOperation km;

    auto data = makeClusterData(100, /*seed=*/42);

    KMeansParameters params;
    params.k = 3;
    params.seed = 42;

    REQUIRE(km.fit(data.features, &params));

    SECTION("centroids are near true cluster centers")
    {
        auto const & centroids = km.centroids();
        REQUIRE(centroids.n_cols == 3);

        // True centers: (-5,-5), (+5,+5), (+5,-5)
        // K-Means labels are arbitrary, so find which centroid is closest to each true center
        arma::mat true_centers = {{-5.0, 5.0, 5.0},
                                  {-5.0, 5.0, -5.0}};

        for (std::size_t t = 0; t < 3; ++t) {
            double min_dist = std::numeric_limits<double>::max();
            for (std::size_t c = 0; c < 3; ++c) {
                double dist = arma::norm(centroids.col(c) - true_centers.col(t), 2);
                min_dist = std::min(min_dist, dist);
            }
            // Each true center should have a centroid within 1.0 unit
            CHECK(min_dist < 1.0);
        }
    }
}

// ============================================================================
// Save / load round-trip
// ============================================================================

TEST_CASE("KMeansOperation - save/load round-trip", "[MLCore][KMeans]")
{
    auto data = makeClusterData(60, /*seed=*/42);

    KMeansOperation original;
    KMeansParameters params;
    params.k = 3;
    params.seed = 42;
    REQUIRE(original.fit(data.features, &params));

    arma::Row<std::size_t> original_assignments;
    REQUIRE(original.assignClusters(data.features, original_assignments));

    SECTION("save and load produces identical assignments")
    {
        std::stringstream ss;
        REQUIRE(original.save(ss));

        KMeansOperation loaded;
        CHECK_FALSE(loaded.isTrained());
        REQUIRE(loaded.load(ss));

        CHECK(loaded.isTrained());
        CHECK(loaded.numClasses() == original.numClasses());
        CHECK(loaded.numFeatures() == original.numFeatures());

        arma::Row<std::size_t> loaded_assignments;
        REQUIRE(loaded.assignClusters(data.features, loaded_assignments));
        CHECK(arma::all(loaded_assignments == original_assignments));
    }

    SECTION("save and load preserves centroids")
    {
        std::stringstream ss;
        REQUIRE(original.save(ss));

        KMeansOperation loaded;
        REQUIRE(loaded.load(ss));

        CHECK(arma::approx_equal(
            original.centroids(), loaded.centroids(), "absdiff", 1e-10));
    }

    SECTION("save fails when not fitted")
    {
        KMeansOperation unfitted;
        std::stringstream ss;
        CHECK_FALSE(unfitted.save(ss));
    }

    SECTION("load from corrupted stream fails")
    {
        std::stringstream ss("garbage data that is not a valid model");
        KMeansOperation loaded;
        CHECK_FALSE(loaded.load(ss));
        CHECK_FALSE(loaded.isTrained());
    }
}

// ============================================================================
// Parameter variations
// ============================================================================

TEST_CASE("KMeansOperation - parameter variations", "[MLCore][KMeans]")
{
    auto data = makeClusterData(60, /*seed=*/42);

    SECTION("different k values produce valid models")
    {
        for (std::size_t k : {2, 3, 4, 5}) {
            KMeansOperation km;
            KMeansParameters params;
            params.k = k;
            params.seed = 42;

            REQUIRE(km.fit(data.features, &params));
            CHECK(km.isTrained());
            CHECK(km.numClasses() == k);

            arma::Row<std::size_t> assignments;
            REQUIRE(km.assignClusters(data.features, assignments));
            CHECK(assignments.n_elem == data.features.n_cols);

            // Verify all assignments are in [0, k)
            CHECK(arma::max(assignments) < k);
        }
    }

    SECTION("deterministic with same seed")
    {
        KMeansParameters params;
        params.k = 3;
        params.seed = 42;

        KMeansOperation km1;
        REQUIRE(km1.fit(data.features, &params));

        KMeansOperation km2;
        REQUIRE(km2.fit(data.features, &params));

        // With the same seed, centroids should be identical
        CHECK(arma::approx_equal(km1.centroids(), km2.centroids(), "absdiff", 1e-10));
    }

    SECTION("different seeds produce valid but possibly different results")
    {
        KMeansParameters params1;
        params1.k = 3;
        params1.seed = 42;

        KMeansParameters params2;
        params2.k = 3;
        params2.seed = 7;

        KMeansOperation km1;
        REQUIRE(km1.fit(data.features, &params1));

        KMeansOperation km2;
        REQUIRE(km2.fit(data.features, &params2));

        // Both should produce valid assignments
        arma::Row<std::size_t> assignments1, assignments2;
        REQUIRE(km1.assignClusters(data.features, assignments1));
        REQUIRE(km2.assignClusters(data.features, assignments2));

        CHECK(assignments1.n_elem == data.features.n_cols);
        CHECK(assignments2.n_elem == data.features.n_cols);

        // Both should have all cluster labels in [0, k)
        CHECK(arma::max(assignments1) < 3);
        CHECK(arma::max(assignments2) < 3);

        // At least one should achieve high accuracy on well-separated data
        double acc1 = permutedAccuracy(assignments1, data.true_labels, 3);
        CHECK(acc1 > 0.90);
    }

    SECTION("wrong parameter type falls back to defaults")
    {
        KMeansOperation km;
        RandomForestParameters wrong_params;
        bool ok = km.fit(data.features, &wrong_params);
        CHECK(ok);
        CHECK(km.isTrained());
        CHECK(km.numClasses() == 3);  // default k=3
    }

    SECTION("custom max_iterations fits successfully")
    {
        KMeansOperation km;
        KMeansParameters params;
        params.k = 3;
        params.max_iterations = 10;
        params.seed = 42;

        bool ok = km.fit(data.features, &params);
        CHECK(ok);
        CHECK(km.isTrained());
    }
}

// ============================================================================
// Registry integration
// ============================================================================

TEST_CASE("KMeansOperation - registry integration", "[MLCore][KMeans]")
{
    MLModelRegistry registry;

    SECTION("model is registered as built-in")
    {
        CHECK(registry.hasModel("K-Means"));
    }

    SECTION("found under Clustering task")
    {
        auto names = registry.getModelNames(MLTaskType::Clustering);
        CHECK(std::find(names.begin(), names.end(), "K-Means") != names.end());
    }

    SECTION("not listed under BinaryClassification task")
    {
        auto names = registry.getModelNames(MLTaskType::BinaryClassification);
        CHECK(std::find(names.begin(), names.end(), "K-Means") == names.end());
    }

    SECTION("not listed under MultiClassClassification task")
    {
        auto names = registry.getModelNames(MLTaskType::MultiClassClassification);
        CHECK(std::find(names.begin(), names.end(), "K-Means") == names.end());
    }

    SECTION("create returns a fresh instance")
    {
        auto model = registry.create("K-Means");
        REQUIRE(model != nullptr);
        CHECK(model->getName() == "K-Means");
        CHECK_FALSE(model->isTrained());
    }

    SECTION("created instances are independent")
    {
        auto model1 = registry.create("K-Means");
        auto model2 = registry.create("K-Means");
        REQUIRE(model1 != nullptr);
        REQUIRE(model2 != nullptr);

        auto data = makeClusterData(30);
        KMeansParameters params;
        params.k = 3;
        params.seed = 42;
        model1->fit(data.features, &params);

        CHECK(model1->isTrained());
        CHECK_FALSE(model2->isTrained());
    }

    SECTION("registry contains RF, NB, LR, and K-Means")
    {
        CHECK(registry.hasModel("Random Forest"));
        CHECK(registry.hasModel("Naive Bayes"));
        CHECK(registry.hasModel("Logistic Regression"));
        CHECK(registry.hasModel("K-Means"));
        CHECK(registry.size() >= 4);
    }
}
