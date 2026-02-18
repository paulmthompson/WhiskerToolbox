/**
 * @file GaussianMixtureOperation.test.cpp
 * @brief Catch2 tests for MLCore::GaussianMixtureOperation
 *
 * Tests the MLCore-native Gaussian Mixture Model clustering implementation
 * against synthetic data with known cluster structure. Covers:
 * - Metadata (name, task type, default parameters)
 * - Fitting on synthetic clustered data
 * - Cluster assignment accuracy on well-separated clusters
 * - Soft (probabilistic) cluster assignments via clusterProbabilities()
 * - Mean and covariance correctness after fitting
 * - Feature dimension validation
 * - Edge cases (empty data, unfitted model, invalid parameters)
 * - Save/load round-trip
 * - Parameter variations (k, max_iterations, seed)
 * - Registry integration
 *
 * Key difference from K-Means: GMM provides soft cluster assignments
 * (probabilities per component) and models clusters with full covariance
 * matrices, allowing ellipsoidal cluster shapes.
 */

#include "models/unsupervised/GaussianMixtureOperation.hpp"
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
// Helper: create well-separated 2D cluster data for GMM
// ============================================================================

namespace {

/**
 * @brief Generate well-separated 2D data with known clusters
 *
 * Cluster 0: centered around (-5, -5)
 * Cluster 1: centered around (+5, +5)
 * Cluster 2: centered around (+5, -5)
 *
 * Well-separated clusters so GMM should recover them easily.
 */
struct SyntheticClusterData {
    arma::mat features;                  // 2 × (3 * n_per_cluster)
    arma::Row<std::size_t> true_labels;  // ground truth cluster labels
    std::size_t num_clusters = 3;
};

SyntheticClusterData makeClusterData(std::size_t n_per_cluster = 50, int seed = 42)
{
    arma::arma_rng::set_seed(seed);

    arma::mat cluster0 = arma::randn(2, n_per_cluster) * 0.5;
    cluster0.row(0) -= 5.0;
    cluster0.row(1) -= 5.0;

    arma::mat cluster1 = arma::randn(2, n_per_cluster) * 0.5;
    cluster1.row(0) += 5.0;
    cluster1.row(1) += 5.0;

    arma::mat cluster2 = arma::randn(2, n_per_cluster) * 0.5;
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

    arma::mat cluster0 = arma::randn(2, n_per_cluster) * 0.5;
    cluster0.row(0) -= 5.0;
    cluster0.row(1) -= 5.0;

    arma::mat cluster1 = arma::randn(2, n_per_cluster) * 0.5;
    cluster1.row(0) += 5.0;
    cluster1.row(1) += 5.0;

    SyntheticBinaryClusterData data;
    data.features = arma::join_horiz(cluster0, cluster1);
    data.true_labels = arma::join_horiz(
        arma::Row<std::size_t>(n_per_cluster, arma::fill::zeros),
        arma::ones<arma::Row<std::size_t>>(n_per_cluster));

    return data;
}

/**
 * @brief Compute cluster assignment accuracy accounting for label permutation.
 *
 * GMM cluster labels are arbitrary (component 0 in GMM may correspond to
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

} // anonymous namespace

// ============================================================================
// Metadata tests
// ============================================================================

TEST_CASE("GaussianMixtureOperation - metadata", "[MLCore][GMM]")
{
    GaussianMixtureOperation gmm;

    SECTION("name is 'Gaussian Mixture Model'")
    {
        CHECK(gmm.getName() == "Gaussian Mixture Model");
    }

    SECTION("task type is Clustering")
    {
        CHECK(gmm.getTaskType() == MLTaskType::Clustering);
    }

    SECTION("default parameters are GMMParameters")
    {
        auto params = gmm.getDefaultParameters();
        REQUIRE(params != nullptr);

        auto const * gmm_params = dynamic_cast<GMMParameters const *>(params.get());
        REQUIRE(gmm_params != nullptr);

        CHECK(gmm_params->k == 3);
        CHECK(gmm_params->max_iterations == 300);
        CHECK_FALSE(gmm_params->seed.has_value());
    }

    SECTION("not trained initially")
    {
        CHECK_FALSE(gmm.isTrained());
        CHECK(gmm.numClasses() == 0);
        CHECK(gmm.numFeatures() == 0);
    }

    SECTION("accessors are empty before fitting")
    {
        CHECK(gmm.means().empty());
        CHECK(gmm.covariances().empty());
        CHECK(gmm.weights().empty());
    }

    SECTION("supervised methods return false")
    {
        arma::mat features = arma::randn(2, 10);
        arma::Row<std::size_t> labels(10, arma::fill::zeros);
        arma::Row<std::size_t> predictions;
        arma::mat probs;

        CHECK_FALSE(gmm.train(features, labels, nullptr));
        CHECK_FALSE(gmm.predict(features, predictions));
        CHECK_FALSE(gmm.predictProbabilities(features, probs));
    }
}

// ============================================================================
// Fitting tests
// ============================================================================

TEST_CASE("GaussianMixtureOperation - fitting", "[MLCore][GMM]")
{
    GaussianMixtureOperation gmm;
    auto data = makeClusterData(50);

    SECTION("fit with default parameters (nullptr)")
    {
        bool ok = gmm.fit(data.features, nullptr);
        CHECK(ok);
        CHECK(gmm.isTrained());
        CHECK(gmm.numClasses() == 3);  // default k=3
        CHECK(gmm.numFeatures() == 2);
    }

    SECTION("fit with explicit parameters")
    {
        GMMParameters params;
        params.k = 3;
        params.max_iterations = 500;
        params.seed = 42;

        bool ok = gmm.fit(data.features, &params);
        CHECK(ok);
        CHECK(gmm.isTrained());
        CHECK(gmm.numClasses() == 3);
        CHECK(gmm.numFeatures() == 2);
    }

    SECTION("means have correct dimensions after fitting")
    {
        GMMParameters params;
        params.k = 3;
        params.seed = 42;

        REQUIRE(gmm.fit(data.features, &params));

        auto const & means = gmm.means();
        REQUIRE(means.size() == 3);
        for (auto const & mean : means) {
            CHECK(mean.n_elem == 2);  // 2D features
        }
    }

    SECTION("covariances have correct dimensions after fitting")
    {
        GMMParameters params;
        params.k = 3;
        params.seed = 42;

        REQUIRE(gmm.fit(data.features, &params));

        auto const & covs = gmm.covariances();
        REQUIRE(covs.size() == 3);
        for (auto const & cov : covs) {
            CHECK(cov.n_rows == 2);
            CHECK(cov.n_cols == 2);
        }
    }

    SECTION("weights sum to 1.0 after fitting")
    {
        GMMParameters params;
        params.k = 3;
        params.seed = 42;

        REQUIRE(gmm.fit(data.features, &params));

        auto const & weights = gmm.weights();
        CHECK(weights.n_elem == 3);
        CHECK_THAT(arma::sum(weights), Catch::Matchers::WithinAbs(1.0, 1e-10));

        // All weights should be positive
        for (std::size_t i = 0; i < weights.n_elem; ++i) {
            CHECK(weights(i) > 0.0);
        }
    }

    SECTION("fit with k=2")
    {
        GMMParameters params;
        params.k = 2;
        params.seed = 42;

        bool ok = gmm.fit(data.features, &params);
        CHECK(ok);
        CHECK(gmm.numClasses() == 2);
    }

    SECTION("refitting resets model")
    {
        GMMParameters params;
        params.k = 3;
        params.seed = 42;
        gmm.fit(data.features, &params);
        CHECK(gmm.numClasses() == 3);

        // Refit with different k
        params.k = 2;
        bool ok = gmm.fit(data.features, &params);
        CHECK(ok);
        CHECK(gmm.numClasses() == 2);
    }

    SECTION("fit with k=1 (degenerate case)")
    {
        GMMParameters params;
        params.k = 1;
        params.seed = 42;

        bool ok = gmm.fit(data.features, &params);
        CHECK(ok);
        CHECK(gmm.numClasses() == 1);
    }
}

// ============================================================================
// Fit error handling
// ============================================================================

TEST_CASE("GaussianMixtureOperation - fit error handling", "[MLCore][GMM]")
{
    GaussianMixtureOperation gmm;

    SECTION("empty features")
    {
        arma::mat empty_features;
        CHECK_FALSE(gmm.fit(empty_features, nullptr));
        CHECK_FALSE(gmm.isTrained());
    }

    SECTION("k = 0")
    {
        arma::mat features = arma::randn(2, 10);
        GMMParameters params;
        params.k = 0;
        CHECK_FALSE(gmm.fit(features, &params));
        CHECK_FALSE(gmm.isTrained());
    }

    SECTION("k > number of observations")
    {
        arma::mat features = arma::randn(2, 5);
        GMMParameters params;
        params.k = 10;
        CHECK_FALSE(gmm.fit(features, &params));
        CHECK_FALSE(gmm.isTrained());
    }
}

// ============================================================================
// Cluster assignment accuracy
// ============================================================================

TEST_CASE("GaussianMixtureOperation - cluster assignment on well-separated data",
          "[MLCore][GMM]")
{
    GaussianMixtureOperation gmm;

    auto data = makeClusterData(50, /*seed=*/42);

    GMMParameters params;
    params.k = 3;
    params.seed = 42;

    REQUIRE(gmm.fit(data.features, &params));

    SECTION("high accuracy on fitting data (accounting for label permutation)")
    {
        arma::Row<std::size_t> assignments;
        bool ok = gmm.assignClusters(data.features, assignments);
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
        bool ok = gmm.assignClusters(test_data.features, assignments);
        REQUIRE(ok);

        double accuracy = permutedAccuracy(assignments, test_data.true_labels, 3);
        CHECK(accuracy > 0.90);
    }

    SECTION("k=2 on 2-cluster data achieves high accuracy")
    {
        auto binary_data = makeBinaryClusterData(60, /*seed=*/42);

        GaussianMixtureOperation gmm2;
        GMMParameters params2;
        params2.k = 2;
        params2.seed = 42;

        REQUIRE(gmm2.fit(binary_data.features, &params2));

        arma::Row<std::size_t> assignments;
        REQUIRE(gmm2.assignClusters(binary_data.features, assignments));

        double accuracy = permutedAccuracy(assignments, binary_data.true_labels, 2);
        CHECK(accuracy > 0.95);
    }
}

