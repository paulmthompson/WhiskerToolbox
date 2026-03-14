/**
 * @file RegularEventsGenerator.test.cpp
 * @brief Unit tests for the RegularEvents generator.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <memory>
#include <string>

using namespace WhiskerToolbox::DataSynthesizer;

static std::shared_ptr<DigitalEventSeries> runRegularEvents(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("RegularEvents", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<DigitalEventSeries>>(*result);
}

TEST_CASE("RegularEvents produces correct number of events without jitter", "[RegularEvents]") {
    auto des = runRegularEvents(R"({"num_samples": 100, "interval": 10})");
    // Events at 0, 10, 20, ..., 90 => 10 events
    REQUIRE(des->size() == 10);
}

TEST_CASE("RegularEvents events are evenly spaced without jitter", "[RegularEvents]") {
    auto des = runRegularEvents(R"({"num_samples": 100, "interval": 25})");
    // Events at 0, 25, 50, 75 => 4 events
    REQUIRE(des->size() == 4);

    auto v = des->view();
    REQUIRE(v[0].time().getValue() == 0);
    REQUIRE(v[1].time().getValue() == 25);
    REQUIRE(v[2].time().getValue() == 50);
    REQUIRE(v[3].time().getValue() == 75);
}

TEST_CASE("RegularEvents events are within range", "[RegularEvents]") {
    auto des = runRegularEvents(
            R"({"num_samples": 1000, "interval": 20, "jitter_stddev": 5.0, "seed": 42})");
    REQUIRE(des->size() > 0);

    for (auto event: des->view()) {
        REQUIRE(event.time().getValue() >= 0);
        REQUIRE(event.time().getValue() < 1000);
    }
}

TEST_CASE("RegularEvents is deterministic with the same seed", "[RegularEvents]") {
    auto des1 = runRegularEvents(
            R"({"num_samples": 500, "interval": 10, "jitter_stddev": 3.0, "seed": 123})");
    auto des2 = runRegularEvents(
            R"({"num_samples": 500, "interval": 10, "jitter_stddev": 3.0, "seed": 123})");

    REQUIRE(des1->size() == des2->size());
    auto v1 = des1->view();
    auto v2 = des2->view();
    for (size_t i = 0; i < des1->size(); ++i) {
        REQUIRE(v1[i].time() == v2[i].time());
    }
}

TEST_CASE("RegularEvents jitter displaces events from grid positions", "[RegularEvents]") {
    auto des_no_jitter = runRegularEvents(R"({"num_samples": 1000, "interval": 50})");
    auto des_jitter = runRegularEvents(
            R"({"num_samples": 1000, "interval": 50, "jitter_stddev": 10.0, "seed": 42})");

    // With jitter, at least some events should be displaced from the grid
    bool any_displaced = false;
    auto v_no = des_no_jitter->view();
    auto v_jit = des_jitter->view();

    // Sizes may differ due to deduplication, but check the grids differ
    if (des_no_jitter->size() == des_jitter->size()) {
        for (size_t i = 0; i < des_no_jitter->size(); ++i) {
            if (v_no[i].time() != v_jit[i].time()) {
                any_displaced = true;
                break;
            }
        }
    } else {
        any_displaced = true;
    }
    REQUIRE(any_displaced);
}

TEST_CASE("RegularEvents rejects invalid parameters", "[RegularEvents]") {
    REQUIRE_FALSE(
            GeneratorRegistry::instance()
                    .generate("RegularEvents", R"({"num_samples": 0, "interval": 10})")
                    .has_value());
    REQUIRE_FALSE(
            GeneratorRegistry::instance()
                    .generate("RegularEvents", R"({"num_samples": 100, "interval": 0})")
                    .has_value());
    REQUIRE_FALSE(
            GeneratorRegistry::instance()
                    .generate("RegularEvents",
                              R"({"num_samples": 100, "interval": 10, "jitter_stddev": -1.0})")
                    .has_value());
}
