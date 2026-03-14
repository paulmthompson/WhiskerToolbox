/**
 * @file BurstEventsGenerator.test.cpp
 * @brief Unit tests for the BurstEvents generator.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>

using namespace WhiskerToolbox::DataSynthesizer;

static std::shared_ptr<DigitalEventSeries> runBurstEvents(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("BurstEvents", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<DigitalEventSeries>>(*result);
}

TEST_CASE("BurstEvents produces events within range", "[BurstEvents]") {
    auto des = runBurstEvents(
            R"({"num_samples": 10000, "burst_rate": 0.005, "within_burst_rate": 0.5, "burst_duration": 20, "seed": 42})");
    REQUIRE(des->size() > 0);

    for (auto event: des->view()) {
        REQUIRE(event.time().getValue() >= 0);
        REQUIRE(event.time().getValue() < 10000);
    }
}

TEST_CASE("BurstEvents events are sorted", "[BurstEvents]") {
    auto des = runBurstEvents(
            R"({"num_samples": 10000, "burst_rate": 0.005, "within_burst_rate": 0.5, "burst_duration": 20, "seed": 42})");

    int64_t prev = -1;
    for (auto event: des->view()) {
        REQUIRE(event.time().getValue() > prev);
        prev = event.time().getValue();
    }
}

TEST_CASE("BurstEvents is deterministic with the same seed", "[BurstEvents]") {
    std::string const json =
            R"({"num_samples": 5000, "burst_rate": 0.01, "within_burst_rate": 0.3, "burst_duration": 15, "seed": 99})";
    auto des1 = runBurstEvents(json);
    auto des2 = runBurstEvents(json);

    REQUIRE(des1->size() == des2->size());
    auto v1 = des1->view();
    auto v2 = des2->view();
    for (size_t i = 0; i < des1->size(); ++i) {
        REQUIRE(v1[i].time() == v2[i].time());
    }
}

TEST_CASE("BurstEvents different seeds produce different output", "[BurstEvents]") {
    auto des1 = runBurstEvents(
            R"({"num_samples": 10000, "burst_rate": 0.005, "within_burst_rate": 0.5, "burst_duration": 20, "seed": 1})");
    auto des2 = runBurstEvents(
            R"({"num_samples": 10000, "burst_rate": 0.005, "within_burst_rate": 0.5, "burst_duration": 20, "seed": 2})");

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

TEST_CASE("BurstEvents higher burst_rate produces more events", "[BurstEvents]") {
    auto des_low = runBurstEvents(
            R"({"num_samples": 50000, "burst_rate": 0.001, "within_burst_rate": 0.5, "burst_duration": 10, "seed": 42})");
    auto des_high = runBurstEvents(
            R"({"num_samples": 50000, "burst_rate": 0.01, "within_burst_rate": 0.5, "burst_duration": 10, "seed": 42})");

    REQUIRE(des_high->size() > des_low->size());
}

TEST_CASE("BurstEvents rejects invalid parameters", "[BurstEvents]") {
    REQUIRE_FALSE(GeneratorRegistry::instance()
                          .generate("BurstEvents",
                                    R"({"num_samples": 0, "burst_rate": 0.005, "within_burst_rate": 0.5, "burst_duration": 20})")
                          .has_value());
    REQUIRE_FALSE(GeneratorRegistry::instance()
                          .generate("BurstEvents",
                                    R"({"num_samples": 1000, "burst_rate": -1.0, "within_burst_rate": 0.5, "burst_duration": 20})")
                          .has_value());
    REQUIRE_FALSE(GeneratorRegistry::instance()
                          .generate("BurstEvents",
                                    R"({"num_samples": 1000, "burst_rate": 0.005, "within_burst_rate": 0.5, "burst_duration": 0})")
                          .has_value());
}
