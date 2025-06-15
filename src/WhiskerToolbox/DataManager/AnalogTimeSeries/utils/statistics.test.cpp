#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/utils/statistics.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <map>
#include <ranges>
#include <random>
#include <vector>


TEST_CASE("AnalogTimeSeries - Statistics", "[analog][timeseries][statistics]") {

    SECTION("Statistical calculations") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        AnalogTimeSeries series(data, 5);

        REQUIRE_THAT(calculate_mean(series), Catch::Matchers::WithinRel(3.0f, 1e-3f));
        REQUIRE_THAT(calculate_std_dev(series), Catch::Matchers::WithinRel(1.41421f, 1e-3f));// 1.41421 for N, 1.5811 for N-1 denom
        REQUIRE(calculate_min(series) == 1.0f);
        REQUIRE(calculate_max(series) == 5.0f);

        // Test with range. 1 to 4 will reduce the data to {2.0f, 3.0f, 4.0f}
        REQUIRE_THAT(calculate_mean(series, 1, 4), Catch::Matchers::WithinRel(3.0f, 1e-3f));
        REQUIRE_THAT(calculate_std_dev(series, 1, 4), Catch::Matchers::WithinRel(0.8165f, 1e-3f));// 0.8165 for N, 1.0 for N-1 denom
        REQUIRE(calculate_min(series, 1, 4) == 2.0f);
        REQUIRE(calculate_max(series, 1, 4) == 4.0f);
    }

}

TEST_CASE("AnalogTimeSeries - Approximate Statistics", "[analog][timeseries][approximate]") {
    SECTION("Approximate standard deviation with percentage sampling") {
        // Create a large dataset with known statistical properties
        std::vector<float> data;
        data.reserve(10000);
        //Create gaussian-like data
        std::default_random_engine generator;
        std::normal_distribution<float> distribution(50.0f, 10.0f);// Mean 50, StdDev 10
        for (int i = 0; i < 10000; ++i) {
            data.push_back(distribution(generator));
        }
        AnalogTimeSeries series(data, 10000);

        float exact_std = calculate_std_dev(series);
        float approx_std = calculate_std_dev_approximate(series, 1.0f, 50);// Sample 1% with min threshold 50

        // The approximation should be reasonably close (within 10% for this regular pattern)
        float relative_error = std::abs(exact_std - approx_std) / exact_std;
        REQUIRE(relative_error < 0.1f);
    }

    SECTION("Single data point") {
        std::vector<float> data{42.0f};
        AnalogTimeSeries series(data, 1);

        REQUIRE(calculate_mean(series) == 42.0f);
        REQUIRE(calculate_min(series) == 42.0f);
        REQUIRE(calculate_max(series) == 42.0f);
        REQUIRE(calculate_std_dev(series) == 0.0f);
    }

    SECTION("Approximate standard deviation falls back to exact for small datasets") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        AnalogTimeSeries series(data, 5);

        float exact_std = calculate_std_dev(series);
        float approx_std = calculate_std_dev_approximate(series, 10.0f, 1000);// High percentage but min threshold 1000

        // Should fall back to exact calculation since 5 * 0.1 < 1000
        REQUIRE(exact_std == approx_std);
    }

    SECTION("Adaptive standard deviation convergence") {
        // Create a dataset with varying values but consistent distribution
        std::vector<float> data;
        data.reserve(50000);
        for (int i = 0; i < 50000; ++i) {
            data.push_back(static_cast<float>(std::sin(i * 0.1) * 10.0 + 50.0));
        }
        AnalogTimeSeries series(data, 50000);

        float exact_std = calculate_std_dev(series);
        float adaptive_std = calculate_std_dev_adaptive(series, 100, 5000, 0.02f);

        // The adaptive method should converge to a reasonable approximation
        float relative_error = std::abs(exact_std - adaptive_std) / exact_std;
        REQUIRE(relative_error < 0.05f);
    }

    SECTION("Adaptive standard deviation falls back to exact for small datasets") {
        std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        AnalogTimeSeries series(data, 5);

        float exact_std = calculate_std_dev(series);
        float adaptive_std = calculate_std_dev_adaptive(series, 100, 1000, 0.01f);

        // Should fall back to exact calculation since data size (5) <= max_sample_size (1000)
        REQUIRE(exact_std == adaptive_std);
    }

    SECTION("Empty series handling for approximate methods") {
        AnalogTimeSeries empty_series;

        REQUIRE(calculate_std_dev_approximate(empty_series) == 0.0f);
        REQUIRE(calculate_std_dev_adaptive(empty_series) == 0.0f);
    }

    SECTION("Single value series for approximate methods") {
        std::vector<float> data{42.0f};
        AnalogTimeSeries series(data, 1);

        REQUIRE(calculate_std_dev_approximate(series) == 0.0f);
        REQUIRE(calculate_std_dev_adaptive(series) == 0.0f);
    }

    SECTION("Performance comparison scenario") {
        // Create a large dataset similar to neuroscience recordings
        std::vector<float> data;
        data.reserve(1000000);// 1M samples
        for (int i = 0; i < 1000000; ++i) {
            // Simulate noisy signal with some trend
            data.push_back(static_cast<float>(std::sin(i * 0.001) * 5.0 + (i * 0.00001) +
                                              (std::rand() % 100) * 0.01));
        }
        AnalogTimeSeries series(data, 1000000);

        float exact_std = calculate_std_dev(series);
        float approx_std = calculate_std_dev_approximate(series, 0.1f, 1000);// Sample 0.1%
        float adaptive_std = calculate_std_dev_adaptive(series, 500, 5000, 0.01f);

        // Both approximations should be reasonably close to exact
        float approx_error = std::abs(exact_std - approx_std) / exact_std;
        float adaptive_error = std::abs(exact_std - adaptive_std) / exact_std;

        REQUIRE(approx_error < 0.05f);
        REQUIRE(adaptive_error < 0.05f);
    }
}
