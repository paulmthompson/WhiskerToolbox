/**
 * @file InhomogeneousPoissonEventsGenerator.test.cpp
 * @brief Unit tests for the InhomogeneousPoissonEvents generator.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DataSynthesizer/GeneratorTypes.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using namespace WhiskerToolbox::DataSynthesizer;

namespace {

/// @brief Create a DataManager with a constant-rate AnalogTimeSeries
std::unique_ptr<DataManager> makeDMWithConstantRate(
        std::string const & key,
        std::size_t num_samples,
        float rate_value) {
    auto dm = std::make_unique<DataManager>();
    std::vector<float> values(num_samples, rate_value);
    auto series = std::make_shared<AnalogTimeSeries>(std::move(values), num_samples);
    dm->setData<AnalogTimeSeries>(key, series, TimeKey("time"));
    return dm;
}

/// @brief Create a DataManager with a custom-valued AnalogTimeSeries
std::unique_ptr<DataManager> makeDMWithRateSignal(
        std::string const & key,
        std::vector<float> values) {
    auto dm = std::make_unique<DataManager>();
    auto const n = values.size();
    auto series = std::make_shared<AnalogTimeSeries>(std::move(values), n);
    dm->setData<AnalogTimeSeries>(key, series, TimeKey("time"));
    return dm;
}

/// @brief Helper to run the InhomogeneousPoissonEvents generator
std::shared_ptr<DigitalEventSeries> runGenerator(
        DataManager const * dm,
        std::string const & json) {
    auto ctx = GeneratorContext{.data_manager = dm};
    auto result = GeneratorRegistry::instance().generate(
            "InhomogeneousPoissonEvents", json, ctx);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<DigitalEventSeries>>(*result);
}

}// namespace

TEST_CASE("InhomogeneousPoissonEvents produces events within range",
          "[InhomogeneousPoissonEvents]") {
    auto dm = makeDMWithConstantRate("rate", 1000, 0.1f);
    auto des = runGenerator(dm.get(),
                            R"({"rate_signal_key": "rate", "seed": 42})");

    REQUIRE(des->size() > 0);
    for (auto event: des->view()) {
        REQUIRE(event.time().getValue() >= 0);
        REQUIRE(event.time().getValue() < 1000);
    }
}

TEST_CASE("InhomogeneousPoissonEvents events are sorted",
          "[InhomogeneousPoissonEvents]") {
    auto dm = makeDMWithConstantRate("rate", 1000, 0.1f);
    auto des = runGenerator(dm.get(),
                            R"({"rate_signal_key": "rate", "seed": 42})");

    int64_t prev = -1;
    for (auto event: des->view()) {
        REQUIRE(event.time().getValue() > prev);
        prev = event.time().getValue();
    }
}

TEST_CASE("InhomogeneousPoissonEvents is deterministic with same seed",
          "[InhomogeneousPoissonEvents]") {
    auto dm = makeDMWithConstantRate("rate", 500, 0.1f);
    auto des1 = runGenerator(dm.get(),
                             R"({"rate_signal_key": "rate", "seed": 123})");
    auto des2 = runGenerator(dm.get(),
                             R"({"rate_signal_key": "rate", "seed": 123})");

    REQUIRE(des1->size() == des2->size());
    auto v1 = des1->view();
    auto v2 = des2->view();
    for (size_t i = 0; i < des1->size(); ++i) {
        REQUIRE(v1[i].time() == v2[i].time());
    }
}

TEST_CASE("InhomogeneousPoissonEvents different seeds produce different output",
          "[InhomogeneousPoissonEvents]") {
    auto dm = makeDMWithConstantRate("rate", 1000, 0.1f);
    auto des1 = runGenerator(dm.get(),
                             R"({"rate_signal_key": "rate", "seed": 1})");
    auto des2 = runGenerator(dm.get(),
                             R"({"rate_signal_key": "rate", "seed": 2})");

    bool differs = (des1->size() != des2->size());
    if (!differs) {
        auto v1 = des1->view();
        auto v2 = des2->view();
        for (size_t i = 0; i < des1->size(); ++i) {
            if (v1[i].time() != v2[i].time()) {
                differs = true;
                break;
            }
        }
    }
    REQUIRE(differs);
}

TEST_CASE("InhomogeneousPoissonEvents with constant rate matches homogeneous Poisson",
          "[InhomogeneousPoissonEvents]") {
    // With a constant rate, the inhomogeneous process should produce a similar
    // event count to num_samples * rate (within a generous tolerance)
    float const rate = 0.05f;
    int const num_samples = 10000;
    auto dm = makeDMWithConstantRate("rate", num_samples, rate);
    auto des = runGenerator(dm.get(),
                            R"({"rate_signal_key": "rate", "seed": 42})");

    auto const expected = static_cast<double>(num_samples) * rate;
    auto const actual = static_cast<double>(des->size());
    REQUIRE(actual > expected * 0.5);
    REQUIRE(actual < expected * 2.0);
}

TEST_CASE("InhomogeneousPoissonEvents rate_scale modulates event density",
          "[InhomogeneousPoissonEvents]") {
    auto dm = makeDMWithConstantRate("rate", 10000, 0.05f);
    auto des_low = runGenerator(dm.get(),
                                R"({"rate_signal_key": "rate", "rate_scale": 0.5, "seed": 42})");
    auto des_high = runGenerator(dm.get(),
                                 R"({"rate_signal_key": "rate", "rate_scale": 2.0, "seed": 42})");

    REQUIRE(des_high->size() > des_low->size());
}

TEST_CASE("InhomogeneousPoissonEvents concentrates events in high-rate regions",
          "[InhomogeneousPoissonEvents]") {
    // First half has rate 0, second half has rate 0.2
    std::vector<float> rates(1000, 0.0f);
    for (std::size_t i = 500; i < 1000; ++i) {
        rates[i] = 0.2f;
    }
    auto dm = makeDMWithRateSignal("rate", std::move(rates));
    auto des = runGenerator(dm.get(),
                            R"({"rate_signal_key": "rate", "seed": 42})");

    // All events should be in the second half [500, 999]
    REQUIRE(des->size() > 0);
    for (auto event: des->view()) {
        REQUIRE(event.time().getValue() >= 500);
    }
}

TEST_CASE("InhomogeneousPoissonEvents with zero rate produces no events",
          "[InhomogeneousPoissonEvents]") {
    auto dm = makeDMWithConstantRate("rate", 1000, 0.0f);
    auto des = runGenerator(dm.get(),
                            R"({"rate_signal_key": "rate", "seed": 42})");
    REQUIRE(des->size() == 0);
}

TEST_CASE("InhomogeneousPoissonEvents rejects missing DataManager",
          "[InhomogeneousPoissonEvents]") {
    auto result = GeneratorRegistry::instance().generate(
            "InhomogeneousPoissonEvents",
            R"({"rate_signal_key": "rate", "seed": 42})");
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("InhomogeneousPoissonEvents rejects missing rate_signal_key",
          "[InhomogeneousPoissonEvents]") {
    auto dm = makeDMWithConstantRate("rate", 100, 0.1f);
    auto ctx = GeneratorContext{.data_manager = dm.get()};
    auto result = GeneratorRegistry::instance().generate(
            "InhomogeneousPoissonEvents",
            R"({"rate_signal_key": "nonexistent", "seed": 42})",
            ctx);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("InhomogeneousPoissonEvents rejects empty rate_signal_key",
          "[InhomogeneousPoissonEvents]") {
    auto dm = makeDMWithConstantRate("rate", 100, 0.1f);
    auto ctx = GeneratorContext{.data_manager = dm.get()};
    auto result = GeneratorRegistry::instance().generate(
            "InhomogeneousPoissonEvents",
            R"({"rate_signal_key": "", "seed": 42})",
            ctx);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("InhomogeneousPoissonEvents rejects negative rate values",
          "[InhomogeneousPoissonEvents]") {
    auto dm = makeDMWithRateSignal("rate", {0.1f, -0.5f, 0.2f});
    auto ctx = GeneratorContext{.data_manager = dm.get()};
    auto result = GeneratorRegistry::instance().generate(
            "InhomogeneousPoissonEvents",
            R"({"rate_signal_key": "rate", "seed": 42})",
            ctx);
    REQUIRE_FALSE(result.has_value());
}
