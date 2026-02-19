/**
 * @file DBSCANOperation.test.cpp
 * @brief Catch2 tests for MLCore::DBSCANOperation
 *
 * Tests the MLCore-native DBSCAN clustering implementation against
 * synthetic data with known cluster structure. Covers:
 * - Metadata (name, task type, default parameters)
 * - Fitting on synthetic clustered data
 * - Noise detection (points in sparse regions)
 * - Cluster assignment accuracy on well-separated clusters
 * - Feature dimension validation
 * - Edge cases (empty data, unfitted model, invalid parameters)
 * - Save/load round-trip
 * - Parameter variations (epsilon, min_points)
 * - Registry integration
 *
 * Key difference from K-Means: DBSCAN can produce noise points (SIZE_MAX)
 * and discovers the number of clusters automatically.
 */

#include "models/unsupervised/DBSCANOperation.hpp"
#include "models/MLModelParameters.hpp"
#include "models/MLModelRegistry.hpp"
#include "models/MLTaskType.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <armadillo>

#include <algorithm>
#include <numeric>
#include <sstream>

using namespace MLCore;

// ============================================================================
// Helper: create well-separated 2D cluster data for DBSCAN
// ============================================================================

namespace {

/**
 * @brief Generate well-separated dense 2D clusters suitable for DBSCAN
 *
 * Cluster 0: centered around (-10, -10), tight spread (std=0.3)
 * Cluster 1: centered around (+10, +10), tight spread (std=0.3)
 * Cluster 2: centered around (+10, -10), tight spread (std=0.3)
 *
 * Very well-separated and dense so DBSCAN should recover them with
 * reasonable epsilon & min_points.
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
    cluster0.row(0) -= 10.0;
    cluster0.row(1) -= 10.0;

    arma::mat cluster1 = arma::randn(2, n_per_cluster) * 0.3;
    cluster1.row(0) += 10.0;
    cluster1.row(1) += 10.0;

    arma::mat cluster2 = arma::randn(2, n_per_cluster) * 0.3;
    cluster2.row(0) += 10.0;
    cluster2.row(1) -= 10.0;

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
 * @brief Generate 2D data with 2 clusters and some noise outliers
 *
 * Cluster 0: centered at (-5, -5), tight
 * Cluster 1: centered at (+5, +5), tight
 * Noise: scattered far from both clusters
 */
struct SyntheticDataWithNoise {
    arma::mat features;
    arma::Row<std::size_t> true_labels;  // SIZE_MAX for noise
    std::size_t n_cluster;
    std::size_t n_noise;
};

SyntheticDataWithNoise makeDataWithNoise(
    std::size_t n_per_cluster = 40,
    std::size_t n_noise = 10,
    int seed = 42)
{
    arma::arma_rng::set_seed(seed);

    arma::mat cluster0 = arma::randn(2, n_per_cluster) * 0.3;
    cluster0.row(0) -= 5.0;
    cluster0.row(1) -= 5.0;

    arma::mat cluster1 = arma::randn(2, n_per_cluster) * 0.3;
    cluster1.row(0) += 5.0;
    cluster1.row(1) += 5.0;

    // Noise: scattered between -20 and +20, but far from clusters
    arma::mat noise(2, n_noise);
    for (std::size_t i = 0; i < n_noise; ++i) {
        // Place noise at large distances from clusters
        noise(0, i) = (i % 2 == 0 ? -18.0 : 18.0) + arma::randn() * 0.1;
        noise(1, i) = (i % 3 == 0 ? -18.0 : 18.0) + arma::randn() * 0.1;
    }

    SyntheticDataWithNoise data;
    data.features = arma::join_horiz(cluster0, arma::join_horiz(cluster1, noise));
    data.n_cluster = n_per_cluster;
    data.n_noise = n_noise;

    // Labels: 0 for cluster0, 1 for cluster1, SIZE_MAX for noise
    data.true_labels = arma::Row<std::size_t>(2 * n_per_cluster + n_noise);
    for (std::size_t i = 0; i < n_per_cluster; ++i) {
        data.true_labels(i) = 0;
    }
    for (std::size_t i = n_per_cluster; i < 2 * n_per_cluster; ++i) {
        data.true_labels(i) = 1;
    }
    for (std::size_t i = 2 * n_per_cluster; i < 2 * n_per_cluster + n_noise; ++i) {
        data.true_labels(i) = SIZE_MAX;
    }

    return data;
}

/**
 * @brief Compute cluster assignment accuracy accounting for label permutation.
 *
 * DBSCAN cluster labels are arbitrary. This function tries all permutations
 * of label mapping for small k and returns the best accuracy.
 * Only considers non-noise points (both in predictions and truth).
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
    std::size_t non_noise_count = 0;

    // Count non-noise points in truth
    for (std::size_t i = 0; i < truth.n_elem; ++i) {
        if (truth(i) != SIZE_MAX) {
            ++non_noise_count;
        }
    }

    if (non_noise_count == 0) {
        return 1.0; // no non-noise points to evaluate
    }

    do {
        std::size_t correct = 0;
        for (std::size_t i = 0; i < predictions.n_elem; ++i) {
            if (truth(i) == SIZE_MAX) {
                continue; // skip noise ground truth
            }
            if (predictions(i) != SIZE_MAX && perm[predictions(i)] == truth(i)) {
                ++correct;
            }
        }
        double accuracy = static_cast<double>(correct) / static_cast<double>(non_noise_count);
        best_accuracy = std::max(best_accuracy, accuracy);
    } while (std::next_permutation(perm.begin(), perm.end()));

    return best_accuracy;
}

} // anonymous namespace

// ============================================================================
// Metadata tests
// ============================================================================

TEST_CASE("DBSCANOperation - metadata", "[MLCore][DBSCAN]")
{
    DBSCANOperation dbscan;

    SECTION("name is 'DBSCAN'")
    {
        CHECK(dbscan.getName() == "DBSCAN");
    }

    SECTION("task type is Clustering")
    {
        CHECK(dbscan.getTaskType() == MLTaskType::Clustering);
    }

    SECTION("default parameters are DBSCANParameters")
    {
        auto params = dbscan.getDefaultParameters();
        REQUIRE(params != nullptr);

        auto const * db_params = dynamic_cast<DBSCANParameters const *>(params.get());
        REQUIRE(db_params != nullptr);

        CHECK(db_params->epsilon == 1.0);
        CHECK(db_params->min_points == 5);
    }

    SECTION("not trained initially")
    {
        CHECK_FALSE(dbscan.isTrained());
        CHECK(dbscan.numClasses() == 0);
        CHECK(dbscan.numFeatures() == 0);
        CHECK(dbscan.numNoisePoints() == 0);
        CHECK(dbscan.fittedEpsilon() == 0.0);
    }

    SECTION("centroids are empty before fitting")
    {
        CHECK(dbscan.centroids().empty());
    }

    SECTION("supervised methods return false")
    {
        arma::mat features = arma::randn(2, 10);
        arma::Row<std::size_t> labels(10, arma::fill::zeros);
        arma::Row<std::size_t> predictions;
        arma::mat probs;

        CHECK_FALSE(dbscan.train(features, labels, nullptr));
        CHECK_FALSE(dbscan.predict(features, predictions));
        CHECK_FALSE(dbscan.predictProbabilities(features, probs));
    }
}

// ============================================================================
// Fitting tests
// ============================================================================

TEST_CASE("DBSCANOperation - fitting", "[MLCore][DBSCAN]")
{
    DBSCANOperation dbscan;
    auto data = makeClusterData(50);

    SECTION("fit with appropriate parameters discovers clusters")
    {
        DBSCANParameters params;
        params.epsilon = 2.0;
        params.min_points = 3;

        bool ok = dbscan.fit(data.features, &params);
        CHECK(ok);
        CHECK(dbscan.isTrained());
        CHECK(dbscan.numFeatures() == 2);
        // Should discover 3 clusters with well-separated data
        CHECK(dbscan.numClasses() >= 2);
        CHECK(dbscan.fittedEpsilon() == 2.0);
    }

    SECTION("fit with default parameters (nullptr) uses defaults")
    {
        bool ok = dbscan.fit(data.features, nullptr);
        CHECK(ok);
        CHECK(dbscan.isTrained());
        CHECK(dbscan.numFeatures() == 2);
    }

    SECTION("centroids have correct dimensions after fitting")
    {
        DBSCANParameters params;
        params.epsilon = 2.0;
        params.min_points = 3;

        REQUIRE(dbscan.fit(data.features, &params));

        if (dbscan.numClasses() > 0) {
            auto const & centroids = dbscan.centroids();
            CHECK(centroids.n_rows == 2);  // 2D features
            CHECK(centroids.n_cols == dbscan.numClasses());
        }
    }

    SECTION("refitting resets model")
    {
        DBSCANParameters params;
        params.epsilon = 2.0;
        params.min_points = 3;
        REQUIRE(dbscan.fit(data.features, &params));
        auto first_clusters = dbscan.numClasses();

        // Refit with very large epsilon (should merge all into 1 cluster)
        params.epsilon = 100.0;
        bool ok = dbscan.fit(data.features, &params);
        CHECK(ok);
        CHECK(dbscan.numClasses() <= first_clusters);
    }

    SECTION("very large epsilon merges all points into one cluster")
    {
        DBSCANParameters params;
        params.epsilon = 100.0;
        params.min_points = 2;

        REQUIRE(dbscan.fit(data.features, &params));
        CHECK(dbscan.numClasses() == 1);
        CHECK(dbscan.numNoisePoints() == 0);
    }

    SECTION("very small epsilon produces all noise")
    {
        DBSCANParameters params;
        params.epsilon = 0.001;
        params.min_points = 100;

        REQUIRE(dbscan.fit(data.features, &params));
        // With very small epsilon and high min_points, everything should be noise
        CHECK(dbscan.numClasses() == 0);
        CHECK(dbscan.numNoisePoints() == data.features.n_cols);
    }
}

// ============================================================================
// Fit error handling
// ============================================================================

TEST_CASE("DBSCANOperation - fit error handling", "[MLCore][DBSCAN]")
{
    DBSCANOperation dbscan;

    SECTION("empty features")
    {
        arma::mat empty_features;
        CHECK_FALSE(dbscan.fit(empty_features, nullptr));
        CHECK_FALSE(dbscan.isTrained());
    }

    SECTION("epsilon = 0")
    {
        arma::mat features = arma::randn(2, 10);
        DBSCANParameters params;
        params.epsilon = 0.0;
        CHECK_FALSE(dbscan.fit(features, &params));
        CHECK_FALSE(dbscan.isTrained());
    }

    SECTION("epsilon < 0")
    {
        arma::mat features = arma::randn(2, 10);
        DBSCANParameters params;
        params.epsilon = -1.0;
        CHECK_FALSE(dbscan.fit(features, &params));
        CHECK_FALSE(dbscan.isTrained());
    }

    SECTION("min_points = 0")
    {
        arma::mat features = arma::randn(2, 10);
        DBSCANParameters params;
        params.min_points = 0;
        CHECK_FALSE(dbscan.fit(features, &params));
        CHECK_FALSE(dbscan.isTrained());
    }
}

// ============================================================================
// Noise detection
// ============================================================================

TEST_CASE("DBSCANOperation - noise detection", "[MLCore][DBSCAN]")
{
    auto data = makeDataWithNoise(40, 10, /*seed=*/42);

    DBSCANOperation dbscan;
    DBSCANParameters params;
    params.epsilon = 2.0;
    params.min_points = 3;

    REQUIRE(dbscan.fit(data.features, &params));

    SECTION("some noise points detected")
    {
        // With outlier points far from clusters, DBSCAN should detect some noise
        CHECK(dbscan.numNoisePoints() > 0);
    }

    SECTION("clusters discovered and non-zero")
    {
        CHECK(dbscan.numClasses() >= 2);
    }

    SECTION("assignClusters produces SIZE_MAX for far-away points")
    {
        // Create points that are very far from any cluster
        arma::mat far_points = {{100.0, -100.0}, {100.0, -100.0}};

        arma::Row<std::size_t> assignments;
        REQUIRE(dbscan.assignClusters(far_points, assignments));

        // These should be marked as noise
        for (std::size_t i = 0; i < assignments.n_elem; ++i) {
            CHECK(assignments(i) == SIZE_MAX);
        }
    }
}

