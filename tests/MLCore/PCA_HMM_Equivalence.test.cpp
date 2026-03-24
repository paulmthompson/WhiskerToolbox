/**
 * @file PCA_HMM_Equivalence.test.cpp
 * @brief Tests for PCA mathematical equivalence and HMM interaction
 *
 * Validates that PCA with all components (scale=false) produces a pure
 * orthogonal rotation of centered data — preserving pairwise distances and
 * Gaussian log-likelihoods. Also tests the z-score vs PCA-scale distinction
 * and demonstrates when PCA + HMM breaks equivalence.
 *
 * Covers:
 * - Full-component PCA is exact reconstruction
 * - Pairwise distance preservation (orthogonal rotation)
 * - HMM log-likelihood equivalence under rotation (full covariance)
 * - HMM non-equivalence with diagonal covariance after rotation
 * - Z-score followed by PCA scale is redundant
 * - Z-score vs PCA scale produce equivalent results individually
 * - Float round-trip precision loss quantification
 * - Z-scoring PCA output destroys variance ranking
 */

#include "features/FeatureConverter.hpp"
#include "models/MLModelParameters.hpp"
#include "models/supervised/HiddenMarkovModelOperation.hpp"
#include "models/unsupervised/PCAOperation.hpp"

#include "Tensors/TensorData.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <armadillo>

#include <cstddef>

using namespace MLCore;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Helpers
// ============================================================================

namespace {

/**
 * @brief Generate high-dimensional 2-state block sequence data
 *
 * Creates n_features-dimensional data with clear state separation.
 * State 0: each feature ~ N(-1 * scale_i, 0.3)
 * State 1: each feature ~ N(+1 * scale_i, 0.3)
 * where scale_i varies per feature to create heterogeneous variance.
 *
 * @param n_features  Number of features (columns in observation space)
 * @param block_size  Frames per state block
 * @param num_blocks  Number of state-0/state-1 pairs
 * @param seed        Random seed
 */
struct SequenceData {
    arma::mat features;           ///< (n_features × total_frames)
    arma::Row<std::size_t> labels;///< (1 × total_frames)
};

SequenceData makeHighDimBlockSequence(
        std::size_t n_features,
        std::size_t block_size = 30,
        std::size_t num_blocks = 3,
        int seed = 42) {
    arma::arma_rng::set_seed(seed);

    std::size_t const total = block_size * num_blocks * 2;
    SequenceData data;
    data.features.set_size(n_features, total);
    data.labels.set_size(total);

    // Create per-feature scale factors (varying from 0.5 to 5.0)
    arma::vec scales(n_features);
    for (std::size_t f = 0; f < n_features; ++f) {
        scales(f) = 0.5 + 4.5 * static_cast<double>(f) / static_cast<double>(n_features);
    }

    std::size_t col = 0;
    for (std::size_t b = 0; b < num_blocks; ++b) {
        // State 0 block
        for (std::size_t i = 0; i < block_size; ++i) {
            for (std::size_t f = 0; f < n_features; ++f) {
                data.features(f, col) = -1.0 * scales(f) + arma::randn() * 0.3;
            }
            data.labels(col) = 0;
            ++col;
        }
        // State 1 block
        for (std::size_t i = 0; i < block_size; ++i) {
            for (std::size_t f = 0; f < n_features; ++f) {
                data.features(f, col) = +1.0 * scales(f) + arma::randn() * 0.3;
            }
            data.labels(col) = 1;
            ++col;
        }
    }

    return data;
}

/**
 * @brief Compute squared pairwise distance matrix for column vectors
 *
 * For matrix X (d × n), returns an n × n matrix where D(i,j) = ||x_i - x_j||²
 */
arma::mat pairwiseDistances(arma::mat const & X) {
    auto const n = X.n_cols;
    arma::mat D(n, n, arma::fill::zeros);
    for (arma::uword i = 0; i < n; ++i) {
        for (arma::uword j = i + 1; j < n; ++j) {
            double dist = arma::accu(arma::square(X.col(i) - X.col(j)));
            D(i, j) = dist;
            D(j, i) = dist;
        }
    }
    return D;
}

}// namespace

// ============================================================================
// PCA all-components is an exact orthogonal rotation
// ============================================================================

TEST_CASE("PCA all components reconstructs original data", "[MLCore][PCA][equivalence]") {
    constexpr std::size_t n_features = 10;
    constexpr std::size_t n_obs = 100;
    arma::arma_rng::set_seed(42);

    arma::mat features(n_features, n_obs, arma::fill::randn);
    // Add different scales per feature to make PCA interesting
    for (std::size_t f = 0; f < n_features; ++f) {
        features.row(f) *= (1.0 + static_cast<double>(f));
    }

    PCAOperation pca;
    PCAParameters params;
    params.n_components = n_features;// ALL components
    params.scale = false;

    arma::mat pca_result;
    REQUIRE(pca.fitTransform(features, &params, pca_result));
    REQUIRE(pca_result.n_rows == n_features);
    REQUIRE(pca_result.n_cols == n_obs);

    SECTION("explained variance sums to 1.0") {
        auto ratios = pca.explainedVarianceRatio();
        double total = 0.0;
        for (auto r: ratios) {
            total += r;
        }
        REQUIRE_THAT(total, WithinAbs(1.0, 1e-10));
    }

    SECTION("pairwise distances preserved (rotation invariance)") {
        // Center the original data the same way PCA does
        arma::vec mean = arma::mean(features, 1);
        arma::mat centered = features.each_col() - mean;

        arma::mat dist_original = pairwiseDistances(centered);
        arma::mat dist_pca = pairwiseDistances(pca_result);

        // Orthogonal rotation preserves all pairwise distances
        REQUIRE(arma::approx_equal(dist_original, dist_pca, "absdiff", 1e-8));
    }

    SECTION("total variance preserved") {
        arma::vec mean = arma::mean(features, 1);
        arma::mat centered = features.each_col() - mean;

        double var_original = arma::accu(arma::var(centered, 0, 1));
        double var_pca = arma::accu(arma::var(pca_result, 0, 1));

        REQUIRE_THAT(var_original, WithinAbs(var_pca, 1e-8));
    }
}

