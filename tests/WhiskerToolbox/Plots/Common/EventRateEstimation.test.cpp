/**
 * @file EventRateEstimation.test.cpp
 * @brief Unit tests for scaling/normalization in EventRateEstimation
 *
 * Tests the normalizeRateProfiles() function with all HeatmapScaling modes.
 * These tests operate on synthetic RateProfile data (no DataManager required).
 */

#include "Plots/Common/EventRateEstimation/EventRateEstimation.hpp"
#include "Plots/Common/EventRateEstimation/RateScaling.hpp"

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
 * @brief Create a simple RateProfile for testing
 *
 * @param counts    Raw event counts per bin
 * @param bin_start Left edge of first bin
 * @param bin_width Bin width
 * @param num_trials Number of trials
 */
static RateProfile makeProfile(std::vector<double> counts,
                               double bin_start = -50.0,
                               double bin_width = 10.0,
                               size_t num_trials = 5)
{
    return RateProfile{std::move(counts), bin_start, bin_width, num_trials};
}

// =============================================================================
// RawCount
// =============================================================================

TEST_CASE("normalizeRateProfiles RawCount returns counts unchanged",
          "[EventRateEstimation][Scaling]")
{
    auto profiles = std::vector<RateProfile>{
        makeProfile({0, 5, 10, 15, 20}),
    };
    auto rows = normalizeRateProfiles(profiles, HeatmapScaling::RawCount);

    REQUIRE(rows.size() == 1);
    REQUIRE(rows[0].values.size() == 5);
    CHECK(rows[0].values[0] == 0.0);
    CHECK(rows[0].values[1] == 5.0);
    CHECK(rows[0].values[2] == 10.0);
    CHECK(rows[0].values[3] == 15.0);
    CHECK(rows[0].values[4] == 20.0);
    CHECK(rows[0].bin_start == -50.0);
    CHECK(rows[0].bin_width == 10.0);
}

// =============================================================================
// CountPerTrial
// =============================================================================

TEST_CASE("normalizeRateProfiles CountPerTrial divides by num_trials",
          "[EventRateEstimation][Scaling]")
{
    auto profiles = std::vector<RateProfile>{
        makeProfile({0, 10, 20, 30, 40}, -50.0, 10.0, 10),
    };
    auto rows = normalizeRateProfiles(profiles, HeatmapScaling::CountPerTrial);

    REQUIRE(rows.size() == 1);
    CHECK(rows[0].values[0] == Approx(0.0));
    CHECK(rows[0].values[1] == Approx(1.0));
    CHECK(rows[0].values[2] == Approx(2.0));
    CHECK(rows[0].values[3] == Approx(3.0));
    CHECK(rows[0].values[4] == Approx(4.0));
}

TEST_CASE("normalizeRateProfiles CountPerTrial with zero trials returns zeros",
          "[EventRateEstimation][Scaling]")
{
    auto profiles = std::vector<RateProfile>{
        makeProfile({5, 10, 15}, -50.0, 10.0, 0),
    };
    auto rows = normalizeRateProfiles(profiles, HeatmapScaling::CountPerTrial);

    REQUIRE(rows.size() == 1);
    // With 0 trials, division doesn't happen — counts are unchanged
    CHECK(rows[0].values[0] == 5.0);
}

// =============================================================================
// FiringRate
// =============================================================================

TEST_CASE("normalizeRateProfiles FiringRate converts to Hz",
          "[EventRateEstimation][Scaling]")
{
    // 10 counts in a 10ms bin across 5 trials -> 10 / (5 * 0.01s) = 200 Hz
    double const time_units_per_second = 1000.0; // ms
    auto profiles = std::vector<RateProfile>{
        makeProfile({10, 20, 0, 5, 50}, -50.0, 10.0, 5),
    };
    auto rows = normalizeRateProfiles(profiles, HeatmapScaling::FiringRate,
                                       time_units_per_second);

    REQUIRE(rows.size() == 1);
    // rate = count / (num_trials * bin_size_s)
    // bin_size_s = 10.0 / 1000.0 = 0.01
    // divisor = 5 * 0.01 = 0.05
    CHECK(rows[0].values[0] == Approx(10.0 / 0.05));   // 200
    CHECK(rows[0].values[1] == Approx(20.0 / 0.05));   // 400
    CHECK(rows[0].values[2] == Approx(0.0));
    CHECK(rows[0].values[3] == Approx(5.0 / 0.05));    // 100
    CHECK(rows[0].values[4] == Approx(50.0 / 0.05));   // 1000
}

