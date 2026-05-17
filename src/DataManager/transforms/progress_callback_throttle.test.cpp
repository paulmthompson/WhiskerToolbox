#include "catch2/catch_test_macros.hpp"

#include "transforms/data_transforms.hpp"

#include <vector>

TEST_CASE("throttleProgressCallbackToWholePercents coalesces duplicate percents", "[transforms][progress]") {
    std::vector<int> seen;
    auto inner = [&seen](int p) { seen.push_back(p); };
    auto throttled = throttleProgressCallbackToWholePercents(inner);
    for (int i = 0; i < 100; ++i) {
        throttled(42);
    }
    throttled(43);
    REQUIRE(seen.size() == 2);
    REQUIRE(seen[0] == 42);
    REQUIRE(seen[1] == 43);
}

TEST_CASE("throttleProgressCallbackToWholePercents forwards each integer percent once", "[transforms][progress]") {
    std::vector<int> seen;
    auto inner = [&seen](int p) { seen.push_back(p); };
    auto throttled = throttleProgressCallbackToWholePercents(inner);
    for (int p = 0; p <= 100; ++p) {
        throttled(p);
    }
    std::vector<int> expected;
    expected.reserve(101U);
    for (int i = 0; i <= 100; ++i) {
        expected.push_back(i);
    }
    REQUIRE(seen == expected);
}

TEST_CASE("throttleProgressCallbackToWholePercents clamps to 0-100", "[transforms][progress]") {
    std::vector<int> seen;
    auto inner = [&seen](int p) { seen.push_back(p); };
    auto throttled = throttleProgressCallbackToWholePercents(inner);
    throttled(-10);
    throttled(200);
    REQUIRE(seen.size() == 2);
    REQUIRE(seen[0] == 0);
    REQUIRE(seen[1] == 100);
}

TEST_CASE("throttleProgressCallbackToWholePercents returns empty when inner empty", "[transforms][progress]") {
    ProgressCallback empty;
    auto throttled = throttleProgressCallbackToWholePercents(empty);
    REQUIRE_FALSE(static_cast<bool>(throttled));
}
