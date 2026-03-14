/**
 * @file RandomIntervalsGenerator.test.cpp
 * @brief Unit tests for the RandomIntervals generator.
 */
#include "DataSynthesizer/GeneratorRegistry.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>

using namespace WhiskerToolbox::DataSynthesizer;

static std::shared_ptr<DigitalIntervalSeries> runRandomIntervals(std::string const & json) {
    auto result = GeneratorRegistry::instance().generate("RandomIntervals", json);
    REQUIRE(result.has_value());
    return std::get<std::shared_ptr<DigitalIntervalSeries>>(*result);
}

TEST_CASE("RandomIntervals produces intervals within range", "[RandomIntervals]") {
    auto dis = runRandomIntervals(
            R"({"num_samples": 10000, "mean_duration": 50.0, "mean_gap": 100.0, "seed": 42})");
    REQUIRE(dis->size() > 0);

    for (auto interval: dis->view()) {
        REQUIRE(interval.value().start >= 0);
        REQUIRE(interval.value().end < 10000);
        REQUIRE(interval.value().start <= interval.value().end);
    }
}

TEST_CASE("RandomIntervals intervals are sorted and non-overlapping", "[RandomIntervals]") {
    auto dis = runRandomIntervals(
            R"({"num_samples": 10000, "mean_duration": 30.0, "mean_gap": 50.0, "seed": 42})");

    int64_t prev_end = -1;
    for (auto interval: dis->view()) {
        REQUIRE(interval.value().start > prev_end);
        REQUIRE(interval.value().start <= interval.value().end);
        prev_end = interval.value().end;
    }
}

TEST_CASE("RandomIntervals is deterministic with the same seed", "[RandomIntervals]") {
    std::string const json =
            R"({"num_samples": 5000, "mean_duration": 40.0, "mean_gap": 80.0, "seed": 99})";
    auto dis1 = runRandomIntervals(json);
    auto dis2 = runRandomIntervals(json);

    REQUIRE(dis1->size() == dis2->size());
    auto v1 = dis1->view();
    auto v2 = dis2->view();
    for (size_t i = 0; i < dis1->size(); ++i) {
        REQUIRE(v1[i].value().start == v2[i].value().start);
        REQUIRE(v1[i].value().end == v2[i].value().end);
    }
}

TEST_CASE("RandomIntervals different seeds produce different output", "[RandomIntervals]") {
    auto dis1 = runRandomIntervals(
            R"({"num_samples": 10000, "mean_duration": 50.0, "mean_gap": 100.0, "seed": 1})");
    auto dis2 = runRandomIntervals(
            R"({"num_samples": 10000, "mean_duration": 50.0, "mean_gap": 100.0, "seed": 2})");

    bool differs = (dis1->size() != dis2->size());
    if (!differs) {
        auto v1 = dis1->view();
        auto v2 = dis2->view();
        for (size_t i = 0; i < dis1->size(); ++i) {
            if (v1[i].value().start != v2[i].value().start ||
                v1[i].value().end != v2[i].value().end) {
                differs = true;
                break;
            }
        }
    }
    REQUIRE(differs);
}

TEST_CASE("RandomIntervals smaller mean_gap produces more intervals", "[RandomIntervals]") {
    auto dis_sparse = runRandomIntervals(
            R"({"num_samples": 50000, "mean_duration": 20.0, "mean_gap": 500.0, "seed": 42})");
    auto dis_dense = runRandomIntervals(
            R"({"num_samples": 50000, "mean_duration": 20.0, "mean_gap": 50.0, "seed": 42})");

    REQUIRE(dis_dense->size() > dis_sparse->size());
}

TEST_CASE("RandomIntervals rejects invalid parameters", "[RandomIntervals]") {
    REQUIRE_FALSE(
            GeneratorRegistry::instance()
                    .generate("RandomIntervals",
                              R"({"num_samples": 0, "mean_duration": 50.0, "mean_gap": 100.0})")
                    .has_value());
    REQUIRE_FALSE(
            GeneratorRegistry::instance()
                    .generate("RandomIntervals",
                              R"({"num_samples": 1000, "mean_duration": -1.0, "mean_gap": 100.0})")
                    .has_value());
    REQUIRE_FALSE(
            GeneratorRegistry::instance()
                    .generate("RandomIntervals",
                              R"({"num_samples": 1000, "mean_duration": 50.0, "mean_gap": 0.0})")
                    .has_value());
}