// ============================================================================
// Cluster assignment accuracy
// ============================================================================

TEST_CASE("DBSCANOperation - cluster assignment on well-separated data",
          "[MLCore][DBSCAN]")
{
    DBSCANOperation dbscan;

    auto data = makeClusterData(50, /*seed=*/42);

    DBSCANParameters params;
    params.epsilon = 2.0;
    params.min_points = 3;

    REQUIRE(dbscan.fit(data.features, &params));

    SECTION("high accuracy on fitting data (accounting for label permutation)")
    {
        arma::Row<std::size_t> assignments;
        bool ok = dbscan.assignClusters(data.features, assignments);
        REQUIRE(ok);
        REQUIRE(assignments.n_elem == data.true_labels.n_elem);

        // DBSCAN should find 3 clusters on well-separated data
        REQUIRE(dbscan.numClasses() == 3);

        double accuracy = permutedAccuracy(assignments, data.true_labels, 3);
        // Well-separated clusters — should achieve high accuracy
        CHECK(accuracy > 0.90);
    }

    SECTION("high accuracy on new data from same distribution")
    {
        auto test_data = makeClusterData(30, /*seed=*/123);

        arma::Row<std::size_t> assignments;
        bool ok = dbscan.assignClusters(test_data.features, assignments);
        REQUIRE(ok);

        // Most points from tight clusters should be assigned correctly
        std::size_t non_noise = 0;
        for (std::size_t i = 0; i < assignments.n_elem; ++i) {
            if (assignments(i) != SIZE_MAX) {
                ++non_noise;
            }
        }
        // Most points should be assigned (not noise) since they're from the same clusters
        CHECK(non_noise > test_data.features.n_cols * 0.8);
    }
}

