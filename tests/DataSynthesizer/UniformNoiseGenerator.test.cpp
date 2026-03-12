/**
 * @file UniformNoiseGenerator.test.cpp
 * @brief Unit tests for the UniformNoise generator.
 */
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataSynthesizer/GeneratorRegistry.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <numeric>

using namespace WhiskerToolbox::DataSynthesizer;

static std::shared_ptr<AnalogTimeSeries> runUniformNoise(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("UniformNoise", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<AnalogTimeSeries>>(*result);
}

TEST_CASE("UniformNoise produces correct output size", "[UniformNoise]") {
    auto ts = runUniformNoise(R"({"num_samples": 400, "min_value": 0.0, "max_value": 1.0})");
    REQUIRE(ts->getNumSamples() == 400);
}

TEST_CASE("UniformNoise all values are within [min_value, max_value)", "[UniformNoise]") {
    float const lo = -3.0f;
    float const hi = 7.0f;
    auto ts = runUniformNoise(
            R"({"num_samples": 1000, "min_value": -3.0, "max_value": 7.0, "seed": 42})");
    auto values = ts->getAnalogTimeSeries();
    for (auto v: values) {
        REQUIRE(v >= lo);
        REQUIRE(v < hi);
    }
}

TEST_CASE("UniformNoise is deterministic with the same seed", "[UniformNoise]") {
    auto ts1 = runUniformNoise(
            R"({"num_samples": 300, "min_value": 0.0, "max_value": 1.0, "seed": 77})");
    auto ts2 = runUniformNoise(
            R"({"num_samples": 300, "min_value": 0.0, "max_value": 1.0, "seed": 77})");

    auto v1 = ts1->getAnalogTimeSeries();
    auto v2 = ts2->getAnalogTimeSeries();
    REQUIRE(v1.size() == v2.size());
    for (size_t i = 0; i < v1.size(); ++i) {
        REQUIRE(v1[i] == v2[i]);
    }
}

TEST_CASE("UniformNoise different seeds produce different output", "[UniformNoise]") {
    auto ts1 = runUniformNoise(
            R"({"num_samples": 100, "min_value": 0.0, "max_value": 1.0, "seed": 10})");
    auto ts2 = runUniformNoise(
            R"({"num_samples": 100, "min_value": 0.0, "max_value": 1.0, "seed": 20})");

    auto v1 = ts1->getAnalogTimeSeries();
    auto v2 = ts2->getAnalogTimeSeries();
    bool differs = false;
    for (size_t i = 0; i < v1.size(); ++i) {
        if (v1[i] != v2[i]) {
            differs = true;
            break;
        }
    }
    REQUIRE(differs);
}

TEST_CASE("UniformNoise default seed produces deterministic output", "[UniformNoise]") {
    auto ts1 = runUniformNoise(R"({"num_samples": 50, "min_value": 0.0, "max_value": 1.0})");
    auto ts2 = runUniformNoise(R"({"num_samples": 50, "min_value": 0.0, "max_value": 1.0})");

    auto v1 = ts1->getAnalogTimeSeries();
    auto v2 = ts2->getAnalogTimeSeries();
    for (size_t i = 0; i < v1.size(); ++i) {
        REQUIRE(v1[i] == v2[i]);
    }
}

TEST_CASE("UniformNoise mean is approximately (min+max)/2 for large N", "[UniformNoise]") {
    float const lo = 2.0f;
    float const hi = 8.0f;
    float const expected_mean = (lo + hi) / 2.0f;

    auto ts = runUniformNoise(
            R"({"num_samples": 10000, "min_value": 2.0, "max_value": 8.0, "seed": 42})");
    auto values = ts->getAnalogTimeSeries();

    float sum = std::accumulate(values.begin(), values.end(), 0.0f);
    float sample_mean = sum / static_cast<float>(values.size());

    REQUIRE(sample_mean == Catch::Approx(expected_mean).margin(0.1f));
}

TEST_CASE("UniformNoise rejects min_value >= max_value", "[UniformNoise]") {
    auto r1 = GeneratorRegistry::instance().generate(
            "UniformNoise", R"({"num_samples": 100, "min_value": 5.0, "max_value": 5.0})");
    REQUIRE_FALSE(r1.has_value());

    auto r2 = GeneratorRegistry::instance().generate(
            "UniformNoise", R"({"num_samples": 100, "min_value": 6.0, "max_value": 5.0})");
    REQUIRE_FALSE(r2.has_value());
}

TEST_CASE("UniformNoise rejects num_samples <= 0", "[UniformNoise]") {
    auto result = GeneratorRegistry::instance().generate(
            "UniformNoise", R"({"num_samples": 0, "min_value": 0.0, "max_value": 1.0})");
    REQUIRE_FALSE(result.has_value());
}
