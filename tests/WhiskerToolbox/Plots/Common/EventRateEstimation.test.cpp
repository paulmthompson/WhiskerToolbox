/**
 * @file EventRateEstimation.test.cpp
 * @brief Unit tests for the rate estimation and normalization API
 *
 * Tests the new RateEstimate-based API:
 * - `applyScaling()` with all ScalingMode values
 * - Normalization primitives (toFiringRateHz, toCountPerTrial, etc.)
 * - Helper functions (scalingLabel, allScalingModes)
 * - RateEstimate metadata preservation
 *
 * These tests operate on synthetic RateEstimate data (no DataManager required).
 */

#include "Plots/Common/EventRateEstimation/EventRateEstimation.hpp"
#include "Plots/Common/EventRateEstimation/RateEstimate.hpp"
#include "Plots/Common/EventRateEstimation/RateNormalization.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numeric>

using namespace WhiskerToolbox::Plots;
using Catch::Approx;

// =============================================================================
// Helpers
// =============================================================================

/**
 * @brief Create a simple RateEstimate for testing
 *
 * Generates bin centers from bin_start to cover all values with the given spacing.
 *
 * @param values        Raw event counts per bin
 * @param bin_start     Left edge of first bin
 * @param sample_spacing Bin width / spacing
 * @param num_trials    Number of trials
 */
static RateEstimate makeEstimate(std::vector<double> values,
                                  double bin_start = -50.0,
                                  double sample_spacing = 10.0,
                                  size_t num_trials = 5)
{
    RateEstimate est;
    est.num_trials = num_trials;
    est.metadata.sample_spacing = sample_spacing;
    est.values = std::move(values);
    // Build bin centers
    est.times.resize(est.values.size());
    for (size_t i = 0; i < est.values.size(); ++i) {
        est.times[i] = bin_start + static_cast<double>(i) * sample_spacing
                        + sample_spacing / 2.0;
    }
    return est;
}

// =============================================================================
// RawCount
// =============================================================================

TEST_CASE("applyScaling RawCount returns values unchanged",
          "[EventRateEstimation][Scaling]")
{
    auto est = makeEstimate({0, 5, 10, 15, 20});
    applyScaling(est, ScalingMode::RawCount, 1000.0);

    REQUIRE(est.values.size() == 5);
    CHECK(est.values[0] == 0.0);
    CHECK(est.values[1] == 5.0);
    CHECK(est.values[2] == 10.0);
    CHECK(est.values[3] == 15.0);
    CHECK(est.values[4] == 20.0);
}

// =============================================================================
// CountPerTrial
// =============================================================================

TEST_CASE("applyScaling CountPerTrial divides by num_trials",
          "[EventRateEstimation][Scaling]")
{
    auto est = makeEstimate({0, 10, 20, 30, 40}, -50.0, 10.0, 10);
    applyScaling(est, ScalingMode::CountPerTrial, 1000.0);

    REQUIRE(est.values.size() == 5);
    CHECK(est.values[0] == Approx(0.0));
    CHECK(est.values[1] == Approx(1.0));
    CHECK(est.values[2] == Approx(2.0));
    CHECK(est.values[3] == Approx(3.0));
    CHECK(est.values[4] == Approx(4.0));
}

TEST_CASE("applyScaling CountPerTrial with zero trials is no-op",
          "[EventRateEstimation][Scaling]")
{
    auto est = makeEstimate({5, 10, 15}, -50.0, 10.0, 0);
    applyScaling(est, ScalingMode::CountPerTrial, 1000.0);

    REQUIRE(est.values.size() == 3);
    // With 0 trials, division doesn't happen — values are unchanged
    CHECK(est.values[0] == 5.0);
}

// =============================================================================
// FiringRateHz
// =============================================================================

TEST_CASE("applyScaling FiringRateHz converts to Hz",
          "[EventRateEstimation][Scaling]")
{
    // 10 counts in a 10ms bin across 5 trials -> 10 / (5 * 0.01s) = 200 Hz
    double const time_units_per_second = 1000.0; // ms
    auto est = makeEstimate({10, 20, 0, 5, 50}, -50.0, 10.0, 5);
    applyScaling(est, ScalingMode::FiringRateHz, time_units_per_second);

    REQUIRE(est.values.size() == 5);
    // rate = count / (num_trials * sample_spacing_s)
    // sample_spacing_s = 10.0 / 1000.0 = 0.01
    // divisor = 5 * 0.01 = 0.05
    CHECK(est.values[0] == Approx(10.0 / 0.05));   // 200
    CHECK(est.values[1] == Approx(20.0 / 0.05));   // 400
    CHECK(est.values[2] == Approx(0.0));
    CHECK(est.values[3] == Approx(5.0 / 0.05));    // 100
    CHECK(est.values[4] == Approx(50.0 / 0.05));   // 1000
}