// ============================================================================
// Cluster assignment error handling
// ============================================================================

TEST_CASE("DBSCANOperation - assignClusters error handling", "[MLCore][DBSCAN]")
{
    DBSCANOperation dbscan;

    SECTION("assignClusters before fitting fails")
    {
        arma::mat features = arma::randn(2, 5);
        arma::Row<std::size_t> assignments;
        CHECK_FALSE(dbscan.assignClusters(features, assignments));
    }

    SECTION("assignClusters with empty features fails")
    {
        auto data = makeClusterData(30);
        DBSCANParameters params;
        params.epsilon = 2.0;
        params.min_points = 3;
        dbscan.fit(data.features, &params);

        arma::mat empty_features;
        arma::Row<std::size_t> assignments;
        CHECK_FALSE(dbscan.assignClusters(empty_features, assignments));
    }

    SECTION("assignClusters with wrong feature dimension fails")
    {
        auto data = makeClusterData(30);
        DBSCANParameters params;
        params.epsilon = 2.0;
        params.min_points = 3;
        dbscan.fit(data.features, &params);
        REQUIRE(dbscan.numFeatures() == 2);

        arma::mat wrong_dim = arma::randn(5, 10);  // 5 features instead of 2
        arma::Row<std::size_t> assignments;
        CHECK_FALSE(dbscan.assignClusters(wrong_dim, assignments));
    }

    SECTION("assignClusters when all noise returns all SIZE_MAX")
    {
        // Fit with params that produce all noise
        auto data = makeClusterData(30);
        DBSCANParameters params;
        params.epsilon = 0.001;
        params.min_points = 100;
        REQUIRE(dbscan.fit(data.features, &params));
        REQUIRE(dbscan.numClasses() == 0);

        arma::mat test = arma::randn(2, 5);
        arma::Row<std::size_t> assignments;
        REQUIRE(dbscan.assignClusters(test, assignments));

        for (std::size_t i = 0; i < assignments.n_elem; ++i) {
            CHECK(assignments(i) == SIZE_MAX);
        }
    }
}

