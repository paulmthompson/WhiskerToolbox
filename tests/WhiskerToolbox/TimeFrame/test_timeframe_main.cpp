#include <catch2/catch_test_macros.hpp>

// This file serves as the main entry point for TimeFrame tests
// The actual test cases are included from StrongTimeTypes.test.cpp
// This allows us to have an independent test driver for the TimeFrame library

TEST_CASE("TimeFrame Library Test Runner", "[timeframe]") {
    // This test case serves as a basic check that the test framework is working
    REQUIRE(true);
}