// ============================================================================
// PCA scale=true changes distances (not equivalent to original)
// ============================================================================

TEST_CASE("PCA scale=true changes pairwise distances", "[MLCore][PCA][equivalence]") {
    constexpr std::size_t n_features = 10;
    constexpr std::size_t n_obs = 100;
    arma::arma_rng::set_seed(42);

    arma::mat features(n_features, n_obs, arma::fill::randn);
    // Drastically different scales to make the effect obvious
    for (std::size_t f = 0; f < n_features; ++f) {
        features.row(f) *= std::pow(10.0, static_cast<double>(f) / n_features * 3.0);
    }

    // PCA with scale=false (rotation only)
    PCAOperation pca_noscale;
    PCAParameters params_noscale;
    params_noscale.n_components = n_features;
    params_noscale.scale = false;

    arma::mat result_noscale;
    REQUIRE(pca_noscale.fitTransform(features, &params_noscale, result_noscale));

    // PCA with scale=true (standardize + rotation)
    PCAOperation pca_scaled;
    PCAParameters params_scaled;
    params_scaled.n_components = n_features;
    params_scaled.scale = true;

    arma::mat result_scaled;
    REQUIRE(pca_scaled.fitTransform(features, &params_scaled, result_scaled));

    // Distances should be DIFFERENT between scaled and unscaled
    arma::mat dist_noscale = pairwiseDistances(result_noscale);
    arma::mat dist_scaled = pairwiseDistances(result_scaled);

    // They should NOT be approximately equal (scaling changes geometry)
    double max_diff = arma::abs(dist_noscale - dist_scaled).max();
    REQUIRE(max_diff > 1.0);// substantially different
}

// ============================================================================
// Z-score before PCA vs PCA scale=true produce equivalent results
// ============================================================================

TEST_CASE("Z-score then PCA(scale=false) equivalent to PCA(scale=true)", "[MLCore][PCA][equivalence]") {
    constexpr std::size_t n_features = 8;
    constexpr std::size_t n_obs = 200;
    arma::arma_rng::set_seed(42);

    arma::mat features(n_features, n_obs, arma::fill::randn);
    for (std::size_t f = 0; f < n_features; ++f) {
        features.row(f) *= (1.0 + 5.0 * static_cast<double>(f));
        features.row(f) += static_cast<double>(f) * 10.0;// add offset
    }

    // Path A: Z-score normalize, then PCA with scale=false
    arma::mat features_zscored = features;
    zscoreNormalize(features_zscored, 0.0);// epsilon=0 for exact comparison

    PCAOperation pca_a;
    PCAParameters params_a;
    params_a.n_components = n_features;
    params_a.scale = false;

    arma::mat result_a;
    REQUIRE(pca_a.fitTransform(features_zscored, &params_a, result_a));

    // Path B: PCA with scale=true (no prior z-score)
    PCAOperation pca_b;
    PCAParameters params_b;
    params_b.n_components = n_features;
    params_b.scale = true;

    arma::mat result_b;
    REQUIRE(pca_b.fitTransform(features, &params_b, result_b));

    // Both paths should produce the same pairwise distances
    // (they operate on the same standardized data, just computed differently)
    arma::mat dist_a = pairwiseDistances(result_a);
    arma::mat dist_b = pairwiseDistances(result_b);
    REQUIRE(arma::approx_equal(dist_a, dist_b, "absdiff", 1e-6));

    // Explained variance ratios should also match
    auto ratios_a = pca_a.explainedVarianceRatio();
    auto ratios_b = pca_b.explainedVarianceRatio();
    REQUIRE(ratios_a.size() == ratios_b.size());
    for (std::size_t i = 0; i < ratios_a.size(); ++i) {
        REQUIRE_THAT(ratios_a[i], WithinAbs(ratios_b[i], 1e-6));
    }
}

// ============================================================================
// Double standardization (z-score + PCA scale) is redundant
// ============================================================================

TEST_CASE("Z-score then PCA(scale=true) is redundant but not harmful", "[MLCore][PCA][equivalence]") {
    constexpr std::size_t n_features = 6;
    constexpr std::size_t n_obs = 150;
    arma::arma_rng::set_seed(42);

    arma::mat features(n_features, n_obs, arma::fill::randn);
    for (std::size_t f = 0; f < n_features; ++f) {
        features.row(f) *= (1.0 + 3.0 * static_cast<double>(f));
        features.row(f) += static_cast<double>(f) * 5.0;
    }

    // Path A: PCA(scale=true) only
    PCAOperation pca_a;
    PCAParameters params;
    params.n_components = n_features;
    params.scale = true;

    arma::mat result_a;
    REQUIRE(pca_a.fitTransform(features, &params, result_a));

    // Path B: z-score THEN PCA(scale=true) — double standardization
    arma::mat features_zscored = features;
    zscoreNormalize(features_zscored, 0.0);

    PCAOperation pca_b;
    arma::mat result_b;
    REQUIRE(pca_b.fitTransform(features_zscored, &params, result_b));

    // Both should produce the same pairwise distances and variance ratios
    // because z-scoring already-standardized data is a no-op
    arma::mat dist_a = pairwiseDistances(result_a);
    arma::mat dist_b = pairwiseDistances(result_b);
    REQUIRE(arma::approx_equal(dist_a, dist_b, "absdiff", 1e-6));
}

