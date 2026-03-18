/**
 * @file SequenceAssembler.test.cpp
 * @brief Unit tests for SequenceAssembler contiguous sequence segmentation
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "pipelines/SequenceAssembler.hpp"

#include "TimeFrame/TimeFrameIndex.hpp"

#include <vector>

using Catch::Matchers::WithinAbs;

// ============================================================================
// Helpers
// ============================================================================

namespace {

/**
 * @brief Build a vector of TimeFrameIndex from raw int64_t values
 */
std::vector<TimeFrameIndex> makeTimes(std::initializer_list<int64_t> vals) {
    std::vector<TimeFrameIndex> times;
    times.reserve(vals.size());
    for (auto v: vals) {
        times.emplace_back(v);
    }
    return times;
}

/**
 * @brief Create a simple feature matrix (2 features × n_obs)
 *
 * Row 0 = observation index (0, 1, 2, ...)
 * Row 1 = observation index * 10
 */
arma::mat makeFeatures(std::size_t n_obs) {
    arma::mat m(2, n_obs);
    for (std::size_t i = 0; i < n_obs; ++i) {
        m(0, i) = static_cast<double>(i);
        m(1, i) = static_cast<double>(i) * 10.0;
    }
    return m;
}

/**
 * @brief Create a label vector from raw values
 */
arma::Row<std::size_t> makeLabels(std::initializer_list<std::size_t> vals) {
    arma::Row<std::size_t> labels(vals.size());
    std::size_t idx = 0;
    for (auto v: vals) {
        labels(idx++) = v;
    }
    return labels;
}

}// namespace

// ============================================================================
// Empty input
// ============================================================================

TEST_CASE("SequenceAssembler empty input returns empty", "[MLCore][SequenceAssembler]") {
    arma::mat const features(2, 0);
    arma::Row<std::size_t> const labels;
    std::vector<TimeFrameIndex> const times;

    auto segments = MLCore::SequenceAssembler::segment(features, labels, times);

    REQUIRE(segments.empty());
}

TEST_CASE("SequenceAssembler empty input unlabeled returns empty", "[MLCore][SequenceAssembler]") {
    arma::mat const features(2, 0);
    std::vector<TimeFrameIndex> const times;

    auto segments = MLCore::SequenceAssembler::segment(features, times);

    REQUIRE(segments.empty());
}

// ============================================================================
// Single-frame sequences
// ============================================================================

TEST_CASE("SequenceAssembler single frame filtered by default min_length", "[MLCore][SequenceAssembler]") {
    // Single frame: default min_sequence_length = 2 should filter it out
    auto features = makeFeatures(1);
    auto labels = makeLabels({0});
    auto times = makeTimes({100});

    auto segments = MLCore::SequenceAssembler::segment(features, labels, times);

    REQUIRE(segments.empty());
}

TEST_CASE("SequenceAssembler single frame kept with min_length=1", "[MLCore][SequenceAssembler]") {
    auto features = makeFeatures(1);
    auto labels = makeLabels({1});
    auto times = makeTimes({42});

    MLCore::SequenceAssemblerConfig config;
    config.min_sequence_length = 1;

    auto segments = MLCore::SequenceAssembler::segment(features, labels, times, config);

    REQUIRE(segments.size() == 1);
    REQUIRE(segments[0].features.n_cols == 1);
    REQUIRE(segments[0].labels.n_elem == 1);
    REQUIRE(segments[0].labels(0) == 1);
    REQUIRE(segments[0].times.size() == 1);
    REQUIRE(segments[0].times[0].getValue() == 42);
}

// ============================================================================
// No gaps (single contiguous sequence)
// ============================================================================