TEST_CASE("normalizeRateProfiles FiringRate with different time units",
          "[EventRateEstimation][Scaling]")
{
    // Time in seconds (time_units_per_second = 1.0)
    double const time_units_per_second = 1.0;
    auto profiles = std::vector<RateProfile>{
        makeProfile({20}, -0.5, 1.0, 4), // 1s bin, 4 trials
    };
    auto rows = normalizeRateProfiles(profiles, HeatmapScaling::FiringRate,
                                       time_units_per_second);

    REQUIRE(rows.size() == 1);
    // rate = 20 / (4 * 1.0) = 5 Hz
    CHECK(rows[0].values[0] == Approx(5.0));
}

// =============================================================================
// ZScore
// =============================================================================

TEST_CASE("normalizeRateProfiles ZScore computes per-unit z-scores",
          "[EventRateEstimation][Scaling]")
{
    // 5 trials, counts: [10, 20, 30, 40, 50]
    // After count/trial: [2, 4, 6, 8, 10]
    // mean = 6, std = sqrt(((2-6)^2+(4-6)^2+(6-6)^2+(8-6)^2+(10-6)^2)/5) = sqrt(8) ≈ 2.828
    auto profiles = std::vector<RateProfile>{
        makeProfile({10, 20, 30, 40, 50}, -50.0, 10.0, 5),
    };
    auto rows = normalizeRateProfiles(profiles, HeatmapScaling::ZScore);

    REQUIRE(rows.size() == 1);
    double const expected_std = std::sqrt(8.0);
    CHECK(rows[0].values[0] == Approx((2.0 - 6.0) / expected_std));
    CHECK(rows[0].values[1] == Approx((4.0 - 6.0) / expected_std));
    CHECK(rows[0].values[2] == Approx(0.0).margin(1e-12));  // mean = 6, z = 0
    CHECK(rows[0].values[3] == Approx((8.0 - 6.0) / expected_std));
    CHECK(rows[0].values[4] == Approx((10.0 - 6.0) / expected_std));
}

TEST_CASE("normalizeRateProfiles ZScore constant signal yields all zeros",
          "[EventRateEstimation][Scaling]")
{
    auto profiles = std::vector<RateProfile>{
        makeProfile({10, 10, 10, 10}, -50.0, 10.0, 5),
    };
    auto rows = normalizeRateProfiles(profiles, HeatmapScaling::ZScore);

    REQUIRE(rows.size() == 1);
    for (auto const & v : rows[0].values) {
        CHECK(v == 0.0);
    }
}

// =============================================================================
// Normalized01
// =============================================================================

TEST_CASE("normalizeRateProfiles Normalized01 maps to [0, 1]",
          "[EventRateEstimation][Scaling]")
{
    // 4 trials, counts: [4, 8, 12, 16]
    // After count/trial: [1, 2, 3, 4]
    // min = 1, max = 4 → (v - 1) / 3
    auto profiles = std::vector<RateProfile>{
        makeProfile({4, 8, 12, 16}, -50.0, 10.0, 4),
    };
    auto rows = normalizeRateProfiles(profiles, HeatmapScaling::Normalized01);

    REQUIRE(rows.size() == 1);
    CHECK(rows[0].values[0] == Approx(0.0));
    CHECK(rows[0].values[1] == Approx(1.0 / 3.0));
    CHECK(rows[0].values[2] == Approx(2.0 / 3.0));
    CHECK(rows[0].values[3] == Approx(1.0));
}

