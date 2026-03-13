/**
 * @file RowDescriptor.test.cpp
 * @brief Unit tests for RowDescriptor
 *
 * Tests the tensor row descriptor, including:
 * - Factory construction (ordinal, time indices, intervals)
 * - Row count queries
 * - Type-specific access and error handling
 * - Row labeling
 * - Equality comparison
 * - Edge cases
 */

#include <catch2/catch_test_macros.hpp>

#include "Tensors/RowDescriptor.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/TimeIndexStorage.hpp"

#include <memory>
#include <variant>
#include <vector>

// =============================================================================
// Helper: create a simple TimeFrame for testing
// =============================================================================

static std::shared_ptr<TimeFrame> makeTestTimeFrame(std::size_t size) {
    std::vector<int> times(size);
    for (std::size_t i = 0; i < size; ++i) {
        times[i] = static_cast<int>(i);
    }
    return std::make_shared<TimeFrame>(times);
}

// =============================================================================
// Ordinal Row Tests
// =============================================================================

TEST_CASE("RowDescriptor ordinal construction", "[RowDescriptor]") {
    auto rd = RowDescriptor::ordinal(50);

    CHECK(rd.type() == RowType::Ordinal);
    CHECK(rd.count() == 50);
    CHECK(rd.timeFrame() == nullptr);
}

TEST_CASE("RowDescriptor ordinal zero rows", "[RowDescriptor]") {
    auto rd = RowDescriptor::ordinal(0);

    CHECK(rd.type() == RowType::Ordinal);
    CHECK(rd.count() == 0);
}

TEST_CASE("RowDescriptor ordinal labels", "[RowDescriptor]") {
    auto rd = RowDescriptor::ordinal(3);

    auto label0 = rd.labelAt(0);
    auto label1 = rd.labelAt(1);
    auto label2 = rd.labelAt(2);

    CHECK(std::holds_alternative<std::size_t>(label0));
    CHECK(std::get<std::size_t>(label0) == 0);
    CHECK(std::get<std::size_t>(label1) == 1);
    CHECK(std::get<std::size_t>(label2) == 2);

    CHECK_THROWS_AS(rd.labelAt(3), std::out_of_range);
}

TEST_CASE("RowDescriptor ordinal rejects time-specific access", "[RowDescriptor]") {
    auto rd = RowDescriptor::ordinal(10);

    CHECK_THROWS_AS(rd.timeStorage(), std::logic_error);
    CHECK_THROWS_AS(rd.timeStoragePtr(), std::logic_error);
    CHECK_THROWS_AS(rd.intervals(), std::logic_error);
}

// =============================================================================
// TimeFrameIndex Row Tests
// =============================================================================

TEST_CASE("RowDescriptor fromTimeIndices construction", "[RowDescriptor]") {
    auto tf = makeTestTimeFrame(1000);
    auto storage = TimeIndexStorageFactory::createDense(TimeFrameIndex{100}, 50);

    auto rd = RowDescriptor::fromTimeIndices(storage, tf);

    CHECK(rd.type() == RowType::TimeFrameIndex);
    CHECK(rd.count() == 50);
    CHECK(rd.timeFrame() == tf);
}

TEST_CASE("RowDescriptor fromTimeIndices time storage access", "[RowDescriptor]") {
    auto tf = makeTestTimeFrame(100);
    auto storage = TimeIndexStorageFactory::createDense(TimeFrameIndex{0}, 10);

    auto rd = RowDescriptor::fromTimeIndices(storage, tf);

    auto const & ts = rd.timeStorage();
    CHECK(ts.size() == 10);
    CHECK(ts.getTimeFrameIndexAt(0) == TimeFrameIndex{0});
    CHECK(ts.getTimeFrameIndexAt(9) == TimeFrameIndex{9});

    // shared_ptr access
    auto ptr = rd.timeStoragePtr();
    CHECK(ptr != nullptr);
    CHECK(ptr->size() == 10);
}

