#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/DataTypes/AlphaCurve.hpp"

#include <string>

using namespace CorePlotting;
using Catch::Matchers::WithinAbs;

// ============================================================================
// AlphaCurve enum string conversion Tests
// ============================================================================

TEST_CASE("AlphaCurve - string round-trip", "[AlphaCurve]") {
    REQUIRE(alphaCurveToString(AlphaCurve::Linear) == "linear");
    REQUIRE(alphaCurveToString(AlphaCurve::Exponential) == "exponential");
    REQUIRE(alphaCurveToString(AlphaCurve::Gaussian) == "gaussian");

    REQUIRE(alphaCurveFromString("linear") == AlphaCurve::Linear);
    REQUIRE(alphaCurveFromString("exponential") == AlphaCurve::Exponential);
    REQUIRE(alphaCurveFromString("gaussian") == AlphaCurve::Gaussian);
}

TEST_CASE("AlphaCurve - unknown string defaults to Linear", "[AlphaCurve]") {
    REQUIRE(alphaCurveFromString("unknown") == AlphaCurve::Linear);
    REQUIRE(alphaCurveFromString("") == AlphaCurve::Linear);
}

// ============================================================================
// computeTemporalAlpha (int overload) Tests
// ============================================================================

TEST_CASE("computeTemporalAlpha - center returns max_alpha", "[AlphaCurve]") {
    SECTION("Linear") {
        REQUIRE_THAT(computeTemporalAlpha(0, 10, AlphaCurve::Linear, 0.1f, 1.0f),
                     WithinAbs(1.0, 1e-5));
    }
    SECTION("Exponential") {
        REQUIRE_THAT(computeTemporalAlpha(0, 10, AlphaCurve::Exponential, 0.1f, 1.0f),
                     WithinAbs(1.0, 1e-5));
    }
    SECTION("Gaussian") {
        REQUIRE_THAT(computeTemporalAlpha(0, 10, AlphaCurve::Gaussian, 0.1f, 1.0f),
                     WithinAbs(1.0, 1e-5));
    }
}

TEST_CASE("computeTemporalAlpha - at edge returns min_alpha", "[AlphaCurve]") {
    SECTION("Linear at half_width") {
        REQUIRE_THAT(computeTemporalAlpha(10, 10, AlphaCurve::Linear, 0.1f, 1.0f),
                     WithinAbs(0.1, 1e-5));
    }
    SECTION("Beyond half_width clips to min_alpha") {
        REQUIRE_THAT(computeTemporalAlpha(15, 10, AlphaCurve::Linear, 0.1f, 1.0f),
                     WithinAbs(0.1, 1e-5));
    }
}

TEST_CASE("computeTemporalAlpha - linear midpoint", "[AlphaCurve]") {
    // At half_width/2, linear should return exactly midpoint alpha
    float alpha = computeTemporalAlpha(5, 10, AlphaCurve::Linear, 0.0f, 1.0f);
    REQUIRE_THAT(alpha, WithinAbs(0.5, 1e-5));
}

TEST_CASE("computeTemporalAlpha - negative distance", "[AlphaCurve]") {
    // Should use absolute value
    float alpha_pos = computeTemporalAlpha(3, 10, AlphaCurve::Linear, 0.0f, 1.0f);
    float alpha_neg = computeTemporalAlpha(-3, 10, AlphaCurve::Linear, 0.0f, 1.0f);
    REQUIRE_THAT(alpha_pos, WithinAbs(alpha_neg, 1e-5));
}

TEST_CASE("computeTemporalAlpha - half_width zero returns max_alpha", "[AlphaCurve]") {
    REQUIRE_THAT(computeTemporalAlpha(5, 0, AlphaCurve::Linear, 0.1f, 1.0f),
                 WithinAbs(1.0, 1e-5));
}

TEST_CASE("computeTemporalAlpha - monotonically decreasing", "[AlphaCurve]") {
    // Alpha should decrease as distance increases for all curve types
    auto const curves = {AlphaCurve::Linear, AlphaCurve::Exponential, AlphaCurve::Gaussian};

    for (auto curve : curves) {
        float prev_alpha = computeTemporalAlpha(0, 10, curve, 0.0f, 1.0f);
        for (int d = 1; d <= 10; ++d) {
            float alpha = computeTemporalAlpha(d, 10, curve, 0.0f, 1.0f);
            REQUIRE(alpha <= prev_alpha);
            prev_alpha = alpha;
        }
    }
}

TEST_CASE("computeTemporalAlpha - custom min/max", "[AlphaCurve]") {
    float alpha_center = computeTemporalAlpha(0, 5, AlphaCurve::Linear, 0.3f, 0.8f);
    REQUIRE_THAT(alpha_center, WithinAbs(0.8, 1e-5));

    float alpha_edge = computeTemporalAlpha(5, 5, AlphaCurve::Linear, 0.3f, 0.8f);
    REQUIRE_THAT(alpha_edge, WithinAbs(0.3, 1e-5));
}

// ============================================================================
// computeTemporalAlpha (float overload) Tests
// ============================================================================

TEST_CASE("computeTemporalAlpha float - center returns max_alpha", "[AlphaCurve]") {
    REQUIRE_THAT(computeTemporalAlpha(0.0f, 10.0f, AlphaCurve::Linear, 0.1f, 1.0f),
                 WithinAbs(1.0, 1e-5));
}

TEST_CASE("computeTemporalAlpha float - linear midpoint", "[AlphaCurve]") {
    float alpha = computeTemporalAlpha(5.0f, 10.0f, AlphaCurve::Linear, 0.0f, 1.0f);
    REQUIRE_THAT(alpha, WithinAbs(0.5, 1e-5));
}

TEST_CASE("computeTemporalAlpha float - negative distance", "[AlphaCurve]") {
    float alpha_pos = computeTemporalAlpha(3.5f, 10.0f, AlphaCurve::Gaussian, 0.0f, 1.0f);
    float alpha_neg = computeTemporalAlpha(-3.5f, 10.0f, AlphaCurve::Gaussian, 0.0f, 1.0f);
    REQUIRE_THAT(alpha_pos, WithinAbs(alpha_neg, 1e-5));
}

// ============================================================================
// Exponential vs Gaussian curve shape Tests
// ============================================================================

TEST_CASE("computeTemporalAlpha - exponential differs from linear", "[AlphaCurve]") {
    // Exponential and linear should give different values at intermediate distances
    float linear_mid = computeTemporalAlpha(5, 10, AlphaCurve::Linear, 0.0f, 1.0f);
    float exp_mid = computeTemporalAlpha(5, 10, AlphaCurve::Exponential, 0.0f, 1.0f);
    // They should differ
    REQUIRE(std::abs(linear_mid - exp_mid) > 0.01f);
    // Both should be in (0, 1)
    REQUIRE(exp_mid > 0.0f);
    REQUIRE(exp_mid < 1.0f);
}

TEST_CASE("computeTemporalAlpha - gaussian holds near center", "[AlphaCurve]") {
    // Near center (small t), gaussian curve is also flatter than linear
    float linear_alpha = computeTemporalAlpha(2, 10, AlphaCurve::Linear, 0.0f, 1.0f);
    float gauss_alpha = computeTemporalAlpha(2, 10, AlphaCurve::Gaussian, 0.0f, 1.0f);
    // Gaussian near zero is flatter → higher alpha at small distances
    REQUIRE(gauss_alpha >= linear_alpha);
}
