/**
 * @file sinc_interpolation.test.cpp
 * @brief Unit tests for CoreMath::SincInterpolator.
 */

#include "CoreMath/sinc_interpolation.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>
#include <vector>

using namespace CoreMath;
using Catch::Matchers::WithinAbs;

TEST_CASE("SincInterpolator exact integer reconstruction", "[CoreMath][SincInterpolator]") {
    std::vector<float> const data = {1.0f, 4.0f, 2.0f, 8.0f, 5.0f, 7.0f, 3.0f};

    for (size_t i = 0; i < data.size(); ++i) {
        float const val = SincInterpolator::interpolateAt(
                data, static_cast<double>(i), 8, SincWindowType::Lanczos);
        REQUIRE_THAT(val, WithinAbs(data[i], 1e-4f));
    }
}

TEST_CASE("SincInterpolator band-limited sinusoid upsampling", "[CoreMath][SincInterpolator]") {
    size_t const n_samples = 32;
    std::vector<float> input(n_samples);
    double const freq = 2.0 * std::numbers::pi * 2.0 / static_cast<double>(n_samples);

    for (size_t i = 0; i < n_samples; ++i) {
        input[i] = static_cast<float>(std::sin(freq * static_cast<double>(i)));
    }

    int const factor = 4;
    auto const upsampled = SincInterpolator::upsample(input, factor, 8, SincWindowType::Lanczos);

    size_t const expected_len = (n_samples - 1) * factor + 1;
    REQUIRE(upsampled.size() == expected_len);

    // Verify intermediate points match true continuous sinusoid
    double const step = 1.0 / static_cast<double>(factor);
    for (size_t m = 4; m < expected_len - 4; ++m) {
        double const true_val = std::sin(freq * static_cast<double>(m) * step);
        REQUIRE_THAT(upsampled[m], WithinAbs(true_val, 0.05f));
    }
}

TEST_CASE("SincInterpolator fractional shift", "[CoreMath][SincInterpolator]") {
    // Pulse centered at sample index 10
    std::vector<float> input(21, 0.0f);
    input[10] = 100.0f;
    input[9] = 50.0f;
    input[11] = 50.0f;

    // Shift by +1.0 sample (pulse should move to sample 11)
    auto const shifted_1 = SincInterpolator::shift(input, 1.0, 8, SincWindowType::Lanczos);
    REQUIRE(shifted_1.size() == input.size());
    REQUIRE_THAT(shifted_1[11], WithinAbs(100.0f, 0.5f));

    // Shift by +0.5 sample (peak should be symmetric between 10 and 11)
    auto const shifted_half = SincInterpolator::shift(input, 0.5, 8, SincWindowType::Lanczos);
    REQUIRE_THAT(shifted_half[10], WithinAbs(shifted_half[11], 0.1f));
}

TEST_CASE("SincInterpolator sub-sample extremum localization", "[CoreMath][SincInterpolator]") {
    // Parabolic trough centered between sample 10 and 11 at 10.4
    std::vector<float> input(21);
    for (size_t i = 0; i < input.size(); ++i) {
        double const dt = static_cast<double>(i) - 10.4;
        input[i] = static_cast<float>(dt * dt - 100.0);
    }

    auto const [sub_pos, sub_val] = SincInterpolator::findSubSampleExtremum(
            input, 10, true, 2, 32, 8, SincWindowType::Hann);

    REQUIRE_THAT(sub_pos, WithinAbs(10.4, 0.05));
    REQUIRE_THAT(sub_val, WithinAbs(-100.0f, 0.5f));
}

TEST_CASE("SincInterpolator sub-sample energy centroid", "[CoreMath][SincInterpolator]") {
    // Voltage wave centered at 12.3
    std::vector<float> input(25, 0.0f);
    for (size_t i = 0; i < input.size(); ++i) {
        double const dt = static_cast<double>(i) - 12.3;
        input[i] = static_cast<float>(std::exp(-0.5 * dt * dt));
    }

    double const cog = SincInterpolator::findSubSampleEnergyCentroid(
            input, 12, 6, 32, 8, SincWindowType::Hann);

    REQUIRE_THAT(cog, WithinAbs(12.3, 0.05));
}