TEST_CASE("SequenceAssembler no gaps produces single segment", "[MLCore][SequenceAssembler]") {
    auto features = makeFeatures(5);
    auto labels = makeLabels({0, 1, 1, 0, 0});
    auto times = makeTimes({10, 11, 12, 13, 14});

    auto segments = MLCore::SequenceAssembler::segment(features, labels, times);

    REQUIRE(segments.size() == 1);
    REQUIRE(segments[0].features.n_cols == 5);
    REQUIRE(segments[0].features.n_rows == 2);
    REQUIRE(segments[0].labels.n_elem == 5);
    REQUIRE(segments[0].times.size() == 5);

    // Verify feature values are preserved
    REQUIRE_THAT(segments[0].features(0, 0), WithinAbs(0.0, 1e-12));
    REQUIRE_THAT(segments[0].features(0, 4), WithinAbs(4.0, 1e-12));
    REQUIRE_THAT(segments[0].features(1, 2), WithinAbs(20.0, 1e-12));

    // Verify labels are preserved
    REQUIRE(segments[0].labels(0) == 0);
    REQUIRE(segments[0].labels(1) == 1);
    REQUIRE(segments[0].labels(2) == 1);

    // Verify times are preserved
    REQUIRE(segments[0].times[0].getValue() == 10);
    REQUIRE(segments[0].times[4].getValue() == 14);
}

// ============================================================================
// All gaps (every frame is isolated)
// ============================================================================

TEST_CASE("SequenceAssembler all gaps filters everything with default config", "[MLCore][SequenceAssembler]") {
    // Times 0, 10, 20 — each is isolated, so each run has length 1
    auto features = makeFeatures(3);
    auto labels = makeLabels({0, 1, 0});
    auto times = makeTimes({0, 10, 20});

    auto segments = MLCore::SequenceAssembler::segment(features, labels, times);

    // Default min_sequence_length = 2, all runs are length 1
    REQUIRE(segments.empty());
}

TEST_CASE("SequenceAssembler all gaps with min_length=1 keeps all", "[MLCore][SequenceAssembler]") {
    auto features = makeFeatures(3);
    auto labels = makeLabels({0, 1, 0});
    auto times = makeTimes({0, 10, 20});

    MLCore::SequenceAssemblerConfig config;
    config.min_sequence_length = 1;

    auto segments = MLCore::SequenceAssembler::segment(features, labels, times, config);

    REQUIRE(segments.size() == 3);
    for (auto const & seg: segments) {
        REQUIRE(seg.features.n_cols == 1);
        REQUIRE(seg.labels.n_elem == 1);
        REQUIRE(seg.times.size() == 1);
    }

    // Verify correct time assignment
    REQUIRE(segments[0].times[0].getValue() == 0);
    REQUIRE(segments[1].times[0].getValue() == 10);
    REQUIRE(segments[2].times[0].getValue() == 20);
}

// ============================================================================
// Multiple segments with gaps
// ============================================================================

TEST_CASE("SequenceAssembler two segments with gap", "[MLCore][SequenceAssembler]") {
    // Times: 100,101,102,  200,201,202
    // Gap between 102 and 200
    auto features = makeFeatures(6);
    auto labels = makeLabels({0, 0, 1, 1, 0, 0});
    auto times = makeTimes({100, 101, 102, 200, 201, 202});

    auto segments = MLCore::SequenceAssembler::segment(features, labels, times);

    REQUIRE(segments.size() == 2);

    // First segment: columns 0-2
    REQUIRE(segments[0].features.n_cols == 3);
    REQUIRE(segments[0].times[0].getValue() == 100);
    REQUIRE(segments[0].times[2].getValue() == 102);
    REQUIRE(segments[0].labels(0) == 0);
    REQUIRE(segments[0].labels(2) == 1);

    // Second segment: columns 3-5
    REQUIRE(segments[1].features.n_cols == 3);
    REQUIRE(segments[1].times[0].getValue() == 200);
    REQUIRE(segments[1].times[2].getValue() == 202);
    REQUIRE(segments[1].labels(0) == 1);
    REQUIRE(segments[1].labels(2) == 0);

    // Verify feature values are sub-matrices of the original
    REQUIRE_THAT(segments[0].features(0, 0), WithinAbs(0.0, 1e-12));
    REQUIRE_THAT(segments[0].features(0, 2), WithinAbs(2.0, 1e-12));
    REQUIRE_THAT(segments[1].features(0, 0), WithinAbs(3.0, 1e-12));
    REQUIRE_THAT(segments[1].features(0, 2), WithinAbs(5.0, 1e-12));
}