// ============================================================================
// Cluster assignment error handling
// ============================================================================

TEST_CASE("GaussianMixtureOperation - assignClusters error handling", "[MLCore][GMM]")
{
    GaussianMixtureOperation gmm;

    SECTION("assignClusters before fitting fails")
    {
        arma::mat features = arma::randn(2, 5);
        arma::Row<std::size_t> assignments;
        CHECK_FALSE(gmm.assignClusters(features, assignments));
    }

    SECTION("assignClusters with empty features fails")
    {
        auto data = makeClusterData(30);
        GMMParameters params;
        params.k = 3;
        params.seed = 42;
        gmm.fit(data.features, &params);

        arma::mat empty_features;
        arma::Row<std::size_t> assignments;
        CHECK_FALSE(gmm.assignClusters(empty_features, assignments));
    }

    SECTION("assignClusters with wrong feature dimension fails")
    {
        auto data = makeClusterData(30);
        GMMParameters params;
        params.k = 3;
        params.seed = 42;
        gmm.fit(data.features, &params);
        REQUIRE(gmm.numFeatures() == 2);

        arma::mat wrong_dim = arma::randn(5, 10);  // 5 features instead of 2
        arma::Row<std::size_t> assignments;
        CHECK_FALSE(gmm.assignClusters(wrong_dim, assignments));
    }
}

