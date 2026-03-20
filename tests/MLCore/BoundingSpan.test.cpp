/**
 * @file BoundingSpan.test.cpp
 * @brief Unit tests for BoundingSpan computation and row filtering
 */

#include <catch2/catch_test_macros.hpp>

#include "pipelines/BoundingSpan.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/TimeFrameIndex.hpp"
#include "TimeFrame/interval_data.hpp"

#include <vector>

using namespace MLCore;

// ============================================================================
// Helpers
// ============================================================================

namespace {

std::vector<TimeFrameIndex> makeTimes(std::initializer_list<int64_t> vals) {
    std::vector<TimeFrameIndex> times;
    times.reserve(vals.size());
    for (auto v: vals) {
        times.emplace_back(v);
    }
    return times;
}

arma::mat makeFeatures(std::size_t n_obs) {
    arma::mat m(2, n_obs);
    for (std::size_t i = 0; i < n_obs; ++i) {
        m(0, i) = static_cast<double>(i);
        m(1, i) = static_cast<double>(i) * 10.0;
    }
    return m;
}

std::shared_ptr<DigitalIntervalSeries> makeIntervals(
        std::initializer_list<std::pair<int64_t, int64_t>> intervals) {
    auto series = std::make_shared<DigitalIntervalSeries>();
    for (auto const & [start, end]: intervals) {
        series->addEvent(Interval{start, end});
    }
    return series;
}

}// anonymous namespace

// ============================================================================
// computeIntervalBounds
// ============================================================================

