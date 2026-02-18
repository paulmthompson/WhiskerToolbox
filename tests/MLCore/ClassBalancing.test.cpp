
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "preprocessing/ClassBalancing.hpp"

#include <algorithm>
#include <armadillo>
#include <cstddef>
#include <map>
#include <numeric>
#include <set>

using Catch::Matchers::WithinRel;

// ============================================================================
// Helpers
// ============================================================================

namespace {

/**
 * @brief Count per-class samples in a label vector
 */
std::map<std::size_t, std::size_t> countClasses(arma::Row<std::size_t> const & labels) {
    std::map<std::size_t, std::size_t> counts;
    for (std::size_t i = 0; i < labels.n_elem; ++i) {
        counts[labels[i]]++;
    }
    return counts;
}

/**
 * @brief Create an imbalanced binary dataset
 *
 * 2 features, class 0 has n0 samples, class 1 has n1 samples.
 * Features are distinguishable by class (class 0 is centered at 0, class 1 at 10).
 */
std::pair<arma::mat, arma::Row<std::size_t>> makeImbalancedBinary(
    std::size_t n0, std::size_t n1)
{
    std::size_t total = n0 + n1;
    arma::mat features(2, total);
    arma::Row<std::size_t> labels(total);

    for (std::size_t i = 0; i < n0; ++i) {
        features(0, i) = static_cast<double>(i);
        features(1, i) = static_cast<double>(i) * 0.5;
        labels[i] = 0;
    }
    for (std::size_t i = 0; i < n1; ++i) {
        features(0, n0 + i) = 10.0 + static_cast<double>(i);
        features(1, n0 + i) = 10.0 + static_cast<double>(i) * 0.5;
        labels[n0 + i] = 1;
    }
    return {features, labels};
}

/**
 * @brief Create an imbalanced 3-class dataset
 */
std::pair<arma::mat, arma::Row<std::size_t>> makeImbalanced3Class(
    std::size_t n0, std::size_t n1, std::size_t n2)
{
    std::size_t total = n0 + n1 + n2;
    arma::mat features(2, total);
    arma::Row<std::size_t> labels(total);

    std::size_t idx = 0;
    for (std::size_t i = 0; i < n0; ++i, ++idx) {
        features(0, idx) = static_cast<double>(i);
        features(1, idx) = 0.0;
        labels[idx] = 0;
    }
    for (std::size_t i = 0; i < n1; ++i, ++idx) {
        features(0, idx) = 10.0 + static_cast<double>(i);
        features(1, idx) = 10.0;
        labels[idx] = 1;
    }
    for (std::size_t i = 0; i < n2; ++i, ++idx) {
        features(0, idx) = 20.0 + static_cast<double>(i);
        features(1, idx) = 20.0;
        labels[idx] = 2;
    }
    return {features, labels};
}

} // anonymous namespace

// ============================================================================
// getClassDistribution
// ============================================================================

TEST_CASE("getClassDistribution: basic counts", "[ClassBalancing]") {
    arma::Row<std::size_t> labels = {0, 1, 1, 0, 2, 1};
    auto dist = MLCore::getClassDistribution(labels);

    REQUIRE(dist.size() == 3);
    // Returns sorted pairs (label, count)
    REQUIRE(dist[0] == std::pair<std::size_t, std::size_t>{0, 2});
    REQUIRE(dist[1] == std::pair<std::size_t, std::size_t>{1, 3});
    REQUIRE(dist[2] == std::pair<std::size_t, std::size_t>{2, 1});
}

TEST_CASE("getClassDistribution: empty labels", "[ClassBalancing]") {
    arma::Row<std::size_t> labels;
    auto dist = MLCore::getClassDistribution(labels);
    REQUIRE(dist.empty());
}

// ============================================================================
// isBalanced
// ============================================================================

TEST_CASE("isBalanced: perfectly balanced", "[ClassBalancing]") {
    arma::Row<std::size_t> labels = {0, 1, 0, 1};
    REQUIRE(MLCore::isBalanced(labels, 1.0));
}

TEST_CASE("isBalanced: imbalanced 2:1", "[ClassBalancing]") {
    arma::Row<std::size_t> labels = {0, 0, 1};
    REQUIRE_FALSE(MLCore::isBalanced(labels, 1.0));
    REQUIRE(MLCore::isBalanced(labels, 2.0));
}