TEST_CASE("SequenceAssembler three segments mixed lengths", "[MLCore][SequenceAssembler]") {
    // Segment 1: times 0,1  (length 2)
    // Segment 2: time  10   (length 1 — filtered by default)
    // Segment 3: times 50,51,52,53 (length 4)
    auto features = makeFeatures(7);
    auto labels = makeLabels({0, 1, 0, 1, 1, 0, 0});
    auto times = makeTimes({0, 1, 10, 50, 51, 52, 53});

    auto segments = MLCore::SequenceAssembler::segment(features, labels, times);

    // The single-frame segment at time=10 should be filtered out
    REQUIRE(segments.size() == 2);

    REQUIRE(segments[0].features.n_cols == 2);
    REQUIRE(segments[0].times[0].getValue() == 0);
    REQUIRE(segments[0].times[1].getValue() == 1);

    REQUIRE(segments[1].features.n_cols == 4);
    REQUIRE(segments[1].times[0].getValue() == 50);
    REQUIRE(segments[1].times[3].getValue() == 53);
}

// ============================================================================
// min_sequence_length filtering
// ============================================================================

TEST_CASE("SequenceAssembler min_sequence_length filters short segments", "[MLCore][SequenceAssembler]") {
    // Segment 1: 0,1,2 (length 3)
    // Segment 2: 10,11 (length 2)
    // Segment 3: 20,21,22,23 (length 4)
    auto features = makeFeatures(9);
    auto labels = makeLabels({0, 0, 0, 1, 1, 0, 0, 0, 0});
    auto times = makeTimes({0, 1, 2, 10, 11, 20, 21, 22, 23});

    MLCore::SequenceAssemblerConfig config;
    config.min_sequence_length = 3;

    auto segments = MLCore::SequenceAssembler::segment(features, labels, times, config);

    // Segment 2 (length 2) is too short
    REQUIRE(segments.size() == 2);
    REQUIRE(segments[0].features.n_cols == 3);
    REQUIRE(segments[1].features.n_cols == 4);
}

TEST_CASE("SequenceAssembler min_sequence_length=0 keeps all", "[MLCore][SequenceAssembler]") {
    auto features = makeFeatures(3);
    auto labels = makeLabels({0, 1, 0});
    auto times = makeTimes({0, 10, 20});

    MLCore::SequenceAssemblerConfig config;
    config.min_sequence_length = 0;

    auto segments = MLCore::SequenceAssembler::segment(features, labels, times, config);

    REQUIRE(segments.size() == 3);
}

// ============================================================================
// Unlabeled segmentation
// ============================================================================

TEST_CASE("SequenceAssembler unlabeled produces empty labels", "[MLCore][SequenceAssembler]") {
    auto features = makeFeatures(4);
    auto times = makeTimes({0, 1, 10, 11});

    auto segments = MLCore::SequenceAssembler::segment(features, times);

    REQUIRE(segments.size() == 2);

    // Labels should be empty
    REQUIRE(segments[0].labels.n_elem == 0);
    REQUIRE(segments[1].labels.n_elem == 0);

    // Features should still be correctly segmented
    REQUIRE(segments[0].features.n_cols == 2);
    REQUIRE(segments[1].features.n_cols == 2);
}

// ============================================================================
// Feature preservation across segments
// ============================================================================

TEST_CASE("SequenceAssembler preserves multi-row features", "[MLCore][SequenceAssembler]") {
    // 3 features × 4 observations with a gap
    arma::mat features(3, 4);
    features(0, 0) = 1.0;
    features(0, 1) = 2.0;
    features(0, 2) = 3.0;
    features(0, 3) = 4.0;
    features(1, 0) = 10.0;
    features(1, 1) = 20.0;
    features(1, 2) = 30.0;
    features(1, 3) = 40.0;
    features(2, 0) = 100.0;
    features(2, 1) = 200.0;
    features(2, 2) = 300.0;
    features(2, 3) = 400.0;

    auto labels = makeLabels({0, 1, 0, 1});
    auto times = makeTimes({5, 6, 100, 101});

    auto segments = MLCore::SequenceAssembler::segment(features, labels, times);

    REQUIRE(segments.size() == 2);

    // First segment: 3 features × 2 cols
    REQUIRE(segments[0].features.n_rows == 3);
    REQUIRE(segments[0].features.n_cols == 2);
    REQUIRE_THAT(segments[0].features(0, 0), WithinAbs(1.0, 1e-12));
    REQUIRE_THAT(segments[0].features(2, 1), WithinAbs(200.0, 1e-12));

    // Second segment: 3 features × 2 cols
    REQUIRE(segments[1].features.n_rows == 3);
    REQUIRE(segments[1].features.n_cols == 2);
    REQUIRE_THAT(segments[1].features(0, 0), WithinAbs(3.0, 1e-12));
    REQUIRE_THAT(segments[1].features(2, 1), WithinAbs(400.0, 1e-12));
}