TEST_CASE("normalizeRateProfiles Normalized01 constant signal yields zeros",
          "[EventRateEstimation][Scaling]")
{
    auto profiles = std::vector<RateProfile>{
        makeProfile({5, 5, 5}, -30.0, 10.0, 1),
    };
    auto rows = normalizeRateProfiles(profiles, HeatmapScaling::Normalized01);

    REQUIRE(rows.size() == 1);
    for (auto const & v : rows[0].values) {
        CHECK(v == 0.0);
    }
}

// =============================================================================
// Multiple units
// =============================================================================

TEST_CASE("normalizeRateProfiles handles multiple units independently",
          "[EventRateEstimation][Scaling]")
{
    auto profiles = std::vector<RateProfile>{
        makeProfile({0, 10, 20}, -30.0, 10.0, 2),  // unit A
        makeProfile({6, 6, 6}, -30.0, 10.0, 3),    // unit B (constant)
    };

    SECTION("Normalized01 applies per-unit") {
        auto rows = normalizeRateProfiles(profiles, HeatmapScaling::Normalized01);
        REQUIRE(rows.size() == 2);

        // Unit A: count/trial = [0, 5, 10], min=0, max=10
        CHECK(rows[0].values[0] == Approx(0.0));
        CHECK(rows[0].values[1] == Approx(0.5));
        CHECK(rows[0].values[2] == Approx(1.0));

        // Unit B: constant → all zeros
        CHECK(rows[1].values[0] == 0.0);
        CHECK(rows[1].values[1] == 0.0);
        CHECK(rows[1].values[2] == 0.0);
    }

    SECTION("FiringRate applies same transform to each unit") {
        double const tps = 1000.0;
        auto rows = normalizeRateProfiles(profiles, HeatmapScaling::FiringRate, tps);
        REQUIRE(rows.size() == 2);

        // Unit A: bin=10ms, trials=2 → divisor = 2 * 0.01 = 0.02
        CHECK(rows[0].values[0] == Approx(0.0));
        CHECK(rows[0].values[1] == Approx(10.0 / 0.02));
        CHECK(rows[0].values[2] == Approx(20.0 / 0.02));

        // Unit B: bin=10ms, trials=3 → divisor = 3 * 0.01 = 0.03
        CHECK(rows[1].values[0] == Approx(6.0 / 0.03));
        CHECK(rows[1].values[1] == Approx(6.0 / 0.03));
        CHECK(rows[1].values[2] == Approx(6.0 / 0.03));
    }
}

// =============================================================================
// Empty input
// =============================================================================

TEST_CASE("normalizeRateProfiles empty input returns empty output",
          "[EventRateEstimation][Scaling]")
{
    auto rows = normalizeRateProfiles({}, HeatmapScaling::FiringRate);
    CHECK(rows.empty());
}

// =============================================================================
// Helper function tests
// =============================================================================

TEST_CASE("scalingLabel returns human-readable labels",
          "[EventRateEstimation][Scaling]")
{
    CHECK(std::string(scalingLabel(HeatmapScaling::FiringRate)) == "Firing Rate (Hz)");
    CHECK(std::string(scalingLabel(HeatmapScaling::ZScore)) == "Z-Score");
    CHECK(std::string(scalingLabel(HeatmapScaling::Normalized01)) == "Normalized [0, 1]");
    CHECK(std::string(scalingLabel(HeatmapScaling::RawCount)) == "Raw Count");
    CHECK(std::string(scalingLabel(HeatmapScaling::CountPerTrial)) == "Count / Trial");
}

TEST_CASE("allScalingModes returns all five modes",
          "[EventRateEstimation][Scaling]")
{
    auto modes = allScalingModes();
    CHECK(modes.size() == 5);
}

// =============================================================================
// Bin axis metadata preservation
// =============================================================================

TEST_CASE("normalizeRateProfiles preserves bin_start and bin_width",
          "[EventRateEstimation][Scaling]")
{
    auto profiles = std::vector<RateProfile>{
        makeProfile({1, 2, 3}, -150.0, 25.0, 1),
    };

    for (auto scaling : allScalingModes()) {
        auto rows = normalizeRateProfiles(profiles, scaling, 1000.0);
        REQUIRE(rows.size() == 1);
        CHECK(rows[0].bin_start == -150.0);
        CHECK(rows[0].bin_width == 25.0);
    }
}