TEST_CASE("RowDescriptor fromTimeIndices labels", "[RowDescriptor]") {
    auto tf = makeTestTimeFrame(100);
    auto storage = TimeIndexStorageFactory::createDense(TimeFrameIndex{10}, 5);

    auto rd = RowDescriptor::fromTimeIndices(storage, tf);

    auto label0 = rd.labelAt(0);
    auto label2 = rd.labelAt(2);

    CHECK(std::holds_alternative<TimeFrameIndex>(label0));
    CHECK(std::get<TimeFrameIndex>(label0) == TimeFrameIndex{10});
    CHECK(std::get<TimeFrameIndex>(label2) == TimeFrameIndex{12});

    CHECK_THROWS_AS(rd.labelAt(5), std::out_of_range);
}

TEST_CASE("RowDescriptor fromTimeIndices with sparse storage", "[RowDescriptor]") {
    auto tf = makeTestTimeFrame(1000);
    auto storage = TimeIndexStorageFactory::createFromTimeIndices({
        TimeFrameIndex{5}, TimeFrameIndex{20}, TimeFrameIndex{100}
    });

    auto rd = RowDescriptor::fromTimeIndices(storage, tf);

    CHECK(rd.count() == 3);
    CHECK(std::get<TimeFrameIndex>(rd.labelAt(0)) == TimeFrameIndex{5});
    CHECK(std::get<TimeFrameIndex>(rd.labelAt(1)) == TimeFrameIndex{20});
    CHECK(std::get<TimeFrameIndex>(rd.labelAt(2)) == TimeFrameIndex{100});
}

TEST_CASE("RowDescriptor fromTimeIndices null arguments", "[RowDescriptor]") {
    auto tf = makeTestTimeFrame(100);
    auto storage = TimeIndexStorageFactory::createDenseFromZero(10);

    CHECK_THROWS_AS(
        RowDescriptor::fromTimeIndices(nullptr, tf),
        std::invalid_argument);
    CHECK_THROWS_AS(
        RowDescriptor::fromTimeIndices(storage, nullptr),
        std::invalid_argument);
}

TEST_CASE("RowDescriptor fromTimeIndices rejects interval access", "[RowDescriptor]") {
    auto tf = makeTestTimeFrame(100);
    auto storage = TimeIndexStorageFactory::createDenseFromZero(10);

    auto rd = RowDescriptor::fromTimeIndices(storage, tf);
    CHECK_THROWS_AS(rd.intervals(), std::logic_error);
}

// =============================================================================
// Interval Row Tests
// =============================================================================

TEST_CASE("RowDescriptor fromIntervals construction", "[RowDescriptor]") {
    auto tf = makeTestTimeFrame(1000);
    std::vector<TimeFrameInterval> intervals{
        {TimeFrameIndex{0}, TimeFrameIndex{99}},
        {TimeFrameIndex{100}, TimeFrameIndex{199}},
        {TimeFrameIndex{200}, TimeFrameIndex{299}}
    };

    auto rd = RowDescriptor::fromIntervals(intervals, tf);

    CHECK(rd.type() == RowType::Interval);
    CHECK(rd.count() == 3);
    CHECK(rd.timeFrame() == tf);
}

TEST_CASE("RowDescriptor fromIntervals access", "[RowDescriptor]") {
    auto tf = makeTestTimeFrame(1000);
    std::vector<TimeFrameInterval> intervals{
        {TimeFrameIndex{0}, TimeFrameIndex{49}},
        {TimeFrameIndex{50}, TimeFrameIndex{99}}
    };

    auto rd = RowDescriptor::fromIntervals(intervals, tf);

    auto span = rd.intervals();
    REQUIRE(span.size() == 2);
    CHECK(span[0].start == TimeFrameIndex{0});
    CHECK(span[0].end == TimeFrameIndex{49});
    CHECK(span[1].start == TimeFrameIndex{50});
    CHECK(span[1].end == TimeFrameIndex{99});
}

