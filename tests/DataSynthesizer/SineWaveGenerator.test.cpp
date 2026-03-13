/**
 * @file SineWaveGenerator.test.cpp
 * @brief Unit tests for the SineWave generator.
 */
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataSynthesizer/GeneratorRegistry.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <numbers>

using namespace WhiskerToolbox::DataSynthesizer;

static std::shared_ptr<AnalogTimeSeries> runSineWave(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("SineWave", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<AnalogTimeSeries>>(*result);
}

TEST_CASE("SineWave produces correct output size", "[SineWave]") {
    auto ts = runSineWave(R"({"num_samples": 200, "amplitude": 1.0, "num_cycles": 2})");
    REQUIRE(ts->getNumSamples() == 200);
}

TEST_CASE("SineWave value at t=0 with default phase is ~0", "[SineWave]") {
    auto ts = runSineWave(R"({"num_samples": 100, "amplitude": 1.0, "num_cycles": 1})");
    auto values = ts->getAnalogTimeSeries();
    REQUIRE(values[0] == Catch::Approx(0.0f).margin(1e-6f));
}

TEST_CASE("SineWave values are bounded within [-amplitude, amplitude]", "[SineWave]") {
    float const amplitude = 3.5f;
    auto ts = runSineWave(R"({"num_samples": 1000, "amplitude": 3.5, "num_cycles": 10})");
    auto values = ts->getAnalogTimeSeries();
    for (auto v: values) {
        REQUIRE(v <= amplitude + 1e-5f);
        REQUIRE(v >= -amplitude - 1e-5f);
    }
}

TEST_CASE("SineWave dc_offset shifts all values", "[SineWave]") {
    float const dc = 5.0f;
    auto ts_no_dc = runSineWave(R"({"num_samples": 100, "amplitude": 1.0, "num_cycles": 1})");
    auto ts_with_dc = runSineWave(R"({"num_samples": 100, "amplitude": 1.0, "num_cycles": 1, "dc_offset": 5.0})");

    auto v_no = ts_no_dc->getAnalogTimeSeries();
    auto v_dc = ts_with_dc->getAnalogTimeSeries();
    REQUIRE(v_no.size() == v_dc.size());
    for (size_t i = 0; i < v_no.size(); ++i) {
        REQUIRE(v_dc[i] == Catch::Approx(v_no[i] + dc).margin(1e-5f));
    }
}

TEST_CASE("SineWave one complete cycle peaks at +amplitude and -amplitude", "[SineWave]") {
    float const amplitude = 2.0f;
    // 1000 samples, num_cycles = 1 → one complete cycle
    auto ts = runSineWave(R"({"num_samples": 1000, "amplitude": 2.0, "num_cycles": 1})");
    auto values = ts->getAnalogTimeSeries();

    float max_val = *std::max_element(values.begin(), values.end());
    float min_val = *std::min_element(values.begin(), values.end());

    // Peak should be very close to ±amplitude (sample at t=250 for 1000-sample cycle)
    REQUIRE(max_val == Catch::Approx(amplitude).epsilon(0.01f));
    REQUIRE(min_val == Catch::Approx(-amplitude).epsilon(0.01f));
}

TEST_CASE("SineWave phase shifts the waveform", "[SineWave]") {
    // With phase = π/2, SineWave becomes a cosine: value at t=0 should be amplitude
    float const amplitude = 1.0f;
    float const half_pi = std::numbers::pi_v<float> / 2.0f;
    // JSON doesn't allow runtime float expressions, compute the string manually
    // π/2 ≈ 1.5707963
    auto ts = runSineWave(R"({"num_samples": 100, "amplitude": 1.0, "num_cycles": 1, "phase": 1.5707963})");
    auto values = ts->getAnalogTimeSeries();
    // sin(π/2) = 1
    REQUIRE(values[0] == Catch::Approx(amplitude).margin(1e-5f));
}

TEST_CASE("SineWave rejects num_samples <= 0", "[SineWave]") {
    auto result = GeneratorRegistry::instance().generate(
            "SineWave", R"({"num_samples": 0, "amplitude": 1.0, "num_cycles": 1})");
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("SineWave rejects num_cycles <= 0", "[SineWave]") {
    auto result = GeneratorRegistry::instance().generate(
            "SineWave", R"({"num_samples": 100, "amplitude": 1.0, "num_cycles": 0.0})");
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("SineWave with explicit cycle_length", "[SineWave]") {
    // 5 cycles over 500 samples in a 1000-sample signal → same freq as 5/500 = 0.01 c/s
    auto ts = runSineWave(R"({"num_samples": 1000, "amplitude": 1.0, "num_cycles": 5, "cycle_length": 500})");
    auto values = ts->getAnalogTimeSeries();
    // Period is 100 samples. Value at sample 25 (quarter-period) should be +amplitude
    REQUIRE(values[25] == Catch::Approx(1.0f).margin(0.01f));
    // Value at sample 100 (full period) should be ~0
    REQUIRE(values[100] == Catch::Approx(0.0f).margin(0.01f));
}
