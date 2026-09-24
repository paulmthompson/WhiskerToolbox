/**
 * @file ragged_out_of_order_append.test.cpp
 * @brief Regression tests for OwningRaggedStorage time-range corruption when
 *        entries are appended out of chronological order.
 *
 * Reproduces the NeuroSAM batch-inference workflow: a memory-frame label is
 * written at frame 50 before batch predictions are appended in ascending order
 * for frames 1–100. Media_Widget reads via getAtTime() and shows spurious masks;
 * MaskTableModel reads via flattened_data() and shows the true per-entry frames.
 */

#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"
#include "RaggedTimeSeries/RaggedStorage.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "fixtures/RaggedTestTraits.hpp"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <ranges>

namespace {

/**
 * @brief Count storage entries whose time equals @p time within getTimeRange.
 *
 * A correct range contains only indices whose stored time matches @p time.
 */
template<typename TData>
std::size_t countMatchingTimesInRange(
        OwningRaggedStorage<TData> const & storage,
        TimeFrameIndex time) {
    auto const [start, end] = storage.getTimeRange(time);
    std::size_t count = 0;
    for (std::size_t idx = start; idx < end; ++idx) {
        if (storage.getTime(idx) == time) {
            ++count;
        }
    }
    return count;
}

/**
 * @brief Count entries at @p time by scanning flattened storage order.
 */
template<typename Series>
std::size_t countFlattenedEntriesAtTime(Series const & series, TimeFrameIndex time) {
    std::size_t count = 0;
    for (auto const & [entry_time, entity_id, data_ref]: series.flattened_data()) {
        (void) entity_id;
        (void) data_ref;
        if (entry_time == time) {
            ++count;
        }
    }
    return count;
}

struct InclusiveFrameRange {
    int start_frame;
    int end_frame;
};

/**
 * @brief Simulate batch append of one element per frame in ascending order.
 */
template<typename Traits>
void appendBatchFramesAscending(
        typename Traits::DataType & data,
        InclusiveFrameRange frame_range) {
    for (int frame = frame_range.start_frame; frame <= frame_range.end_frame; ++frame) {
        Traits::add(data, TimeFrameIndex(frame), Traits::sample2(), NotifyObservers::No);
    }
}

}// namespace

// =============================================================================
// OwningRaggedStorage (low-level)
// =============================================================================

TEST_CASE("OwningRaggedStorage - out-of-order append preserves getTimeRange",
          "[RaggedStorage][ragged][out_of_order][regression]") {
    OwningRaggedStorage<Point2D<float>> storage;

    SECTION("Minimal repro: future frame, intermediate frame, future frame again") {
        storage.append(TimeFrameIndex{50}, Point2D<float>{50.0f, 0.0f}, EntityId{1});
        storage.append(TimeFrameIndex{10}, Point2D<float>{10.0f, 0.0f}, EntityId{2});
        storage.append(TimeFrameIndex{50}, Point2D<float>{50.1f, 0.0f}, EntityId{3});

        REQUIRE(storage.size() == 3);

        auto const [range_start, range_end] = storage.getTimeRange(TimeFrameIndex{50});
        REQUIRE(range_end - range_start == 2);
        REQUIRE(countMatchingTimesInRange(storage, TimeFrameIndex{50}) == 2);

        for (std::size_t idx = range_start; idx < range_end; ++idx) {
            REQUIRE(storage.getTime(idx) == TimeFrameIndex{50});
        }

        auto const [mid_start, mid_end] = storage.getTimeRange(TimeFrameIndex{10});
        REQUIRE(mid_end - mid_start == 1);
        REQUIRE(storage.getTime(mid_start) == TimeFrameIndex{10});
    }

    SECTION("NeuroSAM workflow: label frame 50 then batch-write frames 1-100") {
        storage.append(TimeFrameIndex{50}, Point2D<float>{0.0f, 0.0f}, EntityId{0});

        for (int frame = 1; frame <= 100; ++frame) {
            storage.append(
                    TimeFrameIndex{frame},
                    Point2D<float>{static_cast<float>(frame), 0.0f},
                    EntityId{static_cast<std::uint64_t>(frame)});
        }

        REQUIRE(storage.size() == 101);

        auto const [range_start, range_end] = storage.getTimeRange(TimeFrameIndex{50});
        REQUIRE(range_end - range_start == 2);
        REQUIRE(countMatchingTimesInRange(storage, TimeFrameIndex{50}) == 2);

        auto const [mid_start, mid_end] = storage.getTimeRange(TimeFrameIndex{25});
        REQUIRE(mid_end - mid_start == 1);
        REQUIRE(storage.getTime(mid_start) == TimeFrameIndex{25});
    }
}

// =============================================================================
// RaggedTimeSeries-derived types (MaskData, LineData, PointData)
// =============================================================================

TEMPLATE_TEST_CASE("RaggedTimeSeries - out-of-order append preserves getAtTime counts",
                   "[ragged][out_of_order][regression]",
                   LineData, MaskData, PointData) {

    using Traits = RaggedTestTraits<TestType>;
    TestType data;

    SECTION("Minimal repro: future frame, intermediate frame, future frame again") {
        Traits::add(data, TimeFrameIndex(50), Traits::sample1(), NotifyObservers::No);
        Traits::add(data, TimeFrameIndex(10), Traits::sample2(), NotifyObservers::No);
        Traits::add(data, TimeFrameIndex(50), Traits::sample3(), NotifyObservers::No);

        REQUIRE(data.getTotalEntryCount() == 3);
        REQUIRE(std::ranges::distance(data.getAtTime(TimeFrameIndex(50))) == 2);
        REQUIRE(std::ranges::distance(data.getAtTime(TimeFrameIndex(10))) == 1);
        REQUIRE(countFlattenedEntriesAtTime(data, TimeFrameIndex(50)) == 2);
    }

    SECTION("NeuroSAM workflow: label frame 50 then batch-write frames 1-100") {
        Traits::add(data, TimeFrameIndex(50), Traits::sample1(), NotifyObservers::No);
        appendBatchFramesAscending<Traits>(data, InclusiveFrameRange{1, 100});

        REQUIRE(data.getTotalEntryCount() == 101);
        REQUIRE(std::ranges::distance(data.getAtTime(TimeFrameIndex(50))) == 2);
        REQUIRE(std::ranges::distance(data.getAtTime(TimeFrameIndex(1))) == 1);
        REQUIRE(std::ranges::distance(data.getAtTime(TimeFrameIndex(49))) == 1);
        REQUIRE(std::ranges::distance(data.getAtTime(TimeFrameIndex(100))) == 1);
        REQUIRE(countFlattenedEntriesAtTime(data, TimeFrameIndex(50)) == 2);
    }
}

TEMPLATE_TEST_CASE(
        "RaggedTimeSeries - getAtTime range contains only matching frame times",
        "[ragged][out_of_order][regression][invariant]",
        LineData, MaskData, PointData) {

    using Traits = RaggedTestTraits<TestType>;
    TestType data;

    Traits::add(data, TimeFrameIndex(50), Traits::sample1(), NotifyObservers::No);
    appendBatchFramesAscending<Traits>(data, InclusiveFrameRange{1, 100});

    for (auto const time: data.getTimesWithData()) {
        std::size_t const range_count = std::ranges::distance(data.getAtTime(time));
        std::size_t const flattened_count = countFlattenedEntriesAtTime(data, time);

        INFO("frame = " << time.getValue()
                        << ", getAtTime count = " << range_count
                        << ", flattened count = " << flattened_count);
        REQUIRE(range_count == flattened_count);
    }
}