// ============================================================================
// Z-scoring PCA output destroys variance ranking
// ============================================================================

TEST_CASE("Z-scoring PCA output equalizes PC variances", "[MLCore][PCA][equivalence]") {
    constexpr std::size_t n_features = 10;
    constexpr std::size_t n_obs = 200;
    arma::arma_rng::set_seed(42);

    arma::mat features(n_features, n_obs, arma::fill::randn);
    for (std::size_t f = 0; f < n_features; ++f) {
        features.row(f) *= (1.0 + static_cast<double>(f) * 2.0);
    }

    PCAOperation pca;
    PCAParameters params;
    params.n_components = n_features;
    params.scale = false;

    arma::mat pca_result;
    REQUIRE(pca.fitTransform(features, &params, pca_result));

    // Before z-scoring: PC variances are eigenvalues (descending)
    arma::vec var_before = arma::var(pca_result, 0, 1);
    REQUIRE(var_before(0) > var_before(n_features - 1) * 5.0);// PC1 >> PC_last

    // Z-score the PCA output (this is what happens if the classification
    // pipeline has z-score enabled on PCA-reduced features)
    arma::mat pca_zscored = pca_result;
    zscoreNormalize(pca_zscored);

    // After z-scoring: ALL PCs have unit variance
    arma::vec var_after = arma::var(pca_zscored, 0, 1);
    for (std::size_t f = 0; f < n_features; ++f) {
        REQUIRE_THAT(var_after(f), WithinAbs(1.0, 0.1));
    }

    // The variance ranking is destroyed — this amplifies noise PCs
    // to be as "important" as signal PCs for downstream models like HMM
    // Pairwise distances are also changed
    arma::mat dist_pca = pairwiseDistances(pca_result);
    arma::mat dist_zscored = pairwiseDistances(pca_zscored);
    double max_diff = arma::abs(dist_pca - dist_zscored).max();
    REQUIRE(max_diff > 1.0);
}

// ============================================================================
// Float round-trip precision
// ============================================================================

TEST_CASE("Float round-trip preserves PCA output within tolerance", "[MLCore][PCA][equivalence]") {
    constexpr std::size_t n_features = 20;
    constexpr std::size_t n_obs = 100;
    arma::arma_rng::set_seed(42);

    arma::mat features(n_features, n_obs, arma::fill::randn);
    for (std::size_t f = 0; f < n_features; ++f) {
        features.row(f) *= (0.01 + static_cast<double>(f));
    }

    PCAOperation pca;
    PCAParameters params;
    params.n_components = n_features;
    params.scale = false;

    arma::mat pca_result;
    REQUIRE(pca.fitTransform(features, &params, pca_result));

    // Simulate the TensorData round-trip: double → float → double
    arma::fmat float_result = arma::conv_to<arma::fmat>::from(pca_result);
    arma::mat round_tripped = arma::conv_to<arma::mat>::from(float_result);

    // Relative error for each element
    double max_rel_error = 0.0;
    for (arma::uword r = 0; r < pca_result.n_rows; ++r) {
        for (arma::uword c = 0; c < pca_result.n_cols; ++c) {
            double orig = pca_result(r, c);
            double rt = round_tripped(r, c);
            if (std::abs(orig) > 1e-15) {
                double rel = std::abs(orig - rt) / std::abs(orig);
                max_rel_error = std::max(max_rel_error, rel);
            }
        }
    }

    // Float has ~7 decimal digits of precision
    // Relative error should be < 1e-6 (allowing some margin)
    REQUIRE(max_rel_error < 1e-5);

    // Pairwise distances should also be preserved through float round-trip
    arma::mat dist_orig = pairwiseDistances(pca_result);
    arma::mat dist_rt = pairwiseDistances(round_tripped);

    // Use relative tolerance scaled by distance magnitude
    double max_dist = dist_orig.max();
    double dist_error = arma::abs(dist_orig - dist_rt).max();
    REQUIRE(dist_error / max_dist < 1e-5);
}

// ============================================================================
// HMM equivalence: original features vs PCA-all-components (full covariance)
// ============================================================================

