/**
 * @file GaussianNoiseGenerator.test.cpp
 * @brief Unit tests for the GaussianNoise generator.
 */
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataSynthesizer/GeneratorRegistry.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numeric>

using namespace WhiskerToolbox::DataSynthesizer;

static std::shared_ptr<AnalogTimeSeries> runGaussianNoise(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("GaussianNoise", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<AnalogTimeSeries>>(*result);
}

TEST_CASE("GaussianNoise produces correct output size", "[GaussianNoise]") {
    auto ts = runGaussianNoise(R"({"num_samples": 500, "stddev": 1.0})");
    REQUIRE(ts->getNumSamples() == 500);
}

TEST_CASE("GaussianNoise is deterministic with the same seed", "[GaussianNoise]") {
    auto ts1 = runGaussianNoise(R"({"num_samples": 200, "stddev": 1.5, "seed": 123})");
    auto ts2 = runGaussianNoise(R"({"num_samples": 200, "stddev": 1.5, "seed": 123})");

    auto v1 = ts1->getAnalogTimeSeries();
    auto v2 = ts2->getAnalogTimeSeries();
    REQUIRE(v1.size() == v2.size());
    for (size_t i = 0; i < v1.size(); ++i) {
        REQUIRE(v1[i] == v2[i]);
    }
}

TEST_CASE("GaussianNoise different seeds produce different output", "[GaussianNoise]") {
    auto ts1 = runGaussianNoise(R"({"num_samples": 100, "stddev": 1.0, "seed": 1})");
    auto ts2 = runGaussianNoise(R"({"num_samples": 100, "stddev": 1.0, "seed": 2})");

    auto v1 = ts1->getAnalogTimeSeries();
    auto v2 = ts2->getAnalogTimeSeries();
    // With high probability (essentially certain for 100 samples), they differ
    bool differs = false;
    for (size_t i = 0; i < v1.size(); ++i) {
        if (v1[i] != v2[i]) {
            differs = true;
            break;
        }
    }
    REQUIRE(differs);
}

TEST_CASE("GaussianNoise default seed produces deterministic output", "[GaussianNoise]") {
    // Omitting seed should use default (42) consistently
    auto ts1 = runGaussianNoise(R"({"num_samples": 50, "stddev": 1.0})");
    auto ts2 = runGaussianNoise(R"({"num_samples": 50, "stddev": 1.0})");

    auto v1 = ts1->getAnalogTimeSeries();
    auto v2 = ts2->getAnalogTimeSeries();
    for (size_t i = 0; i < v1.size(); ++i) {
        REQUIRE(v1[i] == v2[i]);
    }
}

TEST_CASE("GaussianNoise mean is approximately correct for large N", "[GaussianNoise]") {
    // With 10000 samples, the sample mean should be close to the specified mean
    float const target_mean = 5.0f;
    auto ts = runGaussianNoise(
            R"({"num_samples": 10000, "stddev": 1.0, "mean": 5.0, "seed": 42})");
    auto values = ts->getAnalogTimeSeries();

    float sum = std::accumulate(values.begin(), values.end(), 0.0f);
    float sample_mean = sum / static_cast<float>(values.size());

    // Sample mean should be within 0.1 of target with very high probability
    REQUIRE(sample_mean == Catch::Approx(target_mean).margin(0.1f));
}

TEST_CASE("GaussianNoise stddev scaling works", "[GaussianNoise]") {
    // With higher stddev, the range of values should be larger on average
    auto ts_low = runGaussianNoise(R"({"num_samples": 5000, "stddev": 0.1, "seed": 99})");
    auto ts_high = runGaussianNoise(R"({"num_samples": 5000, "stddev": 10.0, "seed": 99})");

    auto v_low = ts_low->getAnalogTimeSeries();
    auto v_high = ts_high->getAnalogTimeSeries();

    float max_low = *std::max_element(v_low.begin(), v_low.end());
    float max_high = *std::max_element(v_high.begin(), v_high.end());

    REQUIRE(max_high > max_low);
}

TEST_CASE("GaussianNoise rejects num_samples <= 0", "[GaussianNoise]") {
    auto result = GeneratorRegistry::instance().generate(
            "GaussianNoise", R"({"num_samples": 0, "stddev": 1.0})");
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("GaussianNoise rejects negative stddev", "[GaussianNoise]") {
    auto result = GeneratorRegistry::instance().generate(
            "GaussianNoise", R"({"num_samples": 100, "stddev": -1.0})");
    REQUIRE_FALSE(result.has_value());
}