// ============================================================================
// Centroid tests
// ============================================================================

TEST_CASE("DBSCANOperation - centroid correctness", "[MLCore][DBSCAN]")
{
    DBSCANOperation dbscan;

    auto data = makeClusterData(100, /*seed=*/42);

    DBSCANParameters params;
    params.epsilon = 2.0;
    params.min_points = 3;

    REQUIRE(dbscan.fit(data.features, &params));
    REQUIRE(dbscan.numClasses() == 3);

    SECTION("centroids are near true cluster centers")
    {
        auto const & centroids = dbscan.centroids();
        REQUIRE(centroids.n_cols == 3);

        // True centers: (-10,-10), (+10,+10), (+10,-10)
        arma::mat true_centers = {{-10.0, 10.0, 10.0},
                                  {-10.0, 10.0, -10.0}};

        for (std::size_t t = 0; t < 3; ++t) {
            double min_dist = std::numeric_limits<double>::max();
            for (std::size_t c = 0; c < 3; ++c) {
                double dist = arma::norm(centroids.col(c) - true_centers.col(t), 2);
                min_dist = std::min(min_dist, dist);
            }
            // Each true center should have a centroid within 2.0 units
            CHECK(min_dist < 2.0);
        }
    }
}

// ============================================================================
// Save / load round-trip
// ============================================================================