// ============================================================================
// Cluster probabilities (soft assignments)
// ============================================================================

TEST_CASE("GaussianMixtureOperation - cluster probabilities", "[MLCore][GMM]")
{
    GaussianMixtureOperation gmm;
    auto data = makeClusterData(50, /*seed=*/42);

    GMMParameters params;
    params.k = 3;
    params.seed = 42;
    REQUIRE(gmm.fit(data.features, &params));

    SECTION("probability matrix has correct dimensions")
    {
        arma::mat probs;
        REQUIRE(gmm.clusterProbabilities(data.features, probs));

        CHECK(probs.n_rows == 3);                    // k components
        CHECK(probs.n_cols == data.features.n_cols);  // n observations
    }

    SECTION("each column sums to 1.0")
    {
        arma::mat probs;
        REQUIRE(gmm.clusterProbabilities(data.features, probs));

        for (std::size_t i = 0; i < probs.n_cols; ++i) {
            CHECK_THAT(arma::sum(probs.col(i)), Catch::Matchers::WithinAbs(1.0, 1e-6));
        }
    }

    SECTION("all probabilities are in [0, 1]")
    {
        arma::mat probs;
        REQUIRE(gmm.clusterProbabilities(data.features, probs));

        CHECK(probs.min() >= 0.0);
        CHECK(probs.max() <= 1.0);
    }

    SECTION("dominant probability matches hard assignment")
    {
        arma::mat probs;
        REQUIRE(gmm.clusterProbabilities(data.features, probs));

        arma::Row<std::size_t> assignments;
        REQUIRE(gmm.assignClusters(data.features, assignments));

        // For well-separated data, the argmax of probabilities should match
        // the hard assignment
        std::size_t match_count = 0;
        for (std::size_t i = 0; i < probs.n_cols; ++i) {
            arma::uword argmax_component;
            probs.col(i).max(argmax_component);
            if (static_cast<std::size_t>(argmax_component) == assignments(i)) {
                ++match_count;
            }
        }
        double match_rate = static_cast<double>(match_count) / static_cast<double>(probs.n_cols);
        // With well-separated clusters, most should match
        CHECK(match_rate > 0.95);
    }

    SECTION("well-separated points have high-confidence assignments")
    {
        arma::mat probs;
        REQUIRE(gmm.clusterProbabilities(data.features, probs));

        // For well-separated clusters, the dominant probability should be high
        for (std::size_t i = 0; i < probs.n_cols; ++i) {
            double max_prob = probs.col(i).max();
            CHECK(max_prob > 0.8);
        }
    }

    SECTION("clusterProbabilities fails before fitting")
    {
        GaussianMixtureOperation unfitted;
        arma::mat probs;
        CHECK_FALSE(unfitted.clusterProbabilities(data.features, probs));
    }

    SECTION("clusterProbabilities fails with empty features")
    {
        arma::mat empty_features;
        arma::mat probs;
        CHECK_FALSE(gmm.clusterProbabilities(empty_features, probs));
    }

    SECTION("clusterProbabilities fails with wrong dimensions")
    {
        arma::mat wrong_dim = arma::randn(5, 10);
        arma::mat probs;
        CHECK_FALSE(gmm.clusterProbabilities(wrong_dim, probs));
    }
}