TEST_CASE("isBalanced: single class is balanced", "[ClassBalancing]") {
    arma::Row<std::size_t> labels = {0, 0, 0};
    REQUIRE(MLCore::isBalanced(labels, 1.0));
}

TEST_CASE("isBalanced: empty is balanced", "[ClassBalancing]") {
    arma::Row<std::size_t> labels;
    REQUIRE(MLCore::isBalanced(labels));
}

// ============================================================================
// balanceClasses — Subsample
// ============================================================================

TEST_CASE("Subsample: equal-size result on imbalanced binary data", "[ClassBalancing][Subsample]") {
    auto [features, labels] = makeImbalancedBinary(100, 20);

    MLCore::BalancingConfig config;
    config.strategy = MLCore::BalancingStrategy::Subsample;
    config.max_ratio = 1.0;
    config.random_seed = 42;

    auto result = MLCore::balanceClasses(features, labels, config);

    REQUIRE(result.was_balanced);
    // Each class should have 20 samples (the minority count)
    auto counts = countClasses(result.labels);
    REQUIRE(counts[0] == 20);
    REQUIRE(counts[1] == 20);
    REQUIRE(result.features.n_cols == 40);
    REQUIRE(result.labels.n_elem == 40);
    REQUIRE(result.features.n_rows == 2);
}

TEST_CASE("Subsample: max_ratio > 1 allows larger classes", "[ClassBalancing][Subsample]") {
    auto [features, labels] = makeImbalancedBinary(100, 10);

    MLCore::BalancingConfig config;
    config.strategy = MLCore::BalancingStrategy::Subsample;
    config.max_ratio = 2.0;
    config.random_seed = 42;

    auto result = MLCore::balanceClasses(features, labels, config);

    REQUIRE(result.was_balanced);
    // Target per class = min(10) * 2.0 = 20
    auto counts = countClasses(result.labels);
    REQUIRE(counts[0] == 20);
    REQUIRE(counts[1] == 10);  // only 10 available, can't exceed original
}

TEST_CASE("Subsample: 3-class imbalanced data", "[ClassBalancing][Subsample]") {
    auto [features, labels] = makeImbalanced3Class(50, 20, 10);

    MLCore::BalancingConfig config;
    config.strategy = MLCore::BalancingStrategy::Subsample;
    config.max_ratio = 1.0;
    config.random_seed = 123;

    auto result = MLCore::balanceClasses(features, labels, config);

    REQUIRE(result.was_balanced);
    auto counts = countClasses(result.labels);
    REQUIRE(counts[0] == 10);
    REQUIRE(counts[1] == 10);
    REQUIRE(counts[2] == 10);
    REQUIRE(result.features.n_cols == 30);
}

TEST_CASE("Subsample: already balanced data is not modified", "[ClassBalancing][Subsample]") {
    auto [features, labels] = makeImbalancedBinary(50, 50);

    MLCore::BalancingConfig config;
    config.strategy = MLCore::BalancingStrategy::Subsample;
    config.max_ratio = 1.0;
    config.random_seed = 42;

    auto result = MLCore::balanceClasses(features, labels, config);

    REQUIRE_FALSE(result.was_balanced);
    REQUIRE(result.features.n_cols == 100);
    REQUIRE(result.labels.n_elem == 100);
}

TEST_CASE("Subsample: deterministic with same seed", "[ClassBalancing][Subsample]") {
    auto [features, labels] = makeImbalancedBinary(100, 20);

    MLCore::BalancingConfig config;
    config.strategy = MLCore::BalancingStrategy::Subsample;
    config.max_ratio = 1.0;
    config.random_seed = 999;

    auto result1 = MLCore::balanceClasses(features, labels, config);
    auto result2 = MLCore::balanceClasses(features, labels, config);

    REQUIRE(arma::approx_equal(result1.features, result2.features, "absdiff", 1e-12));
    REQUIRE(arma::all(result1.labels == result2.labels));
}

