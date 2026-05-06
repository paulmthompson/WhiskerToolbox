/**
 * @file SawtoothWaveGenerator.test.cpp
 * @brief Unit tests for the SawtoothWave generator.
 */
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataSynthesizer/GeneratorRegistry.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace WhiskerToolbox::DataSynthesizer;

static std::shared_ptr<AnalogTimeSeries> runSawtoothWave(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("SawtoothWave", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<AnalogTimeSeries>>(*result);
}

TEST_CASE("SawtoothWave produces correct output size", "[SawtoothWave]") {
    auto ts = runSawtoothWave(R"({"num_samples": 200, "amplitude": 1.0, "num_cycles": 2})");
    REQUIRE(ts->getNumSamples() == 200);
}

TEST_CASE("SawtoothWave value at t=0 with default phase is ~0", "[SawtoothWave]") {
    auto ts = runSawtoothWave(R"({"num_samples": 100, "amplitude": 1.0, "num_cycles": 1})");
    auto values = ts->getAnalogTimeSeries();
    REQUIRE(values[0] == Catch::Approx(0.0f).margin(1e-5f));
}

TEST_CASE("SawtoothWave values are bounded within [-amplitude, amplitude]", "[SawtoothWave]") {
    float const amplitude = 2.5f;
    auto ts = runSawtoothWave(R"({"num_samples": 1000, "amplitude": 2.5, "num_cycles": 10})");
    auto values = ts->getAnalogTimeSeries();
    for (auto v: values) {
        REQUIRE(v <= amplitude + 1e-5f);
        REQUIRE(v >= -amplitude - 1e-5f);
    }
}

TEST_CASE("SawtoothWave is linearly rising within a cycle", "[SawtoothWave]") {
    // 1000 samples, num_cycles = 1 → one complete cycle over 1000 samples
    // The first half (t=0 to t=499) rises from 0 to near +amplitude
    auto ts = runSawtoothWave(R"({"num_samples": 1000, "amplitude": 1.0, "num_cycles": 1})");
    auto values = ts->getAnalogTimeSeries();

    // Check slope is approximately constant in the first half (before discontinuity)
    float const expected_slope = values[1] - values[0];
    REQUIRE(expected_slope > 0.0f);// rising
    for (size_t i = 1; i < 499; ++i) {
        float const slope = values[i + 1] - values[i];
        REQUIRE(slope == Catch::Approx(expected_slope).margin(1e-4f));
    }
}

TEST_CASE("SawtoothWave has sharp discontinuity at cycle boundary", "[SawtoothWave]") {
    // 1000 samples, num_cycles = 1 → discontinuity at the mid-point (t=500)
    auto ts = runSawtoothWave(R"({"num_samples": 1000, "amplitude": 1.0, "num_cycles": 1})");
    auto values = ts->getAnalogTimeSeries();

    // Just before the discontinuity, value should be near +amplitude
    REQUIRE(values[499] == Catch::Approx(1.0f).margin(0.01f));
    // Just after the discontinuity, value should be near -amplitude
    REQUIRE(values[500] == Catch::Approx(-1.0f).margin(0.01f));
}

TEST_CASE("SawtoothWave dc_offset shifts all values", "[SawtoothWave]") {
    float const dc = -2.0f;
    auto ts_no_dc = runSawtoothWave(R"({"num_samples": 100, "amplitude": 1.0, "num_cycles": 1})");
    auto ts_with_dc = runSawtoothWave(
            R"({"num_samples": 100, "amplitude": 1.0, "num_cycles": 1, "dc_offset": -2.0})");

    auto v_no = ts_no_dc->getAnalogTimeSeries();
    auto v_dc = ts_with_dc->getAnalogTimeSeries();
    for (size_t i = 0; i < v_no.size(); ++i) {
        REQUIRE(v_dc[i] == Catch::Approx(v_no[i] + dc).margin(1e-5f));
    }
}

TEST_CASE("SawtoothWave rejects num_samples <= 0", "[SawtoothWave]") {
    auto result = GeneratorRegistry::instance().generate(
            "SawtoothWave", R"({"num_samples": 0, "amplitude": 1.0, "num_cycles": 1})");
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("SawtoothWave rejects num_cycles <= 0", "[SawtoothWave]") {
    auto result = GeneratorRegistry::instance().generate(
            "SawtoothWave", R"({"num_samples": 100, "amplitude": 1.0, "num_cycles": -1.0})");
    REQUIRE_FALSE(result.has_value());
}