// ============================================================================
// Mean and covariance correctness
// ============================================================================

TEST_CASE("GaussianMixtureOperation - mean correctness", "[MLCore][GMM]")
{
    GaussianMixtureOperation gmm;

    auto data = makeClusterData(100, /*seed=*/42);

    GMMParameters params;
    params.k = 3;
    params.seed = 42;

    REQUIRE(gmm.fit(data.features, &params));

    SECTION("fitted means are near true cluster centers")
    {
        auto const & means = gmm.means();
        REQUIRE(means.size() == 3);

        // True centers: (-5,-5), (+5,+5), (+5,-5)
        arma::mat true_centers = {{-5.0, 5.0, 5.0},
                                  {-5.0, 5.0, -5.0}};

        for (std::size_t t = 0; t < 3; ++t) {
            double min_dist = std::numeric_limits<double>::max();
            for (std::size_t c = 0; c < 3; ++c) {
                double dist = arma::norm(means[c] - true_centers.col(t), 2);
                min_dist = std::min(min_dist, dist);
            }
            // Each true center should have a fitted mean within 1.5 units
            CHECK(min_dist < 1.5);
        }
    }

    SECTION("covariances are positive definite")
    {
        auto const & covs = gmm.covariances();
        REQUIRE(covs.size() == 3);

        for (std::size_t i = 0; i < 3; ++i) {
            // All eigenvalues should be positive for positive definite
            arma::vec eigenvalues = arma::eig_sym(covs[i]);
            for (std::size_t j = 0; j < eigenvalues.n_elem; ++j) {
                CHECK(eigenvalues(j) > 0.0);
            }
        }
    }
}