TEST_CASE("Subsample: original_indices are valid", "[ClassBalancing][Subsample]") {
    auto [features, labels] = makeImbalancedBinary(80, 20);

    MLCore::BalancingConfig config;
    config.strategy = MLCore::BalancingStrategy::Subsample;
    config.max_ratio = 1.0;
    config.random_seed = 42;

    auto result = MLCore::balanceClasses(features, labels, config);

    REQUIRE(result.original_indices.size() == result.labels.n_elem);
    for (std::size_t i = 0; i < result.original_indices.size(); ++i) {
        std::size_t orig = result.original_indices[i];
        REQUIRE(orig < features.n_cols);
        // Feature data should match the original
        REQUIRE(arma::approx_equal(
            result.features.col(i), features.col(orig), "absdiff", 1e-12));
        // Label should match the original
        REQUIRE(result.labels[i] == labels[orig]);
    }
}

// ============================================================================
// balanceClasses — Oversample
// ============================================================================

TEST_CASE("Oversample: minority class duplicated to match majority", "[ClassBalancing][Oversample]") {
    auto [features, labels] = makeImbalancedBinary(50, 10);

    MLCore::BalancingConfig config;
    config.strategy = MLCore::BalancingStrategy::Oversample;
    config.random_seed = 42;

    auto result = MLCore::balanceClasses(features, labels, config);

    REQUIRE(result.was_balanced);
    auto counts = countClasses(result.labels);
    REQUIRE(counts[0] == 50);
    REQUIRE(counts[1] == 50);
    REQUIRE(result.features.n_cols == 100);
}

TEST_CASE("Oversample: already balanced data is not modified", "[ClassBalancing][Oversample]") {
    auto [features, labels] = makeImbalancedBinary(25, 25);

    MLCore::BalancingConfig config;
    config.strategy = MLCore::BalancingStrategy::Oversample;
    config.random_seed = 42;

    auto result = MLCore::balanceClasses(features, labels, config);

    REQUIRE_FALSE(result.was_balanced);
    REQUIRE(result.features.n_cols == 50);
}

TEST_CASE("Oversample: 3-class data", "[ClassBalancing][Oversample]") {
    auto [features, labels] = makeImbalanced3Class(30, 10, 5);

    MLCore::BalancingConfig config;
    config.strategy = MLCore::BalancingStrategy::Oversample;
    config.random_seed = 42;

    auto result = MLCore::balanceClasses(features, labels, config);

    REQUIRE(result.was_balanced);
    auto counts = countClasses(result.labels);
    REQUIRE(counts[0] == 30);
    REQUIRE(counts[1] == 30);
    REQUIRE(counts[2] == 30);
}

TEST_CASE("Oversample: deterministic with same seed", "[ClassBalancing][Oversample]") {
    auto [features, labels] = makeImbalancedBinary(60, 15);

    MLCore::BalancingConfig config;
    config.strategy = MLCore::BalancingStrategy::Oversample;
    config.random_seed = 777;

    auto result1 = MLCore::balanceClasses(features, labels, config);
    auto result2 = MLCore::balanceClasses(features, labels, config);

    REQUIRE(arma::approx_equal(result1.features, result2.features, "absdiff", 1e-12));
    REQUIRE(arma::all(result1.labels == result2.labels));
}

// ============================================================================
// Error cases
// ============================================================================

TEST_CASE("balanceClasses: dimension mismatch throws", "[ClassBalancing][Error]") {
    arma::mat features(2, 10, arma::fill::randn);
    arma::Row<std::size_t> labels(5, arma::fill::zeros);

    REQUIRE_THROWS_AS(
        MLCore::balanceClasses(features, labels),
        std::invalid_argument);
}

TEST_CASE("balanceClasses: empty data throws", "[ClassBalancing][Error]") {
    arma::mat features;
    arma::Row<std::size_t> labels;

    REQUIRE_THROWS_AS(
        MLCore::balanceClasses(features, labels),
        std::invalid_argument);
}

TEST_CASE("balanceClasses: max_ratio < 1.0 throws", "[ClassBalancing][Error]") {
    auto [features, labels] = makeImbalancedBinary(10, 5);

    MLCore::BalancingConfig config;
    config.max_ratio = 0.5;

    REQUIRE_THROWS_AS(
        MLCore::balanceClasses(features, labels, config),
        std::invalid_argument);
}

// ============================================================================
// toString
// ============================================================================

TEST_CASE("toString(BalancingStrategy)", "[ClassBalancing]") {
    REQUIRE(MLCore::toString(MLCore::BalancingStrategy::Subsample) == "Subsample");
    REQUIRE(MLCore::toString(MLCore::BalancingStrategy::Oversample) == "Oversample");
}