TEST_CASE("DBSCANOperation - save/load round-trip", "[MLCore][DBSCAN]")
{
    auto data = makeClusterData(60, /*seed=*/42);

    DBSCANOperation original;
    DBSCANParameters params;
    params.epsilon = 2.0;
    params.min_points = 3;
    REQUIRE(original.fit(data.features, &params));

    arma::Row<std::size_t> original_assignments;
    REQUIRE(original.assignClusters(data.features, original_assignments));

    SECTION("save and load produces identical assignments")
    {
        std::stringstream ss;
        REQUIRE(original.save(ss));

        DBSCANOperation loaded;
        CHECK_FALSE(loaded.isTrained());
        REQUIRE(loaded.load(ss));

        CHECK(loaded.isTrained());
        CHECK(loaded.numClasses() == original.numClasses());
        CHECK(loaded.numFeatures() == original.numFeatures());
        CHECK(loaded.numNoisePoints() == original.numNoisePoints());
        CHECK(loaded.fittedEpsilon() == original.fittedEpsilon());

        arma::Row<std::size_t> loaded_assignments;
        REQUIRE(loaded.assignClusters(data.features, loaded_assignments));
        CHECK(arma::all(loaded_assignments == original_assignments));
    }

    SECTION("save and load preserves centroids")
    {
        std::stringstream ss;
        REQUIRE(original.save(ss));

        DBSCANOperation loaded;
        REQUIRE(loaded.load(ss));

        CHECK(arma::approx_equal(
            original.centroids(), loaded.centroids(), "absdiff", 1e-10));
    }

    SECTION("save fails when not fitted")
    {
        DBSCANOperation unfitted;
        std::stringstream ss;
        CHECK_FALSE(unfitted.save(ss));
    }

    SECTION("load from corrupted stream fails")
    {
        std::stringstream ss("garbage data that is not a valid model");
        DBSCANOperation loaded;
        CHECK_FALSE(loaded.load(ss));
        CHECK_FALSE(loaded.isTrained());
    }
}

// ============================================================================
// Parameter variations
// ============================================================================

