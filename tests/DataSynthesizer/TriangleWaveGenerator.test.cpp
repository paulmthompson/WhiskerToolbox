/**
 * @file TriangleWaveGenerator.test.cpp
 * @brief Unit tests for the TriangleWave generator.
 */
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataSynthesizer/GeneratorRegistry.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace WhiskerToolbox::DataSynthesizer;

static std::shared_ptr<AnalogTimeSeries> runTriangleWave(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("TriangleWave", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<AnalogTimeSeries>>(*result);
}

TEST_CASE("TriangleWave produces correct output size", "[TriangleWave]") {
    auto ts = runTriangleWave(R"({"num_samples": 150, "amplitude": 1.0, "num_cycles": 1})");
    REQUIRE(ts->getNumSamples() == 150);
}

TEST_CASE("TriangleWave value at t=0 with default phase is ~0", "[TriangleWave]") {
    auto ts = runTriangleWave(R"({"num_samples": 100, "amplitude": 1.0, "num_cycles": 1})");
    auto values = ts->getAnalogTimeSeries();
    REQUIRE(values[0] == Catch::Approx(0.0f).margin(1e-5f));
}

TEST_CASE("TriangleWave values are bounded within [-amplitude, amplitude]", "[TriangleWave]") {
    float const amplitude = 2.5f;
    auto ts = runTriangleWave(R"({"num_samples": 1000, "amplitude": 2.5, "num_cycles": 10})");
    auto values = ts->getAnalogTimeSeries();
    for (auto v: values) {
        REQUIRE(v <= amplitude + 1e-5f);
        REQUIRE(v >= -amplitude - 1e-5f);
    }
}

TEST_CASE("TriangleWave one complete cycle reaches +amplitude and -amplitude", "[TriangleWave]") {
    float const amplitude = 3.0f;
    // 1000 samples, num_cycles = 1 → one complete cycle
    auto ts = runTriangleWave(R"({"num_samples": 1000, "amplitude": 3.0, "num_cycles": 1})");
    auto values = ts->getAnalogTimeSeries();

    float max_val = *std::max_element(values.begin(), values.end());
    float min_val = *std::min_element(values.begin(), values.end());

    // Should reach ±amplitude within a small tolerance (sample quantization)
    REQUIRE(max_val == Catch::Approx(amplitude).epsilon(0.01f));
    REQUIRE(min_val == Catch::Approx(-amplitude).epsilon(0.01f));
}

TEST_CASE("TriangleWave dc_offset shifts all values", "[TriangleWave]") {
    float const dc = -2.0f;
    auto ts_no_dc = runTriangleWave(R"({"num_samples": 100, "amplitude": 1.0, "num_cycles": 1})");
    auto ts_with_dc = runTriangleWave(
            R"({"num_samples": 100, "amplitude": 1.0, "num_cycles": 1, "dc_offset": -2.0})");

    auto v_no = ts_no_dc->getAnalogTimeSeries();
    auto v_dc = ts_with_dc->getAnalogTimeSeries();
    for (size_t i = 0; i < v_no.size(); ++i) {
        REQUIRE(v_dc[i] == Catch::Approx(v_no[i] + dc).margin(1e-5f));
    }
}

TEST_CASE("TriangleWave is linear between zero-crossings", "[TriangleWave]") {
    // 1000 samples, num_cycles = 1 → one complete cycle over 1000 samples
    // Peak at t=250, zero-crossing down at ~t=500, trough at t=750
    auto ts = runTriangleWave(R"({"num_samples": 1000, "amplitude": 1.0, "num_cycles": 1})");
    auto values = ts->getAnalogTimeSeries();

    // The waveform rises linearly from 0 to 1 (t=0 to t=250)
    // Check slope is approximately constant from t=1 to t=249
    float const expected_slope = values[1] - values[0];
    for (size_t i = 1; i < 249; ++i) {
        float const slope = values[i + 1] - values[i];
        REQUIRE(slope == Catch::Approx(expected_slope).margin(1e-4f));
    }
}

TEST_CASE("TriangleWave rejects num_samples <= 0", "[TriangleWave]") {
    auto result = GeneratorRegistry::instance().generate(
            "TriangleWave", R"({"num_samples": 0, "amplitude": 1.0, "num_cycles": 1})");
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("TriangleWave rejects num_cycles <= 0", "[TriangleWave]") {
    auto result = GeneratorRegistry::instance().generate(
            "TriangleWave", R"({"num_samples": 100, "amplitude": 1.0, "num_cycles": -1.0})");
    REQUIRE_FALSE(result.has_value());
}