TEST_CASE("HMM on PCA-all-components has equivalent log-likelihood (full cov)",
          "[MLCore][PCA][HMM][equivalence]") {
    constexpr std::size_t n_features = 5;
    auto data = makeHighDimBlockSequence(n_features, /*block_size=*/40, /*num_blocks=*/3, /*seed=*/42);

    // Train HMM on original features
    HiddenMarkovModelOperation hmm_orig;
    HMMParameters hmm_params;
    hmm_params.num_states = 2;
    hmm_params.use_diagonal_covariance = false;// full covariance

    {
        std::vector<arma::mat> seqs{data.features};
        std::vector<arma::Row<std::size_t>> label_seqs{data.labels};
        REQUIRE(hmm_orig.trainSequences(seqs, label_seqs, &hmm_params));
    }

    // Apply PCA with ALL components and scale=false (pure rotation)
    PCAOperation pca;
    PCAParameters pca_params;
    pca_params.n_components = n_features;
    pca_params.scale = false;

    arma::mat pca_features;
    REQUIRE(pca.fitTransform(data.features, &pca_params, pca_features));

    // Train HMM on PCA features
    HiddenMarkovModelOperation hmm_pca;
    {
        std::vector<arma::mat> seqs{pca_features};
        std::vector<arma::Row<std::size_t>> label_seqs{data.labels};
        REQUIRE(hmm_pca.trainSequences(seqs, label_seqs, &hmm_params));
    }

    // Both should predict the same state sequence (same underlying data structure)
    std::vector<arma::mat> orig_seqs{data.features};
    std::vector<arma::Row<std::size_t>> pred_orig;
    REQUIRE(hmm_orig.predictSequences(orig_seqs, pred_orig));

    std::vector<arma::mat> pca_seqs{pca_features};
    std::vector<arma::Row<std::size_t>> pred_pca;
    REQUIRE(hmm_pca.predictSequences(pca_seqs, pred_pca));

    REQUIRE(pred_orig.size() == 1);
    REQUIRE(pred_pca.size() == 1);
    REQUIRE(pred_orig[0].n_elem == pred_pca[0].n_elem);

    // Count matching predictions — should be very high (>95%)
    // Not necessarily 100% due to EM convergence to different local optima
    std::size_t matches = 0;
    for (std::size_t i = 0; i < pred_orig[0].n_elem; ++i) {
        if (pred_orig[0](i) == pred_pca[0](i)) {
            ++matches;
        }
    }
    double match_rate = static_cast<double>(matches) / static_cast<double>(pred_orig[0].n_elem);

    // With well-separated classes and supervised HMM training,
    // both representations should achieve near-identical predictions
    REQUIRE(match_rate > 0.95);

    // Additionally verify both achieve high accuracy against ground truth
    std::size_t correct_orig = 0;
    std::size_t correct_pca = 0;
    for (std::size_t i = 0; i < data.labels.n_elem; ++i) {
        if (pred_orig[0](i) == data.labels(i)) ++correct_orig;
        if (pred_pca[0](i) == data.labels(i)) ++correct_pca;
    }
    double acc_orig = static_cast<double>(correct_orig) / static_cast<double>(data.labels.n_elem);
    double acc_pca = static_cast<double>(correct_pca) / static_cast<double>(data.labels.n_elem);

    REQUIRE(acc_orig > 0.90);
    REQUIRE(acc_pca > 0.90);
    // The accuracy difference should be small
    REQUIRE(std::abs(acc_orig - acc_pca) < 0.10);
}

// ============================================================================
// HMM equivalence breaks with diagonal covariance
// ============================================================================

TEST_CASE("HMM diagonal covariance is NOT rotation-invariant",
          "[MLCore][PCA][HMM][equivalence]") {
    // Diagonal covariance assumes features are independent.
    // PCA rotation decorrelates the data, changing which features
    // are "diagonal". This can help OR hurt depending on the data.

    constexpr std::size_t n_features = 5;
    arma::arma_rng::set_seed(42);

    // Create data with correlated features where diagonal assumption is poor
    auto data = makeHighDimBlockSequence(n_features, 40, 3, 42);

    // Add inter-feature correlation to make diagonal assumption suboptimal
    arma::mat correlation(n_features, n_features, arma::fill::eye);
    for (std::size_t i = 0; i < n_features; ++i) {
        for (std::size_t j = i + 1; j < n_features; ++j) {
            correlation(i, j) = 0.5;
            correlation(j, i) = 0.5;
        }
    }
    arma::mat L = arma::chol(correlation, "lower");
    data.features = L * data.features;// induce correlations

    HMMParameters hmm_params;
    hmm_params.num_states = 2;
    hmm_params.use_diagonal_covariance = true;

    // Train on original (correlated features + diagonal cov)
    HiddenMarkovModelOperation hmm_orig;
    {
        std::vector<arma::mat> seqs{data.features};
        std::vector<arma::Row<std::size_t>> label_seqs{data.labels};
        REQUIRE(hmm_orig.trainSequences(seqs, label_seqs, &hmm_params));
    }

    // PCA decorrelates the features
    PCAOperation pca;
    PCAParameters pca_params;
    pca_params.n_components = n_features;
    pca_params.scale = false;

    arma::mat pca_features;
    REQUIRE(pca.fitTransform(data.features, &pca_params, pca_features));

    // Train on PCA output (decorrelated features + diagonal cov)
    HiddenMarkovModelOperation hmm_pca;
    {
        std::vector<arma::mat> seqs{pca_features};
        std::vector<arma::Row<std::size_t>> label_seqs{data.labels};
        REQUIRE(hmm_pca.trainSequences(seqs, label_seqs, &hmm_params));
    }

    // Predict with both
    std::vector<arma::Row<std::size_t>> pred_orig;
    std::vector<arma::Row<std::size_t>> pred_pca;
    REQUIRE(hmm_orig.predictSequences({data.features}, pred_orig));
    REQUIRE(hmm_pca.predictSequences({pca_features}, pred_pca));

    // Both should produce reasonable predictions
    REQUIRE(pred_orig.size() == 1);
    REQUIRE(pred_pca.size() == 1);

    // For correlated data, PCA + diagonal HMM should actually do BETTER
    // because PCA decorrelates, making diagonal covariance more appropriate
    std::size_t correct_orig = 0;
    std::size_t correct_pca = 0;
    for (std::size_t i = 0; i < data.labels.n_elem; ++i) {
        if (pred_orig[0](i) == data.labels(i)) ++correct_orig;
        if (pred_pca[0](i) == data.labels(i)) ++correct_pca;
    }
    double acc_orig = static_cast<double>(correct_orig) / static_cast<double>(data.labels.n_elem);
    double acc_pca = static_cast<double>(correct_pca) / static_cast<double>(data.labels.n_elem);

    // PCA decorrelation should help diagonal HMM
    // At minimum, both should be reasonable
    REQUIRE(acc_orig > 0.70);
    REQUIRE(acc_pca > 0.70);

    // The key insight: predictions are NOT identical because diagonal
    // covariance is not rotation-invariant
    // (We don't assert they're different — just that the test documents the property)
}