// ============================================================================
// Save / load round-trip
// ============================================================================

TEST_CASE("GaussianMixtureOperation - save/load round-trip", "[MLCore][GMM]")
{
    auto data = makeClusterData(60, /*seed=*/42);

    GaussianMixtureOperation original;
    GMMParameters params;
    params.k = 3;
    params.seed = 42;
    REQUIRE(original.fit(data.features, &params));

    arma::Row<std::size_t> original_assignments;
    REQUIRE(original.assignClusters(data.features, original_assignments));

    SECTION("save and load produces identical assignments")
    {
        std::stringstream ss;
        REQUIRE(original.save(ss));

        GaussianMixtureOperation loaded;
        CHECK_FALSE(loaded.isTrained());
        REQUIRE(loaded.load(ss));

        CHECK(loaded.isTrained());
        CHECK(loaded.numClasses() == original.numClasses());
        CHECK(loaded.numFeatures() == original.numFeatures());

        arma::Row<std::size_t> loaded_assignments;
        REQUIRE(loaded.assignClusters(data.features, loaded_assignments));
        CHECK(arma::all(loaded_assignments == original_assignments));
    }

    SECTION("save and load preserves means")
    {
        std::stringstream ss;
        REQUIRE(original.save(ss));

        GaussianMixtureOperation loaded;
        REQUIRE(loaded.load(ss));

        REQUIRE(original.means().size() == loaded.means().size());
        for (std::size_t i = 0; i < original.means().size(); ++i) {
            CHECK(arma::approx_equal(
                original.means()[i], loaded.means()[i], "absdiff", 1e-10));
        }
    }

    SECTION("save and load preserves covariances")
    {
        std::stringstream ss;
        REQUIRE(original.save(ss));

        GaussianMixtureOperation loaded;
        REQUIRE(loaded.load(ss));

        REQUIRE(original.covariances().size() == loaded.covariances().size());
        for (std::size_t i = 0; i < original.covariances().size(); ++i) {
            CHECK(arma::approx_equal(
                original.covariances()[i], loaded.covariances()[i], "absdiff", 1e-10));
        }
    }

    SECTION("save and load preserves weights")
    {
        std::stringstream ss;
        REQUIRE(original.save(ss));

        GaussianMixtureOperation loaded;
        REQUIRE(loaded.load(ss));

        CHECK(arma::approx_equal(
            original.weights(), loaded.weights(), "absdiff", 1e-10));
    }

    SECTION("save fails when not fitted")
    {
        GaussianMixtureOperation unfitted;
        std::stringstream ss;
        CHECK_FALSE(unfitted.save(ss));
    }

    SECTION("load from corrupted stream fails")
    {
        std::stringstream ss("garbage data that is not a valid model");
        GaussianMixtureOperation loaded;
        CHECK_FALSE(loaded.load(ss));
        CHECK_FALSE(loaded.isTrained());
    }
}

// ============================================================================
// Parameter variations
// ============================================================================

