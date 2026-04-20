/**
 * @file TimeController.test.cpp
 * @brief Unit tests for the Qt-free TimeController library
 */

#include "TimeController/TimeController.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>

TEST_CASE("TimeController setCurrentTime updates currentPosition", "[TimeController]") {
    TimeController tc;
    auto tf = std::make_shared<TimeFrame>();
    TimePosition const pos{TimeFrameIndex(7), tf};
    tc.setCurrentTime(pos);
    REQUIRE(tc.currentPosition() == pos);
    REQUIRE(tc.currentTimeIndex() == TimeFrameIndex(7));
    REQUIRE(tc.currentTimeFrame() == tf);
}

TEST_CASE("TimeController setCurrentTime invokes callback", "[TimeController]") {
    TimeController tc;
    auto tf = std::make_shared<TimeFrame>();
    TimePosition last{};
    int count = 0;
    tc.setOnTimeChanged([&](TimePosition const & p) {
        ++count;
        last = p;
    });
    TimePosition const pos{TimeFrameIndex(3), tf};
    tc.setCurrentTime(pos);
    REQUIRE(count == 1);
    REQUIRE(last == pos);
}

TEST_CASE("TimeController re-entrancy guard ignores nested setCurrentTime", "[TimeController]") {
    TimeController tc;
    auto tf = std::make_shared<TimeFrame>();
    int count = 0;
    tc.setOnTimeChanged([&](TimePosition const &) {
        ++count;
        tc.setCurrentTime(TimePosition{TimeFrameIndex(99), tf});
    });
    tc.setCurrentTime(TimePosition{TimeFrameIndex(1), tf});
    REQUIRE(count == 1);
    REQUIRE(tc.currentTimeIndex() == TimeFrameIndex(1));
}

TEST_CASE("TimeController equality short-circuit skips callback", "[TimeController]") {
    TimeController tc;
    auto tf = std::make_shared<TimeFrame>();
    TimePosition const pos{TimeFrameIndex(2), tf};
    int count = 0;
    tc.setOnTimeChanged([&](TimePosition const &) { ++count; });
    tc.setCurrentTime(pos);
    tc.setCurrentTime(pos);
    REQUIRE(count == 1);
}

TEST_CASE("TimeController setActiveTimeKey invokes callback when key changes", "[TimeController]") {
    TimeController tc;
    TimeKey const old_default("time");
    TimeKey const next("camera");
    REQUIRE(tc.activeTimeKey() == old_default);

    int count = 0;
    TimeKey seen_new;
    TimeKey seen_old;
    tc.setOnTimeKeyChanged([&](TimeKey nk, TimeKey ok) {
        ++count;
        seen_new = nk;
        seen_old = ok;
    });

    tc.setActiveTimeKey(next);
    REQUIRE(count == 1);
    REQUIRE(seen_new == next);
    REQUIRE(seen_old == old_default);
    REQUIRE(tc.activeTimeKey() == next);

    tc.setActiveTimeKey(next);
    REQUIRE(count == 1);
}

TEST_CASE("TimeController setCurrentTime overload delegates", "[TimeController]") {
    TimeController tc;
    auto tf = std::make_shared<TimeFrame>();
    tc.setCurrentTime(TimeFrameIndex(5), tf);
    REQUIRE(tc.currentTimeIndex() == TimeFrameIndex(5));
    REQUIRE(tc.currentTimeFrame() == tf);
}