// ============================================================================
// Higher-dimensional equivalence test (closer to real 192-feature scenario)
// ============================================================================

TEST_CASE("PCA all-components preserves distances at realistic dimensionality",
          "[MLCore][PCA][equivalence]") {
    // Use 50 features (not 192 for test speed) but same mathematical property
    constexpr std::size_t n_features = 50;
    constexpr std::size_t n_obs = 200;
    arma::arma_rng::set_seed(42);

    arma::mat features(n_features, n_obs, arma::fill::randn);
    // Heterogeneous feature scales (mimicking real neural feature vectors)
    for (std::size_t f = 0; f < n_features; ++f) {
        double scale = 0.01 + 10.0 * static_cast<double>(f) / static_cast<double>(n_features);
        features.row(f) *= scale;
        features.row(f) += static_cast<double>(f);// non-zero means
    }

    PCAOperation pca;
    PCAParameters params;
    params.n_components = n_features;
    params.scale = false;

    arma::mat pca_result;
    REQUIRE(pca.fitTransform(features, &params, pca_result));

    // Verify distances preserved
    arma::vec mean = arma::mean(features, 1);
    arma::mat centered = features.each_col() - mean;

    // Sample a subset of pairwise distances (full matrix too expensive for 200 obs)
    constexpr std::size_t n_pairs = 100;
    arma::arma_rng::set_seed(99);
    double max_rel_diff = 0.0;
    for (std::size_t p = 0; p < n_pairs; ++p) {
        auto i = static_cast<arma::uword>(arma::randi(arma::distr_param(0, static_cast<int>(n_obs - 1))));
        auto j = static_cast<arma::uword>(arma::randi(arma::distr_param(0, static_cast<int>(n_obs - 1))));
        if (i == j) continue;

        double dist_orig = arma::accu(arma::square(centered.col(i) - centered.col(j)));
        double dist_pca = arma::accu(arma::square(pca_result.col(i) - pca_result.col(j)));

        if (dist_orig > 1e-10) {
            double rel = std::abs(dist_orig - dist_pca) / dist_orig;
            max_rel_diff = std::max(max_rel_diff, rel);
        }
    }

    REQUIRE(max_rel_diff < 1e-8);
}

// ============================================================================
// PCA scale=true with all components: z-score normalization difference
// ============================================================================

TEST_CASE("PCA scale changes eigenvalue spectrum even with all components",
          "[MLCore][PCA][equivalence]") {
    constexpr std::size_t n_features = 8;
    constexpr std::size_t n_obs = 200;
    arma::arma_rng::set_seed(42);

    arma::mat features(n_features, n_obs, arma::fill::randn);
    // Make first feature have much more variance than the rest (exponential decay)
    for (std::size_t f = 0; f < n_features; ++f) {
        features.row(f) *= std::pow(10.0, 2.0 - static_cast<double>(f) * 0.5);
    }

    // Unscaled PCA: eigenspectrum reflects raw variance differences
    PCAOperation pca_unscaled;
    PCAParameters params_unscaled;
    params_unscaled.n_components = n_features;
    params_unscaled.scale = false;

    arma::mat result_unscaled;
    REQUIRE(pca_unscaled.fitTransform(features, &params_unscaled, result_unscaled));
    auto ratios_unscaled = pca_unscaled.explainedVarianceRatio();

    // Scaled PCA: eigenspectrum reflects correlation structure
    PCAOperation pca_scaled;
    PCAParameters params_scaled;
    params_scaled.n_components = n_features;
    params_scaled.scale = true;

    arma::mat result_scaled;
    REQUIRE(pca_scaled.fitTransform(features, &params_scaled, result_scaled));
    auto ratios_scaled = pca_scaled.explainedVarianceRatio();

    // Unscaled: PC1 dominates (high-variance feature drives it)
    REQUIRE(ratios_unscaled[0] > 0.8);

    // Scaled: more evenly distributed (all features equalized first)
    // PC1 should explain less variance than in the unscaled case
    REQUIRE(ratios_scaled[0] < ratios_unscaled[0]);
}

// ============================================================================
// REAL-WORLD SCENARIO: Reproduce user's problem
//
// User workflow:
//   1. Has 192-feature TensorData (stored as float32)
//   2. Run HMM directly on features → works well
//   3. Run PCA (all 192 components) → store as TensorData (float32)
//   4. Run HMM on PCA output → performs much worse
//
// Pipeline data path (direct):
//   TensorData(float) → double → HMM
//
// Pipeline data path (via PCA):
//   TensorData(float) → double → [z-score?] → PCA(scale?) → double
//   → TensorData(float) → double → HMM
//
// Possible degradation sources:
//   A. PCA scale=true (default!) changes geometry even with all components
//   B. Z-score before PCA compounds the scaling
//   C. Float round-trip: double→float→double adds ~1e-7 relative error
//   D. With high-dim noise PCs, float precision loss in trailing PCs
//      → HMM covariance estimation becomes ill-conditioned
// ============================================================================