TEST_CASE("applyScaling FiringRateHz with different time units",
          "[EventRateEstimation][Scaling]")
{
    // Time in seconds (time_units_per_second = 1.0)
    double const time_units_per_second = 1.0;
    auto est = makeEstimate({20}, -0.5, 1.0, 4); // 1s bin, 4 trials
    applyScaling(est, ScalingMode::FiringRateHz, time_units_per_second);

    REQUIRE(est.values.size() == 1);
    // rate = 20 / (4 * 1.0) = 5 Hz
    CHECK(est.values[0] == Approx(5.0));
}

// =============================================================================
// ZScore
// =============================================================================

TEST_CASE("applyScaling ZScore computes per-unit z-scores",
          "[EventRateEstimation][Scaling]")
{
    // 5 trials, counts: [10, 20, 30, 40, 50]
    // After count/trial: [2, 4, 6, 8, 10]
    // mean = 6, std = sqrt(((2-6)^2+(4-6)^2+(6-6)^2+(8-6)^2+(10-6)^2)/5) = sqrt(8) ≈ 2.828
    auto est = makeEstimate({10, 20, 30, 40, 50}, -50.0, 10.0, 5);
    applyScaling(est, ScalingMode::ZScore, 1000.0);

    REQUIRE(est.values.size() == 5);
    double const expected_std = std::sqrt(8.0);
    CHECK(est.values[0] == Approx((2.0 - 6.0) / expected_std));
    CHECK(est.values[1] == Approx((4.0 - 6.0) / expected_std));
    CHECK(est.values[2] == Approx(0.0).margin(1e-12));  // mean = 6, z = 0
    CHECK(est.values[3] == Approx((8.0 - 6.0) / expected_std));
    CHECK(est.values[4] == Approx((10.0 - 6.0) / expected_std));
}

TEST_CASE("applyScaling ZScore constant signal yields all zeros",
          "[EventRateEstimation][Scaling]")
{
    auto est = makeEstimate({10, 10, 10, 10}, -50.0, 10.0, 5);
    applyScaling(est, ScalingMode::ZScore, 1000.0);

    REQUIRE(est.values.size() == 4);
    for (auto const & v : est.values) {
        CHECK(v == 0.0);
    }
}

// =============================================================================
// Normalized01
// =============================================================================

TEST_CASE("applyScaling Normalized01 maps to [0, 1]",
          "[EventRateEstimation][Scaling]")
{
    // 4 trials, counts: [4, 8, 12, 16]
    // After count/trial: [1, 2, 3, 4]
    // min = 1, max = 4 → (v - 1) / 3
    auto est = makeEstimate({4, 8, 12, 16}, -50.0, 10.0, 4);
    applyScaling(est, ScalingMode::Normalized01, 1000.0);

    REQUIRE(est.values.size() == 4);
    CHECK(est.values[0] == Approx(0.0));
    CHECK(est.values[1] == Approx(1.0 / 3.0));
    CHECK(est.values[2] == Approx(2.0 / 3.0));
    CHECK(est.values[3] == Approx(1.0));
}

TEST_CASE("applyScaling Normalized01 constant signal yields zeros",
          "[EventRateEstimation][Scaling]")
{
    auto est = makeEstimate({5, 5, 5}, -30.0, 10.0, 1);
    applyScaling(est, ScalingMode::Normalized01, 1000.0);

    REQUIRE(est.values.size() == 3);
    for (auto const & v : est.values) {
        CHECK(v == 0.0);
    }
}

// =============================================================================
// Multiple estimates (independent scaling)
// =============================================================================

TEST_CASE("applyScaling handles multiple estimates independently",
          "[EventRateEstimation][Scaling]")
{
    auto est_a = makeEstimate({0, 10, 20}, -30.0, 10.0, 2);  // unit A
    auto est_b = makeEstimate({6, 6, 6}, -30.0, 10.0, 3);    // unit B (constant)

    SECTION("Normalized01 applies per-unit") {
        applyScaling(est_a, ScalingMode::Normalized01, 1000.0);
        applyScaling(est_b, ScalingMode::Normalized01, 1000.0);

        // Unit A: count/trial = [0, 5, 10], min=0, max=10
        CHECK(est_a.values[0] == Approx(0.0));
        CHECK(est_a.values[1] == Approx(0.5));
        CHECK(est_a.values[2] == Approx(1.0));

        // Unit B: constant → all zeros
        CHECK(est_b.values[0] == 0.0);
        CHECK(est_b.values[1] == 0.0);
        CHECK(est_b.values[2] == 0.0);
    }

    SECTION("FiringRateHz applies same transform to each unit") {
        double const tps = 1000.0;
        applyScaling(est_a, ScalingMode::FiringRateHz, tps);
        applyScaling(est_b, ScalingMode::FiringRateHz, tps);

        // Unit A: bin=10ms, trials=2 → divisor = 2 * 0.01 = 0.02
        CHECK(est_a.values[0] == Approx(0.0));
        CHECK(est_a.values[1] == Approx(10.0 / 0.02));
        CHECK(est_a.values[2] == Approx(20.0 / 0.02));

        // Unit B: bin=10ms, trials=3 → divisor = 3 * 0.01 = 0.03
        CHECK(est_b.values[0] == Approx(6.0 / 0.03));
        CHECK(est_b.values[1] == Approx(6.0 / 0.03));
        CHECK(est_b.values[2] == Approx(6.0 / 0.03));
    }
}