TEST_CASE("RowDescriptor fromIntervals labels", "[RowDescriptor]") {
    auto tf = makeTestTimeFrame(1000);
    std::vector<TimeFrameInterval> intervals{
        {TimeFrameIndex{10}, TimeFrameIndex{20}},
        {TimeFrameIndex{30}, TimeFrameIndex{40}}
    };

    auto rd = RowDescriptor::fromIntervals(intervals, tf);

    auto label0 = rd.labelAt(0);
    CHECK(std::holds_alternative<TimeFrameInterval>(label0));
    auto interval = std::get<TimeFrameInterval>(label0);
    CHECK(interval.start == TimeFrameIndex{10});
    CHECK(interval.end == TimeFrameIndex{20});

    CHECK_THROWS_AS(rd.labelAt(2), std::out_of_range);
}

TEST_CASE("RowDescriptor fromIntervals empty", "[RowDescriptor]") {
    auto tf = makeTestTimeFrame(100);
    auto rd = RowDescriptor::fromIntervals({}, tf);

    CHECK(rd.count() == 0);
    CHECK(rd.intervals().empty());
}

TEST_CASE("RowDescriptor fromIntervals null time_frame", "[RowDescriptor]") {
    CHECK_THROWS_AS(
        RowDescriptor::fromIntervals({{TimeFrameIndex{0}, TimeFrameIndex{10}}}, nullptr),
        std::invalid_argument);
}

TEST_CASE("RowDescriptor fromIntervals rejects time storage access", "[RowDescriptor]") {
    auto tf = makeTestTimeFrame(100);
    auto rd = RowDescriptor::fromIntervals(
        {{TimeFrameIndex{0}, TimeFrameIndex{10}}}, tf);

    CHECK_THROWS_AS(rd.timeStorage(), std::logic_error);
    CHECK_THROWS_AS(rd.timeStoragePtr(), std::logic_error);
}

// =============================================================================
// Equality Tests
// =============================================================================

TEST_CASE("RowDescriptor ordinal equality", "[RowDescriptor]") {
    auto a = RowDescriptor::ordinal(10);
    auto b = RowDescriptor::ordinal(10);
    auto c = RowDescriptor::ordinal(20);

    CHECK(a == b);
    CHECK(a != c);
}

TEST_CASE("RowDescriptor different types are not equal", "[RowDescriptor]") {
    auto tf = makeTestTimeFrame(100);
    auto ordinal = RowDescriptor::ordinal(10);
    auto time_based = RowDescriptor::fromTimeIndices(
        TimeIndexStorageFactory::createDenseFromZero(10), tf);

    CHECK(ordinal != time_based);
}

TEST_CASE("RowDescriptor time index equality", "[RowDescriptor]") {
    auto tf = makeTestTimeFrame(100);
    auto a = RowDescriptor::fromTimeIndices(
        TimeIndexStorageFactory::createDense(TimeFrameIndex{0}, 10), tf);
    auto b = RowDescriptor::fromTimeIndices(
        TimeIndexStorageFactory::createDense(TimeFrameIndex{0}, 10), tf);
    auto c = RowDescriptor::fromTimeIndices(
        TimeIndexStorageFactory::createDense(TimeFrameIndex{5}, 10), tf);

    CHECK(a == b);
    CHECK(a != c);
}

TEST_CASE("RowDescriptor interval equality", "[RowDescriptor]") {
    auto tf = makeTestTimeFrame(100);
    std::vector<TimeFrameInterval> intervals1{
        {TimeFrameIndex{0}, TimeFrameIndex{10}}
    };
    std::vector<TimeFrameInterval> intervals2{
        {TimeFrameIndex{0}, TimeFrameIndex{10}}
    };
    std::vector<TimeFrameInterval> intervals3{
        {TimeFrameIndex{0}, TimeFrameIndex{20}}
    };

    auto a = RowDescriptor::fromIntervals(intervals1, tf);
    auto b = RowDescriptor::fromIntervals(intervals2, tf);
    auto c = RowDescriptor::fromIntervals(intervals3, tf);

    CHECK(a == b);
    CHECK(a != c);
}