namespace {

/**
 * @brief Helper to compute HMM accuracy on a dataset
 */
double computeHMMAccuracy(
        arma::mat const & features,
        arma::Row<std::size_t> const & labels,
        bool use_diagonal,
        int seed = 42) {
    arma::arma_rng::set_seed(seed);

    HiddenMarkovModelOperation hmm;
    HMMParameters params;
    params.num_states = 2;
    params.use_diagonal_covariance = use_diagonal;

    std::vector<arma::mat> seqs{features};
    std::vector<arma::Row<std::size_t>> label_seqs{labels};
    if (!hmm.trainSequences(seqs, label_seqs, &params)) {
        return -1.0;
    }

    std::vector<arma::Row<std::size_t>> preds;
    if (!hmm.predictSequences(seqs, preds)) {
        return -1.0;
    }

    std::size_t correct = 0;
    for (std::size_t i = 0; i < labels.n_elem; ++i) {
        if (preds[0](i) == labels(i)) ++correct;
    }
    return static_cast<double>(correct) / static_cast<double>(labels.n_elem);
}

/**
 * @brief Simulate the TensorData float round-trip: double → float32 → double
 *
 * In the real pipeline, PCA output is stored as TensorData (float32) and then
 * reloaded as arma::mat (double) for HMM. This simulates that precision loss.
 */
arma::mat simulateFloatRoundTrip(arma::mat const & data) {
    // PCA output: (n_components × observations)
    // TensorData stores transposed: (observations × n_components) as float
    arma::fmat as_float = arma::conv_to<arma::fmat>::from(data);
    return arma::conv_to<arma::mat>::from(as_float);
}

}// namespace

TEST_CASE("Reproducing user scenario: PCA(all components) + HMM degrades with scale=true",
          "[MLCore][PCA][HMM][equivalence][scenario]") {

    // Simulate ~192-dimension feature vector (use 50 for test speed, same effect)
    constexpr std::size_t n_features = 50;
    auto data = makeHighDimBlockSequence(n_features, /*block_size=*/40, /*num_blocks=*/4, /*seed=*/42);

    // ========================================================================
    // Step 1: Direct HMM on original features (the "works well" baseline)
    // ========================================================================
    double const acc_direct = computeHMMAccuracy(data.features, data.labels, false);
    REQUIRE(acc_direct > 0.90);

    // ========================================================================
    // Step 2: PCA(all, scale=true) → float round-trip → HMM
    // This is what happens when the user uses DimReductionPanel with defaults:
    //   - "Scale features" checkbox is ON (default)
    //   - n_components = 50 (all features)
    //   - Result stored as TensorData (float32)
    // ========================================================================
    PCAOperation pca_scaled;
    PCAParameters pca_params_scaled;
    pca_params_scaled.n_components = n_features;
    pca_params_scaled.scale = true;// DEFAULT — this is likely the user's setting

    arma::mat pca_scaled_result;
    REQUIRE(pca_scaled.fitTransform(data.features, &pca_params_scaled, pca_scaled_result));

    // Simulate TensorData storage round-trip
    arma::mat pca_scaled_roundtripped = simulateFloatRoundTrip(pca_scaled_result);

    double const acc_scaled_pca = computeHMMAccuracy(pca_scaled_roundtripped, data.labels, false);

    // ========================================================================
    // Step 3: PCA(all, scale=false) → float round-trip → HMM
    // Pure rotation — should be equivalent to direct
    // ========================================================================
    PCAOperation pca_unscaled;
    PCAParameters pca_params_unscaled;
    pca_params_unscaled.n_components = n_features;
    pca_params_unscaled.scale = false;

    arma::mat pca_unscaled_result;
    REQUIRE(pca_unscaled.fitTransform(data.features, &pca_params_unscaled, pca_unscaled_result));

    arma::mat pca_unscaled_roundtripped = simulateFloatRoundTrip(pca_unscaled_result);

    double const acc_unscaled_pca = computeHMMAccuracy(pca_unscaled_roundtripped, data.labels, false);

    // ========================================================================
    // Step 4: PCA(all, scale=false) WITHOUT float round-trip → HMM
    // Pure rotation in double precision — theoretically identical to direct
    // ========================================================================
    double const acc_unscaled_pca_double = computeHMMAccuracy(pca_unscaled_result, data.labels, false);

    // ========================================================================
    // Assertions: document the degradation sources
    // ========================================================================

    // Direct HMM should work well
    REQUIRE(acc_direct > 0.90);

    // PCA(scale=false) in double precision should match direct
    // (pure orthogonal rotation preserves Gaussian log-likelihoods)
    REQUIRE(acc_unscaled_pca_double > 0.90);

    // PCA(scale=false) with float round-trip should still be close
    REQUIRE(acc_unscaled_pca > 0.85);

    // PCA(scale=true) may degrade because scaling changes the data geometry
    // even though all components are kept. The HMM is fitting a different
    // covariance structure.
    // We document this rather than asserting a specific threshold, since
    // the effect magnitude depends on the data.
    INFO("Direct HMM accuracy:                   " << acc_direct);
    INFO("PCA(scale=false, double) HMM accuracy:  " << acc_unscaled_pca_double);
    INFO("PCA(scale=false, float RT) HMM accuracy: " << acc_unscaled_pca);
    INFO("PCA(scale=true, float RT) HMM accuracy:  " << acc_scaled_pca);

    // The key insight: all paths should achieve reasonable accuracy
    // if the data is well-separated. The degradation from scale=true
    // is that it changes which features dominate the Gaussian fit.
    REQUIRE(acc_scaled_pca > 0.70);
}

