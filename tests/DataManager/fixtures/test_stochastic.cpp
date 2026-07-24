#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "fixtures/stochastic/AR1Process.hpp"
#include "fixtures/stochastic/IntervalModulatedRate.hpp"
#include "fixtures/stochastic/PoissonThinning.hpp"
#include "fixtures/stochastic/RandomIntervals.hpp"

#include "TimeFrame/interval_data.hpp"

#include <cmath>
#include <numeric>
#include <vector>

namespace {

double computeMean(std::vector<float> const & values) {
    if (values.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (float const v: values) {
        sum += static_cast<double>(v);
    }
    return sum / static_cast<double>(values.size());
}

double computeLag1Autocorrelation(std::vector<float> const & values) {
    if (values.size() < 2) {
        return 0.0;
    }
    double const mean = computeMean(values);
    double numerator = 0.0;
    double denominator = 0.0;
    for (std::size_t t = 1; t < values.size(); ++t) {
        double const x0 = static_cast<double>(values[t - 1]) - mean;
        double const x1 = static_cast<double>(values[t]) - mean;
        numerator += x0 * x1;
        denominator += x0 * x0;
    }
    if (denominator == 0.0) {
        return 0.0;
    }
    return numerator / denominator;
}

}// namespace

TEST_CASE("AR1Process generates deterministic zero-mean series", "[stochastic][ar1]") {
    auto const first = generateAR1(5000, 0.9f, 0.436f, 42);
    auto const second = generateAR1(5000, 0.9f, 0.436f, 42);

    REQUIRE(first.size() == 5000);
    REQUIRE(first == second);
    REQUIRE(std::abs(computeMean(first)) < 0.15);
    REQUIRE(computeLag1Autocorrelation(first) == Catch::Approx(0.9).margin(0.05));
}

TEST_CASE("PoissonThinning produces events for constant rate", "[stochastic][poisson]") {
    std::vector<float> rate(100000, 0.001f);
    auto const events_a = thinInhomogeneousPoisson(rate, 99);
    auto const events_b = thinInhomogeneousPoisson(rate, 99);

    REQUIRE(events_a == events_b);
    REQUIRE(events_a.size() > 50);
    REQUIRE(events_a.size() < 200);
}

TEST_CASE("RandomIntervals produces intervals with expected mean duration", "[stochastic][intervals]") {
    auto const intervals = generateRandomIntervals(5000, 10.0f, 200.0f, 42);
    REQUIRE(intervals.size() > 5);

    double sum = 0.0;
    for (auto const & interval: intervals) {
        sum += static_cast<double>(interval.end - interval.start + 1);
    }
    double const mean = sum / static_cast<double>(intervals.size());
    REQUIRE(mean == Catch::Approx(10.0).margin(4.0));
}

TEST_CASE("IntervalModulatedRate boosts rate during contact", "[stochastic][rate]") {
    std::vector<float> curvature(100, 0.0f);
    std::vector<Interval> intervals = {Interval{10, 20}};

    auto const baseline_only = buildSpikeRate(
            6000, 100, 60, curvature, intervals, 0.001f, 0.0f, 0.0f, 60);
    auto const with_contact = buildSpikeRate(
            6000, 100, 60, curvature, intervals, 0.001f, 0.05f, 0.0f, 60);

    double const baseline_sum = std::accumulate(baseline_only.begin(), baseline_only.end(), 0.0);
    double const contact_sum = std::accumulate(with_contact.begin(), with_contact.end(), 0.0);
    REQUIRE(contact_sum > baseline_sum);
}
