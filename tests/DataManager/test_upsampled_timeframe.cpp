/**
 * @file test_upsampled_timeframe.cpp
 * @brief Tests for createUpsampledTimeFrame() derived TimeFrame utility
 */

#include <catch2/catch_test_macros.hpp>

#include "DataManager/utils/DerivedTimeFrame.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <memory>
#include <vector>

TEST_CASE("createUpsampledTimeFrame basic upsampling", "[timeframe][upsampling]") {

    SECTION("Factor 4 with evenly spaced source") {
        // Source: [0, 60, 120], factor = 4
        // Expected: [0, 15, 30, 45, 60, 75, 90, 105, 120]
        auto source = std::make_shared<TimeFrame>(std::vector<int>{0, 60, 120});

        DerivedTimeFrameFromUpsamplingOptions opts;
        opts.source_timeframe = source;
        opts.upsampling_factor = 4;

        auto result = createUpsampledTimeFrame(opts);
        REQUIRE(result != nullptr);
        REQUIRE(result->getTotalFrameCount() == 9);

        CHECK(result->getTimeAtIndex(TimeFrameIndex(0)) == 0);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(1)) == 15);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(2)) == 30);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(3)) == 45);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(4)) == 60);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(5)) == 75);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(6)) == 90);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(7)) == 105);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(8)) == 120);
    }

    SECTION("Factor 2 with two entries") {
        // Source: [100, 200], factor = 2
        // Expected: [100, 150, 200]
        auto source = std::make_shared<TimeFrame>(std::vector<int>{100, 200});

        DerivedTimeFrameFromUpsamplingOptions opts;
        opts.source_timeframe = source;
        opts.upsampling_factor = 2;

        auto result = createUpsampledTimeFrame(opts);
        REQUIRE(result != nullptr);
        REQUIRE(result->getTotalFrameCount() == 3);

        CHECK(result->getTimeAtIndex(TimeFrameIndex(0)) == 100);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(1)) == 150);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(2)) == 200);
    }

    SECTION("Original time points are preserved at correct indices") {
        // Source: [10, 70, 130, 190], factor = 3
        // Output size: (4-1)*3 + 1 = 10
        // Original values appear at indices 0, 3, 6, 9
        auto source = std::make_shared<TimeFrame>(std::vector<int>{10, 70, 130, 190});

        DerivedTimeFrameFromUpsamplingOptions opts;
        opts.source_timeframe = source;
        opts.upsampling_factor = 3;

        auto result = createUpsampledTimeFrame(opts);
        REQUIRE(result != nullptr);
        REQUIRE(result->getTotalFrameCount() == 10);

        CHECK(result->getTimeAtIndex(TimeFrameIndex(0)) == 10);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(3)) == 70);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(6)) == 130);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(9)) == 190);
    }
}

TEST_CASE("createUpsampledTimeFrame rounding behavior", "[timeframe][upsampling]") {

    SECTION("Non-divisor factor rounds to nearest tick") {
        // Source: [0, 100], factor = 3
        // Interpolated: 0, 33.333..., 66.666..., 100
        // Rounded: [0, 33, 67, 100]
        auto source = std::make_shared<TimeFrame>(std::vector<int>{0, 100});

        DerivedTimeFrameFromUpsamplingOptions opts;
        opts.source_timeframe = source;
        opts.upsampling_factor = 3;

        auto result = createUpsampledTimeFrame(opts);
        REQUIRE(result != nullptr);
        REQUIRE(result->getTotalFrameCount() == 4);

        CHECK(result->getTimeAtIndex(TimeFrameIndex(0)) == 0);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(1)) == 33);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(2)) == 67);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(3)) == 100);
    }
}

TEST_CASE("createUpsampledTimeFrame identity factor", "[timeframe][upsampling]") {

    SECTION("Factor 1 returns copy of source") {
        auto source = std::make_shared<TimeFrame>(std::vector<int>{0, 60, 120});

        DerivedTimeFrameFromUpsamplingOptions opts;
        opts.source_timeframe = source;
        opts.upsampling_factor = 1;

        auto result = createUpsampledTimeFrame(opts);
        REQUIRE(result != nullptr);
        REQUIRE(result->getTotalFrameCount() == 3);

        CHECK(result->getTimeAtIndex(TimeFrameIndex(0)) == 0);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(1)) == 60);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(2)) == 120);
    }
}