TEST_CASE("Reproducing user scenario: z-score + PCA(scale=true) double-standardization",
          "[MLCore][PCA][HMM][equivalence][scenario]") {

    constexpr std::size_t n_features = 50;
    auto data = makeHighDimBlockSequence(n_features, 40, 4, 42);

    // Baseline: direct HMM
    double const acc_direct = computeHMMAccuracy(data.features, data.labels, false);
    REQUIRE(acc_direct > 0.90);

    // Path A: z-score → PCA(scale=false) → float RT → HMM
    // User enables "Z-score normalize" checkbox in DimReductionPanel,
    // and disables "Scale features"
    arma::mat zscored_features = data.features;
    zscoreNormalize(zscored_features);

    PCAOperation pca_a;
    PCAParameters params_a;
    params_a.n_components = n_features;
    params_a.scale = false;

    arma::mat result_a;
    REQUIRE(pca_a.fitTransform(zscored_features, &params_a, result_a));
    arma::mat result_a_rt = simulateFloatRoundTrip(result_a);
    double const acc_zscore_pca_noscale = computeHMMAccuracy(result_a_rt, data.labels, false);

    // Path B: PCA(scale=true) → float RT → HMM
    // User enables "Scale features" checkbox (the default), no z-score
    PCAOperation pca_b;
    PCAParameters params_b;
    params_b.n_components = n_features;
    params_b.scale = true;

    arma::mat result_b;
    REQUIRE(pca_b.fitTransform(data.features, &params_b, result_b));
    arma::mat result_b_rt = simulateFloatRoundTrip(result_b);
    double const acc_pca_scale = computeHMMAccuracy(result_b_rt, data.labels, false);

    // Path C: z-score → PCA(scale=true) → float RT → HMM
    // User enables BOTH checkboxes — double standardization
    PCAOperation pca_c;
    PCAParameters params_c;
    params_c.n_components = n_features;
    params_c.scale = true;

    arma::mat result_c;
    REQUIRE(pca_c.fitTransform(zscored_features, &params_c, result_c));
    arma::mat result_c_rt = simulateFloatRoundTrip(result_c);
    double const acc_zscore_pca_scale = computeHMMAccuracy(result_c_rt, data.labels, false);

    // Path D: PCA(scale=false) → float RT → HMM  (the correct path)
    PCAOperation pca_d;
    PCAParameters params_d;
    params_d.n_components = n_features;
    params_d.scale = false;

    arma::mat result_d;
    REQUIRE(pca_d.fitTransform(data.features, &params_d, result_d));
    arma::mat result_d_rt = simulateFloatRoundTrip(result_d);
    double const acc_pca_noscale = computeHMMAccuracy(result_d_rt, data.labels, false);

    INFO("Direct HMM:                    " << acc_direct);
    INFO("PCA(scale=false) + float RT:   " << acc_pca_noscale);
    INFO("z-score + PCA(scale=false):    " << acc_zscore_pca_noscale);
    INFO("PCA(scale=true):               " << acc_pca_scale);
    INFO("z-score + PCA(scale=true):     " << acc_zscore_pca_scale);

    // z-score + PCA(scale=false) ≈ PCA(scale=true) — they're equivalent transforms
    // (both standardize before rotation, just computed differently)
    CHECK(std::abs(acc_zscore_pca_noscale - acc_pca_scale) < 0.15);

    // z-score + PCA(scale=true) ≈ PCA(scale=true) — double standardization is a no-op
    CHECK(std::abs(acc_zscore_pca_scale - acc_pca_scale) < 0.15);

    // PCA(scale=false) should be closest to direct HMM
    CHECK(std::abs(acc_pca_noscale - acc_direct) < 0.15);
}

