/**
 * @file SquareWaveGenerator.test.cpp
 * @brief Unit tests for the SquareWave generator.
 */
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataSynthesizer/GeneratorRegistry.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace WhiskerToolbox::DataSynthesizer;

static std::shared_ptr<AnalogTimeSeries> runSquareWave(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("SquareWave", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<AnalogTimeSeries>>(*result);
}

TEST_CASE("SquareWave produces correct output size", "[SquareWave]") {
    auto ts = runSquareWave(R"({"num_samples": 300, "amplitude": 1.0, "frequency": 0.01})");
    REQUIRE(ts->getNumSamples() == 300);
}

TEST_CASE("SquareWave values are exactly ±amplitude (plus dc_offset)", "[SquareWave]") {
    float const amplitude = 2.0f;
    auto ts = runSquareWave(R"({"num_samples": 500, "amplitude": 2.0, "frequency": 0.01})");
    auto values = ts->getAnalogTimeSeries();
    for (auto v: values) {
        bool is_pos = (std::abs(v - amplitude) < 1e-5f);
        bool is_neg = (std::abs(v + amplitude) < 1e-5f);
        REQUIRE((is_pos || is_neg));
    }
}

TEST_CASE("SquareWave default duty_cycle 0.5 splits cycle evenly", "[SquareWave]") {
    // 100 samples, frequency=0.01 → 1 cycle over 100 samples
    // First 50 samples should be +amplitude, next 50 -amplitude
    float const amplitude = 1.0f;
    auto ts = runSquareWave(R"({"num_samples": 100, "amplitude": 1.0, "frequency": 0.01})");
    auto values = ts->getAnalogTimeSeries();

    // First half: +amplitude
    for (size_t i = 0; i < 50; ++i) {
        REQUIRE(values[i] == Catch::Approx(amplitude).margin(1e-5f));
    }
    // Second half: -amplitude
    for (size_t i = 50; i < 100; ++i) {
        REQUIRE(values[i] == Catch::Approx(-amplitude).margin(1e-5f));
    }
}

TEST_CASE("SquareWave duty_cycle 0.25 gives 25% positive", "[SquareWave]") {
    // 100 samples, frequency=0.01 → 1 cycle; first 25 positive, rest negative
    float const amplitude = 1.0f;
    auto ts = runSquareWave(
            R"({"num_samples": 100, "amplitude": 1.0, "frequency": 0.01, "duty_cycle": 0.25})");
    auto values = ts->getAnalogTimeSeries();

    for (size_t i = 0; i < 25; ++i) {
        REQUIRE(values[i] == Catch::Approx(amplitude).margin(1e-5f));
    }
    for (size_t i = 25; i < 100; ++i) {
        REQUIRE(values[i] == Catch::Approx(-amplitude).margin(1e-5f));
    }
}

TEST_CASE("SquareWave dc_offset shifts all values", "[SquareWave]") {
    float const dc = 3.0f;
    auto ts_no_dc = runSquareWave(R"({"num_samples": 100, "amplitude": 1.0, "frequency": 0.01})");
    auto ts_with_dc = runSquareWave(
            R"({"num_samples": 100, "amplitude": 1.0, "frequency": 0.01, "dc_offset": 3.0})");

    auto v_no = ts_no_dc->getAnalogTimeSeries();
    auto v_dc = ts_with_dc->getAnalogTimeSeries();
    for (size_t i = 0; i < v_no.size(); ++i) {
        REQUIRE(v_dc[i] == Catch::Approx(v_no[i] + dc).margin(1e-5f));
    }
}

TEST_CASE("SquareWave rejects duty_cycle out of range", "[SquareWave]") {
    auto r1 = GeneratorRegistry::instance().generate(
            "SquareWave", R"({"num_samples": 100, "amplitude": 1.0, "frequency": 0.01, "duty_cycle": 1.5})");
    REQUIRE_FALSE(r1.has_value());

    auto r2 = GeneratorRegistry::instance().generate(
            "SquareWave", R"({"num_samples": 100, "amplitude": 1.0, "frequency": 0.01, "duty_cycle": -0.1})");
    REQUIRE_FALSE(r2.has_value());
}

TEST_CASE("SquareWave rejects num_samples <= 0", "[SquareWave]") {
    auto result = GeneratorRegistry::instance().generate(
            "SquareWave", R"({"num_samples": -1, "amplitude": 1.0, "frequency": 0.01})");
    REQUIRE_FALSE(result.has_value());
}