// =============================================================================
// Empty input
// =============================================================================

TEST_CASE("applyScaling on empty estimate is no-op",
          "[EventRateEstimation][Scaling]")
{
    RateEstimate est;
    applyScaling(est, ScalingMode::FiringRateHz, 1000.0);
    CHECK(est.values.empty());
    CHECK(est.times.empty());
}

// =============================================================================
// Helper function tests
// =============================================================================

TEST_CASE("scalingLabel returns human-readable labels",
          "[EventRateEstimation][Scaling]")
{
    CHECK(std::string(scalingLabel(ScalingMode::FiringRateHz)) == "Firing Rate (Hz)");
    CHECK(std::string(scalingLabel(ScalingMode::ZScore)) == "Z-Score");
    CHECK(std::string(scalingLabel(ScalingMode::Normalized01)) == "Normalized [0, 1]");
    CHECK(std::string(scalingLabel(ScalingMode::RawCount)) == "Raw Count");
    CHECK(std::string(scalingLabel(ScalingMode::CountPerTrial)) == "Count / Trial");
}

TEST_CASE("allScalingModes returns all five modes",
          "[EventRateEstimation][Scaling]")
{
    auto modes = allScalingModes();
    CHECK(modes.size() == 5);
}

// =============================================================================
// Metadata preservation
// =============================================================================

TEST_CASE("applyScaling preserves times and metadata",
          "[EventRateEstimation][Scaling]")
{
    auto est = makeEstimate({1, 2, 3}, -150.0, 25.0, 1);

    // Save original times and metadata
    auto const original_times = est.times;
    double const original_spacing = est.metadata.sample_spacing;

    for (auto mode : allScalingModes()) {
        auto test_est = makeEstimate({1, 2, 3}, -150.0, 25.0, 1);
        applyScaling(test_est, mode, 1000.0);
        REQUIRE(test_est.times.size() == 3);
        CHECK(test_est.times == original_times);
        CHECK(test_est.metadata.sample_spacing == original_spacing);
    }
}

// =============================================================================
// Normalization primitives
// =============================================================================

TEST_CASE("toFiringRateHz primitive works correctly",
          "[EventRateEstimation][Normalization]")
{
    std::vector<double> values = {10.0, 20.0};
    toFiringRateHz(values, 5, 10.0, 1000.0);
    // divisor = 5 * (10/1000) = 0.05
    CHECK(values[0] == Approx(200.0));
    CHECK(values[1] == Approx(400.0));
}

TEST_CASE("toCountPerTrial primitive works correctly",
          "[EventRateEstimation][Normalization]")
{
    std::vector<double> values = {10.0, 20.0, 30.0};
    toCountPerTrial(values, 5);
    CHECK(values[0] == Approx(2.0));
    CHECK(values[1] == Approx(4.0));
    CHECK(values[2] == Approx(6.0));
}

TEST_CASE("zScoreNormalize primitive works correctly",
          "[EventRateEstimation][Normalization]")
{
    std::vector<double> values = {2.0, 4.0, 6.0, 8.0, 10.0};
    zScoreNormalize(values);
    // mean = 6, std = sqrt(8) ≈ 2.828
    double const expected_std = std::sqrt(8.0);
    CHECK(values[0] == Approx((2.0 - 6.0) / expected_std));
    CHECK(values[2] == Approx(0.0).margin(1e-12));
    CHECK(values[4] == Approx((10.0 - 6.0) / expected_std));
}

TEST_CASE("minMaxNormalize primitive works correctly",
          "[EventRateEstimation][Normalization]")
{
    std::vector<double> values = {1.0, 2.0, 3.0, 4.0};
    minMaxNormalize(values);
    CHECK(values[0] == Approx(0.0));
    CHECK(values[1] == Approx(1.0 / 3.0));
    CHECK(values[2] == Approx(2.0 / 3.0));
    CHECK(values[3] == Approx(1.0));
}

// =============================================================================
// RateEstimate times[] stores bin centers
// =============================================================================

TEST_CASE("RateEstimate times are bin centers",
          "[EventRateEstimation][Output]")
{
    auto est = makeEstimate({1, 2, 3, 4, 5}, -50.0, 10.0, 1);

    // bin_start = -50, spacing = 10
    // Bin 0: [-50, -40), center = -45
    // Bin 1: [-40, -30), center = -35
    // Bin 2: [-30, -20), center = -25
    // etc.
    REQUIRE(est.times.size() == 5);
    CHECK(est.times[0] == Approx(-45.0));
    CHECK(est.times[1] == Approx(-35.0));
    CHECK(est.times[2] == Approx(-25.0));
    CHECK(est.times[3] == Approx(-15.0));
    CHECK(est.times[4] == Approx(-5.0));
}