TEST_CASE("createUpsampledTimeFrame edge cases", "[timeframe][upsampling]") {

    SECTION("Empty source returns empty output") {
        auto source = std::make_shared<TimeFrame>(std::vector<int>{});

        DerivedTimeFrameFromUpsamplingOptions opts;
        opts.source_timeframe = source;
        opts.upsampling_factor = 4;

        auto result = createUpsampledTimeFrame(opts);
        REQUIRE(result != nullptr);
        REQUIRE(result->getTotalFrameCount() == 0);
    }

    SECTION("Single entry source returns single entry") {
        auto source = std::make_shared<TimeFrame>(std::vector<int>{42});

        DerivedTimeFrameFromUpsamplingOptions opts;
        opts.source_timeframe = source;
        opts.upsampling_factor = 4;

        auto result = createUpsampledTimeFrame(opts);
        REQUIRE(result != nullptr);
        REQUIRE(result->getTotalFrameCount() == 1);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(0)) == 42);
    }

    SECTION("Null source returns nullptr") {
        DerivedTimeFrameFromUpsamplingOptions opts;
        opts.source_timeframe = nullptr;
        opts.upsampling_factor = 4;

        auto result = createUpsampledTimeFrame(opts);
        REQUIRE(result == nullptr);
    }

    SECTION("Factor <= 0 returns nullptr") {
        auto source = std::make_shared<TimeFrame>(std::vector<int>{0, 60, 120});

        DerivedTimeFrameFromUpsamplingOptions opts;
        opts.source_timeframe = source;
        opts.upsampling_factor = 0;

        auto result = createUpsampledTimeFrame(opts);
        REQUIRE(result == nullptr);

        opts.upsampling_factor = -1;
        result = createUpsampledTimeFrame(opts);
        REQUIRE(result == nullptr);
    }
}

TEST_CASE("createUpsampledTimeFrame output size formula", "[timeframe][upsampling]") {

    SECTION("Output size is (N-1)*factor + 1") {
        std::vector<int> const times = {0, 10, 20, 30, 40};
        auto source = std::make_shared<TimeFrame>(times);

        for (int const factor: {1, 2, 3, 4, 5, 10}) {
            DerivedTimeFrameFromUpsamplingOptions opts;
            opts.source_timeframe = source;
            opts.upsampling_factor = factor;

            auto result = createUpsampledTimeFrame(opts);
            REQUIRE(result != nullptr);

            int const expected_size = (static_cast<int>(times.size()) - 1) * factor + 1;
            CHECK(result->getTotalFrameCount() == expected_size);
        }
    }
}

TEST_CASE("createUpsampledTimeFrame realistic scenario", "[timeframe][upsampling]") {

    SECTION("500Hz camera on 30kHz master clock upsampled 4x") {
        // Camera samples every 60 ticks on a 30kHz clock (= 500Hz)
        // 5 camera frames at ticks: 0, 60, 120, 180, 240
        // Upsampling by 4 → 17 entries at ticks: 0, 15, 30, 45, 60, ..., 240
        std::vector<int> const camera_times = {0, 60, 120, 180, 240};
        auto source = std::make_shared<TimeFrame>(camera_times);

        DerivedTimeFrameFromUpsamplingOptions opts;
        opts.source_timeframe = source;
        opts.upsampling_factor = 4;

        auto result = createUpsampledTimeFrame(opts);
        REQUIRE(result != nullptr);
        REQUIRE(result->getTotalFrameCount() == 17);

        // Check first and last
        CHECK(result->getTimeAtIndex(TimeFrameIndex(0)) == 0);
        CHECK(result->getTimeAtIndex(TimeFrameIndex(16)) == 240);

        // Check spacing is uniform (15 ticks between each)
        for (int i = 0; i < 16; ++i) {
            int const t_curr = result->getTimeAtIndex(TimeFrameIndex(i));
            int const t_next = result->getTimeAtIndex(TimeFrameIndex(i + 1));
            CHECK(t_next - t_curr == 15);
        }
    }
}
