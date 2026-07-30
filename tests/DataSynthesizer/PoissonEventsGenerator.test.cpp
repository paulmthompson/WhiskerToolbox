/**
 * @file PoissonEventsGenerator.test.cpp
 * @brief Unit tests for the PoissonEvents generator.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>

using namespace Neuralyzer::DataSynthesizer;

static std::shared_ptr<DigitalEventSeries> runPoissonEvents(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("PoissonEvents", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<DigitalEventSeries>>(*result);
}

TEST_CASE("PoissonEvents produces events within range", "[PoissonEvents]") {
    auto des = runPoissonEvents(R"({"num_samples": 1000, "lambda": 0.1, "seed": 42})");
    REQUIRE(des->size() > 0);

    for (std::size_t i = 0; i < des->size(); ++i) {
        auto const event_time = des->getStoredEvent(i).getValue();
        REQUIRE(event_time >= 0);
        REQUIRE(event_time < 1000);
    }
}

TEST_CASE("PoissonEvents events are sorted", "[PoissonEvents]") {
    auto des = runPoissonEvents(R"({"num_samples": 1000, "lambda": 0.1, "seed": 42})");

    int64_t prev = -1;
    for (std::size_t i = 0; i < des->size(); ++i) {
        auto const event_time = des->getStoredEvent(i).getValue();
        REQUIRE(event_time > prev);
        prev = event_time;
    }
}

TEST_CASE("PoissonEvents is deterministic with the same seed", "[PoissonEvents]") {
    auto des1 = runPoissonEvents(R"({"num_samples": 500, "lambda": 0.1, "seed": 123})");
    auto des2 = runPoissonEvents(R"({"num_samples": 500, "lambda": 0.1, "seed": 123})");

    REQUIRE(des1->size() == des2->size());
    for (std::size_t i = 0; i < des1->size(); ++i) {
        REQUIRE(des1->getStoredEvent(i) == des2->getStoredEvent(i));
    }
}

TEST_CASE("PoissonEvents different seeds produce different output", "[PoissonEvents]") {
    auto des1 = runPoissonEvents(R"({"num_samples": 1000, "lambda": 0.1, "seed": 1})");
    auto des2 = runPoissonEvents(R"({"num_samples": 1000, "lambda": 0.1, "seed": 2})");

    // Size or event times should differ
    bool differs = (des1->size() != des2->size());
    if (!differs) {
        for (std::size_t i = 0; i < des1->size(); ++i) {
            if (des1->getStoredEvent(i) != des2->getStoredEvent(i)) {
                differs = true;
                break;
            }
        }
    }
    REQUIRE(differs);
}

TEST_CASE("PoissonEvents higher lambda produces more events on average", "[PoissonEvents]") {
    auto des_low = runPoissonEvents(R"({"num_samples": 10000, "lambda": 0.01, "seed": 42})");
    auto des_high = runPoissonEvents(R"({"num_samples": 10000, "lambda": 0.1, "seed": 42})");

    REQUIRE(des_high->size() > des_low->size());
}

TEST_CASE("PoissonEvents rejects invalid parameters", "[PoissonEvents]") {
    REQUIRE_FALSE(
            GeneratorRegistry::instance()
                    .generate("PoissonEvents", R"({"num_samples": 0, "lambda": 0.1})")
                    .has_value());
    REQUIRE_FALSE(
            GeneratorRegistry::instance()
                    .generate("PoissonEvents", R"({"num_samples": 100, "lambda": -1.0})")
                    .has_value());
}