TEST_CASE("computeIntervalBounds returns nullopt for empty series",
          "[BoundingSpan]") {
    DigitalIntervalSeries empty_series;
    auto result = computeIntervalBounds(empty_series);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("computeIntervalBounds single interval",
          "[BoundingSpan]") {
    auto series = makeIntervals({{10, 20}});
    auto result = computeIntervalBounds(*series);
    REQUIRE(result.has_value());
    CHECK(result->min_time == TimeFrameIndex(10));
    CHECK(result->max_time == TimeFrameIndex(20));
}

TEST_CASE("computeIntervalBounds multiple non-overlapping intervals",
          "[BoundingSpan]") {
    auto series = makeIntervals({{100, 200}, {50, 80}, {300, 400}});
    auto result = computeIntervalBounds(*series);
    REQUIRE(result.has_value());
    CHECK(result->min_time == TimeFrameIndex(50));
    CHECK(result->max_time == TimeFrameIndex(400));
}

TEST_CASE("computeIntervalBounds overlapping intervals",
          "[BoundingSpan]") {
    auto series = makeIntervals({{10, 50}, {30, 70}});
    auto result = computeIntervalBounds(*series);
    REQUIRE(result.has_value());
    CHECK(result->min_time == TimeFrameIndex(10));
    CHECK(result->max_time == TimeFrameIndex(70));
}

// ============================================================================
// computeRowTimeBounds
// ============================================================================

TEST_CASE("computeRowTimeBounds returns nullopt for empty vector",
          "[BoundingSpan]") {
    std::vector<TimeFrameIndex> empty;
    auto result = computeRowTimeBounds(empty);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("computeRowTimeBounds single element",
          "[BoundingSpan]") {
    auto times = makeTimes({42});
    auto result = computeRowTimeBounds(times);
    REQUIRE(result.has_value());
    CHECK(result->min_time == TimeFrameIndex(42));
    CHECK(result->max_time == TimeFrameIndex(42));
}

TEST_CASE("computeRowTimeBounds multiple elements",
          "[BoundingSpan]") {
    auto times = makeTimes({5, 100, 3, 50, 99});
    auto result = computeRowTimeBounds(times);
    REQUIRE(result.has_value());
    CHECK(result->min_time == TimeFrameIndex(3));
    CHECK(result->max_time == TimeFrameIndex(100));
}

// ============================================================================
// mergeSpans
// ============================================================================

TEST_CASE("mergeSpans disjoint spans", "[BoundingSpan]") {
    BoundingSpan a{TimeFrameIndex(10), TimeFrameIndex(20)};
    BoundingSpan b{TimeFrameIndex(50), TimeFrameIndex(100)};
    auto merged = mergeSpans(a, b);
    CHECK(merged.min_time == TimeFrameIndex(10));
    CHECK(merged.max_time == TimeFrameIndex(100));
}

TEST_CASE("mergeSpans overlapping spans", "[BoundingSpan]") {
    BoundingSpan a{TimeFrameIndex(10), TimeFrameIndex(60)};
    BoundingSpan b{TimeFrameIndex(50), TimeFrameIndex(100)};
    auto merged = mergeSpans(a, b);
    CHECK(merged.min_time == TimeFrameIndex(10));
    CHECK(merged.max_time == TimeFrameIndex(100));
}

TEST_CASE("mergeSpans identical spans", "[BoundingSpan]") {
    BoundingSpan a{TimeFrameIndex(10), TimeFrameIndex(20)};
    auto merged = mergeSpans(a, a);
    CHECK(merged.min_time == TimeFrameIndex(10));
    CHECK(merged.max_time == TimeFrameIndex(20));
}

TEST_CASE("mergeSpans is commutative", "[BoundingSpan]") {
    BoundingSpan a{TimeFrameIndex(5), TimeFrameIndex(15)};
    BoundingSpan b{TimeFrameIndex(20), TimeFrameIndex(30)};
    auto ab = mergeSpans(a, b);
    auto ba = mergeSpans(b, a);
    CHECK(ab.min_time == ba.min_time);
    CHECK(ab.max_time == ba.max_time);
}

// ============================================================================
// filterRowsToSpan
// ============================================================================

TEST_CASE("filterRowsToSpan keeps all rows when span covers everything",
          "[BoundingSpan]") {
    auto features = makeFeatures(5);
    auto times = makeTimes({10, 20, 30, 40, 50});
    BoundingSpan span{TimeFrameIndex(0), TimeFrameIndex(100)};

    auto result = filterRowsToSpan(features, times, span);
    CHECK(result.features.n_cols == 5);
    CHECK(result.times.size() == 5);
}

TEST_CASE("filterRowsToSpan filters to subset",
          "[BoundingSpan]") {
    auto features = makeFeatures(5);
    auto times = makeTimes({10, 20, 30, 40, 50});
    BoundingSpan span{TimeFrameIndex(20), TimeFrameIndex(40)};

    auto result = filterRowsToSpan(features, times, span);
    REQUIRE(result.features.n_cols == 3);
    REQUIRE(result.times.size() == 3);
    CHECK(result.times[0] == TimeFrameIndex(20));
    CHECK(result.times[1] == TimeFrameIndex(30));
    CHECK(result.times[2] == TimeFrameIndex(40));
    // Verify feature values correspond to original columns 1, 2, 3
    CHECK(result.features(0, 0) == 1.0);
    CHECK(result.features(0, 1) == 2.0);
    CHECK(result.features(0, 2) == 3.0);
}

TEST_CASE("filterRowsToSpan returns empty when no rows in span",
          "[BoundingSpan]") {
    auto features = makeFeatures(3);
    auto times = makeTimes({10, 20, 30});
    BoundingSpan span{TimeFrameIndex(100), TimeFrameIndex(200)};

    auto result = filterRowsToSpan(features, times, span);
    CHECK(result.features.n_cols == 0);
    CHECK(result.times.empty());
}

TEST_CASE("filterRowsToSpan span endpoints are inclusive",
          "[BoundingSpan]") {
    auto features = makeFeatures(5);
    auto times = makeTimes({10, 20, 30, 40, 50});
    BoundingSpan span{TimeFrameIndex(10), TimeFrameIndex(50)};

    auto result = filterRowsToSpan(features, times, span);
    CHECK(result.features.n_cols == 5);
    CHECK(result.times.size() == 5);
}

TEST_CASE("filterRowsToSpan with non-contiguous times",
          "[BoundingSpan]") {
    auto features = makeFeatures(6);
    auto times = makeTimes({5, 10, 50, 100, 200, 500});
    BoundingSpan span{TimeFrameIndex(10), TimeFrameIndex(200)};

    auto result = filterRowsToSpan(features, times, span);
    REQUIRE(result.features.n_cols == 4);
    REQUIRE(result.times.size() == 4);
    CHECK(result.times[0] == TimeFrameIndex(10));
    CHECK(result.times[1] == TimeFrameIndex(50));
    CHECK(result.times[2] == TimeFrameIndex(100));
    CHECK(result.times[3] == TimeFrameIndex(200));
}