TEST_CASE("GaussianMixtureOperation - parameter variations", "[MLCore][GMM]")
{
    auto data = makeClusterData(60, /*seed=*/42);

    SECTION("different k values produce valid models")
    {
        for (std::size_t k : {2, 3, 4, 5}) {
            GaussianMixtureOperation gmm;
            GMMParameters params;
            params.k = k;
            params.seed = 42;

            REQUIRE(gmm.fit(data.features, &params));
            CHECK(gmm.isTrained());
            CHECK(gmm.numClasses() == k);

            arma::Row<std::size_t> assignments;
            REQUIRE(gmm.assignClusters(data.features, assignments));
            CHECK(assignments.n_elem == data.features.n_cols);

            // Verify all assignments are in [0, k)
            CHECK(arma::max(assignments) < k);

            // Verify means, covariances, weights sizes
            CHECK(gmm.means().size() == k);
            CHECK(gmm.covariances().size() == k);
            CHECK(gmm.weights().n_elem == k);
        }
    }

    SECTION("deterministic with same seed")
    {
        GMMParameters params;
        params.k = 3;
        params.seed = 42;

        GaussianMixtureOperation gmm1;
        REQUIRE(gmm1.fit(data.features, &params));

        GaussianMixtureOperation gmm2;
        REQUIRE(gmm2.fit(data.features, &params));

        // With the same seed, assignments should be identical
        arma::Row<std::size_t> assignments1, assignments2;
        REQUIRE(gmm1.assignClusters(data.features, assignments1));
        REQUIRE(gmm2.assignClusters(data.features, assignments2));
        CHECK(arma::all(assignments1 == assignments2));
    }

    SECTION("different seeds produce valid results")
    {
        GMMParameters params1;
        params1.k = 3;
        params1.seed = 42;

        GMMParameters params2;
        params2.k = 3;
        params2.seed = 7;

        GaussianMixtureOperation gmm1;
        REQUIRE(gmm1.fit(data.features, &params1));

        GaussianMixtureOperation gmm2;
        REQUIRE(gmm2.fit(data.features, &params2));

        // Both should produce valid assignments
        arma::Row<std::size_t> assignments1, assignments2;
        REQUIRE(gmm1.assignClusters(data.features, assignments1));
        REQUIRE(gmm2.assignClusters(data.features, assignments2));

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
        GaussianMixtureOperation gmm;
        RandomForestParameters wrong_params;
        bool ok = gmm.fit(data.features, &wrong_params);
        CHECK(ok);
        CHECK(gmm.isTrained());
        CHECK(gmm.numClasses() == 3);  // default k=3
    }

    SECTION("custom max_iterations fits successfully")
    {
        GaussianMixtureOperation gmm;
        GMMParameters params;
        params.k = 3;
        params.max_iterations = 10;
        params.seed = 42;

        bool ok = gmm.fit(data.features, &params);
        CHECK(ok);
        CHECK(gmm.isTrained());
    }
}

// ============================================================================
// Registry integration
// ============================================================================

TEST_CASE("GaussianMixtureOperation - registry integration", "[MLCore][GMM]")
{
    MLModelRegistry registry;

    SECTION("model is registered as built-in")
    {
        CHECK(registry.hasModel("Gaussian Mixture Model"));
    }

    SECTION("found under Clustering task")
    {
        auto names = registry.getModelNames(MLTaskType::Clustering);
        CHECK(std::find(names.begin(), names.end(), "Gaussian Mixture Model") != names.end());
    }

    SECTION("not listed under BinaryClassification task")
    {
        auto names = registry.getModelNames(MLTaskType::BinaryClassification);
        CHECK(std::find(names.begin(), names.end(), "Gaussian Mixture Model") == names.end());
    }

    SECTION("not listed under MultiClassClassification task")
    {
        auto names = registry.getModelNames(MLTaskType::MultiClassClassification);
        CHECK(std::find(names.begin(), names.end(), "Gaussian Mixture Model") == names.end());
    }

    SECTION("create returns a fresh instance")
    {
        auto model = registry.create("Gaussian Mixture Model");
        REQUIRE(model != nullptr);
        CHECK(model->getName() == "Gaussian Mixture Model");
        CHECK_FALSE(model->isTrained());
    }

    SECTION("created instances are independent")
    {
        auto model1 = registry.create("Gaussian Mixture Model");
        auto model2 = registry.create("Gaussian Mixture Model");
        REQUIRE(model1 != nullptr);
        REQUIRE(model2 != nullptr);

        auto data = makeClusterData(30);
        GMMParameters params;
        params.k = 3;
        params.seed = 42;
        model1->fit(data.features, &params);

        CHECK(model1->isTrained());
        CHECK_FALSE(model2->isTrained());
    }

    SECTION("registry contains all built-in models including GMM")
    {
        CHECK(registry.hasModel("Random Forest"));
        CHECK(registry.hasModel("Naive Bayes"));
        CHECK(registry.hasModel("Logistic Regression"));
        CHECK(registry.hasModel("K-Means"));
        CHECK(registry.hasModel("DBSCAN"));
        CHECK(registry.hasModel("Gaussian Mixture Model"));
        CHECK(registry.size() >= 6);
    }
}