// ============================================================================
// Boundary condition: gap of exactly 2
// ============================================================================

TEST_CASE("SequenceAssembler gap of 2 splits correctly", "[MLCore][SequenceAssembler]") {
    // Times 0,1,3,4 — gap between 1 and 3 (diff=2 > 1)
    auto features = makeFeatures(4);
    auto labels = makeLabels({0, 0, 1, 1});
    auto times = makeTimes({0, 1, 3, 4});

    auto segments = MLCore::SequenceAssembler::segment(features, labels, times);

    REQUIRE(segments.size() == 2);
    REQUIRE(segments[0].features.n_cols == 2);
    REQUIRE(segments[1].features.n_cols == 2);
}

TEST_CASE("SequenceAssembler consecutive times with diff=1 are not split", "[MLCore][SequenceAssembler]") {
    // Times 0,1,2,3 — all consecutive, no gaps
    auto features = makeFeatures(4);
    auto labels = makeLabels({0, 1, 0, 1});
    auto times = makeTimes({0, 1, 2, 3});

    auto segments = MLCore::SequenceAssembler::segment(features, labels, times);

    REQUIRE(segments.size() == 1);
    REQUIRE(segments[0].features.n_cols == 4);
}

// ============================================================================
// Large contiguous block
// ============================================================================

TEST_CASE("SequenceAssembler large contiguous block", "[MLCore][SequenceAssembler]") {
    std::size_t const n = 1000;
    arma::mat features(2, n);
    arma::Row<std::size_t> labels(n);
    std::vector<TimeFrameIndex> times;
    times.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        features(0, i) = static_cast<double>(i);
        features(1, i) = static_cast<double>(i) * 2.0;
        labels(i) = i % 2;
        times.emplace_back(static_cast<int64_t>(i));
    }

    auto segments = MLCore::SequenceAssembler::segment(features, labels, times);

    REQUIRE(segments.size() == 1);
    REQUIRE(segments[0].features.n_cols == n);
    REQUIRE(segments[0].labels.n_elem == n);
    REQUIRE(segments[0].times.size() == n);
}

// ============================================================================
// Negative TimeFrameIndex values
// ============================================================================

TEST_CASE("SequenceAssembler handles negative TimeFrameIndex values", "[MLCore][SequenceAssembler]") {
    auto features = makeFeatures(4);
    auto labels = makeLabels({0, 1, 0, 1});
    // Contiguous negative range
    auto times = makeTimes({-2, -1, 0, 1});

    auto segments = MLCore::SequenceAssembler::segment(features, labels, times);

    REQUIRE(segments.size() == 1);
    REQUIRE(segments[0].features.n_cols == 4);
    REQUIRE(segments[0].times[0].getValue() == -2);
    REQUIRE(segments[0].times[3].getValue() == 1);
}

// ============================================================================
// Two-frame sequences (minimum viable HMM input)
// ============================================================================

TEST_CASE("SequenceAssembler two-frame sequence is valid HMM input", "[MLCore][SequenceAssembler]") {
    auto features = makeFeatures(2);
    auto labels = makeLabels({0, 1});
    auto times = makeTimes({50, 51});

    auto segments = MLCore::SequenceAssembler::segment(features, labels, times);

    REQUIRE(segments.size() == 1);
    REQUIRE(segments[0].features.n_cols == 2);
    REQUIRE(segments[0].labels(0) == 0);
    REQUIRE(segments[0].labels(1) == 1);
}