TEST_CASE("DBSCANOperation - parameter variations", "[MLCore][DBSCAN]")
{
    auto data = makeClusterData(60, /*seed=*/42);

    SECTION("larger epsilon merges clusters")
    {
        DBSCANOperation dbscan;
        DBSCANParameters params;
        params.epsilon = 100.0;
        params.min_points = 2;

        REQUIRE(dbscan.fit(data.features, &params));
        CHECK(dbscan.numClasses() == 1);
    }

    SECTION("smaller epsilon with high min_points produces more noise")
    {
        DBSCANOperation dbscan;
        DBSCANParameters params;
        params.epsilon = 0.01;
        params.min_points = 50;

        REQUIRE(dbscan.fit(data.features, &params));
        // With very tight epsilon and high min_points
        CHECK(dbscan.numNoisePoints() > 0);
    }

    SECTION("reasonable parameters find 3 clusters")
    {
        DBSCANOperation dbscan;
        DBSCANParameters params;
        params.epsilon = 2.0;
        params.min_points = 3;

        REQUIRE(dbscan.fit(data.features, &params));
        CHECK(dbscan.numClasses() == 3);
        CHECK(dbscan.numNoisePoints() == 0);
    }

    SECTION("wrong parameter type falls back to defaults")
    {
        DBSCANOperation dbscan;
        RandomForestParameters wrong_params;
        bool ok = dbscan.fit(data.features, &wrong_params);
        CHECK(ok);
        CHECK(dbscan.isTrained());
    }

    SECTION("different min_points with fixed epsilon")
    {
        for (std::size_t mp : {2, 3, 5, 10}) {
            DBSCANOperation dbscan;
            DBSCANParameters params;
            params.epsilon = 2.0;
            params.min_points = mp;

            REQUIRE(dbscan.fit(data.features, &params));
            CHECK(dbscan.isTrained());

            arma::Row<std::size_t> assignments;
            REQUIRE(dbscan.assignClusters(data.features, assignments));
            CHECK(assignments.n_elem == data.features.n_cols);
        }
    }
}

// ============================================================================
// Registry integration
// ============================================================================

TEST_CASE("DBSCANOperation - registry integration", "[MLCore][DBSCAN]")
{
    MLModelRegistry registry;

    SECTION("model is registered as built-in")
    {
        CHECK(registry.hasModel("DBSCAN"));
    }

    SECTION("found under Clustering task")
    {
        auto names = registry.getModelNames(MLTaskType::Clustering);
        CHECK(std::find(names.begin(), names.end(), "DBSCAN") != names.end());
    }

    SECTION("not listed under BinaryClassification task")
    {
        auto names = registry.getModelNames(MLTaskType::BinaryClassification);
        CHECK(std::find(names.begin(), names.end(), "DBSCAN") == names.end());
    }

    SECTION("not listed under MultiClassClassification task")
    {
        auto names = registry.getModelNames(MLTaskType::MultiClassClassification);
        CHECK(std::find(names.begin(), names.end(), "DBSCAN") == names.end());
    }

    SECTION("create returns a fresh instance")
    {
        auto model = registry.create("DBSCAN");
        REQUIRE(model != nullptr);
        CHECK(model->getName() == "DBSCAN");
        CHECK_FALSE(model->isTrained());
    }

    SECTION("created instances are independent")
    {
        auto model1 = registry.create("DBSCAN");
        auto model2 = registry.create("DBSCAN");
        REQUIRE(model1 != nullptr);
        REQUIRE(model2 != nullptr);

        auto data = makeClusterData(30);
        DBSCANParameters params;
        params.epsilon = 2.0;
        params.min_points = 3;
        model1->fit(data.features, &params);

        CHECK(model1->isTrained());
        CHECK_FALSE(model2->isTrained());
    }

    SECTION("registry contains RF, NB, LR, K-Means, and DBSCAN")
    {
        CHECK(registry.hasModel("Random Forest"));
        CHECK(registry.hasModel("Naive Bayes"));
        CHECK(registry.hasModel("Logistic Regression"));
        CHECK(registry.hasModel("K-Means"));
        CHECK(registry.hasModel("DBSCAN"));
        CHECK(registry.size() >= 5);
    }
}