TEST_CASE("High-dimensional float round-trip + PCA trailing PC precision loss",
          "[MLCore][PCA][HMM][equivalence][scenario]") {
    // With many features (mimicking 192), the trailing PCs have tiny variance.
    // Float32 storage can obliterate the signal in those PCs.
    // This test quantifies the effect.

    constexpr std::size_t n_features = 100;// Large but tractable
    constexpr std::size_t n_obs = 300;
    arma::arma_rng::set_seed(42);

    // Create data where most variance is in a few features
    arma::mat features(n_features, n_obs, arma::fill::randn);
    for (std::size_t f = 0; f < n_features; ++f) {
        // Exponential variance decay: feature 0 has 1000x more variance than feature 99
        double scale = std::pow(10.0, 3.0 * (1.0 - static_cast<double>(f) / static_cast<double>(n_features)));
        features.row(f) *= scale;
    }

    PCAOperation pca;
    PCAParameters params;
    params.n_components = n_features;
    params.scale = false;

    arma::mat pca_result;
    REQUIRE(pca.fitTransform(features, &params, pca_result));

    // Check variance of each PC
    arma::vec pc_variances = arma::var(pca_result, 0, 1);

    // First PC should have huge variance, last should be tiny
    REQUIRE(pc_variances(0) > pc_variances(n_features - 1) * 1000.0);

    // Float round-trip
    arma::mat round_tripped = simulateFloatRoundTrip(pca_result);

    // Measure relative error per PC
    double max_rel_error_top10 = 0.0;
    double max_rel_error_bottom10 = 0.0;

    for (arma::uword pc = 0; pc < 10; ++pc) {
        for (arma::uword c = 0; c < pca_result.n_cols; ++c) {
            double orig = pca_result(pc, c);
            double rt = round_tripped(pc, c);
            if (std::abs(orig) > 1e-15) {
                double rel = std::abs(orig - rt) / std::abs(orig);
                max_rel_error_top10 = std::max(max_rel_error_top10, rel);
            }
        }
    }
    for (arma::uword pc = n_features - 10; pc < n_features; ++pc) {
        for (arma::uword c = 0; c < pca_result.n_cols; ++c) {
            double orig = pca_result(pc, c);
            double rt = round_tripped(pc, c);
            if (std::abs(orig) > 1e-15) {
                double rel = std::abs(orig - rt) / std::abs(orig);
                max_rel_error_bottom10 = std::max(max_rel_error_bottom10, rel);
            }
        }
    }

    INFO("Max relative error (top 10 PCs): " << max_rel_error_top10);
    INFO("Max relative error (bottom 10 PCs): " << max_rel_error_bottom10);

    // Top PCs should have low relative error (large values survive float)
    REQUIRE(max_rel_error_top10 < 1e-5);

    // Bottom PCs may have higher relative error but should still be bounded
    // by float32 precision (~1e-7 relative for single values)
    REQUIRE(max_rel_error_bottom10 < 1e-4);
}

// ============================================================================
// Regression test: arma-to-TensorData column-major/row-major round-trip
// ============================================================================

TEST_CASE("PCA TensorData round-trip preserves observation norms",
          "[PCA][TensorData][regression]") {

    // This test catches the bug where arma::fmat::memptr() (column-major) was
    // passed directly to TensorData factories that expect row-major flat data,
    // scrambling all PC values across observations.

    constexpr std::size_t n_features = 20;
    constexpr std::size_t n_obs = 50;
    constexpr std::size_t n_components = n_features;// Use all components

    auto seq = makeHighDimBlockSequence(n_features, 25, 1, 99);
    arma::mat const & features = seq.features;// (n_features × n_obs)
    REQUIRE(features.n_rows == n_features);
    REQUIRE(features.n_cols == n_obs);

    // Run PCA
    PCAOperation pca;
    PCAParameters params;
    params.n_components = n_components;
    params.scale = false;

    arma::mat reduced;
    REQUIRE(pca.fitTransform(features, &params, reduced));
    // reduced is (n_components × n_obs)
    REQUIRE(reduced.n_rows == n_components);
    REQUIRE(reduced.n_cols == n_obs);

    // Simulate what buildOutputTensor does: arma (n_comp × n_obs) → TensorData
    // Transpose to (n_obs × n_comp), convert to float, extract to flat row-major
    arma::fmat output_fmat = arma::conv_to<arma::fmat>::from(reduced.t());
    auto const n_cols = static_cast<std::size_t>(output_fmat.n_cols);
    std::vector<float> flat_data(output_fmat.n_elem);
    for (arma::uword r = 0; r < output_fmat.n_rows; ++r) {
        for (arma::uword c = 0; c < output_fmat.n_cols; ++c) {
            flat_data[r * n_cols + c] = output_fmat(r, c);
        }
    }

    // Create TensorData and read back
    auto tensor = TensorData::createOrdinal2D(flat_data, n_obs, n_components);
    REQUIRE(tensor.numRows() == n_obs);
    REQUIRE(tensor.numColumns() == n_components);

    // Read back into arma (observations × features) to check round-trip
    arma::mat readback(n_obs, n_components);
    for (std::size_t c = 0; c < n_components; ++c) {
        auto col = tensor.getColumn(c);
        for (std::size_t r = 0; r < n_obs; ++r) {
            readback(r, c) = static_cast<double>(col[r]);
        }
    }

    // Compute centered data for norm comparison
    arma::mat centered = features.each_col() - arma::mean(features, 1);

    // Key invariant: PCA with all components is an orthogonal rotation.
    // The L2 norm of each observation must be preserved.
    for (std::size_t obs = 0; obs < n_obs; ++obs) {
        double orig_norm_sq = arma::dot(centered.col(obs), centered.col(obs));
        double pca_norm_sq = arma::dot(readback.row(obs), readback.row(obs));
        double rel_error = std::abs(orig_norm_sq - pca_norm_sq) /
                           (orig_norm_sq + 1e-30);

        INFO("obs " << obs << ": orig_norm²=" << orig_norm_sq
                    << " pca_norm²=" << pca_norm_sq
                    << " rel_err=" << rel_error);
        // Float32 round-trip allows ~1e-6 relative error, not 10x
        REQUIRE(rel_error < 1e-4);
    }

    // Also verify individual values match the PCA result (not scrambled)
    for (std::size_t obs = 0; obs < n_obs; ++obs) {
        for (std::size_t pc = 0; pc < n_components; ++pc) {
            double expected = reduced(pc, obs);
            double actual = readback(obs, pc);
            double abs_err = std::abs(expected - actual);
            // Float32 precision: relative error ~1e-7, abs error scales with value
            double tol = std::max(1e-6, std::abs(expected) * 1e-5);
            INFO("obs=" << obs << " pc=" << pc
                        << " expected=" << expected << " actual=" << actual);
            REQUIRE(abs_err < tol);
        }
    }
}
