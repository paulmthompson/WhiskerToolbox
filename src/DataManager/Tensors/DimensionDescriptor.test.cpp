/**
 * @file DimensionDescriptor.test.cpp
 * @brief Unit tests for DimensionDescriptor
 *
 * Tests the tensor dimension descriptor, including:
 * - Construction and validation
 * - Shape, strides, and flat index computation
 * - Named axis lookup
 * - Named column support
 * - Dimensionality predicates
 * - Edge cases and error handling
 */

#include <catch2/catch_test_macros.hpp>

#include "Tensors/DimensionDescriptor.hpp"

#include <string>
#include <vector>

// =============================================================================
// Construction Tests
// =============================================================================

TEST_CASE("DimensionDescriptor default constructor creates scalar", "[DimensionDescriptor]") {
    DimensionDescriptor dd;

    CHECK(dd.ndim() == 0);
    CHECK(dd.totalElements() == 1);
    CHECK(dd.shape().empty());
    CHECK(dd.strides().empty());
    CHECK_FALSE(dd.hasColumnNames());
}

TEST_CASE("DimensionDescriptor 1D construction", "[DimensionDescriptor]") {
    DimensionDescriptor dd({AxisDescriptor{"time", 100}});

    CHECK(dd.ndim() == 1);
    CHECK(dd.totalElements() == 100);
    CHECK(dd.is1D());
    CHECK_FALSE(dd.is2D());
    CHECK_FALSE(dd.is3D());
    CHECK(dd.isAtLeast(1));
    CHECK_FALSE(dd.isAtLeast(2));

    auto s = dd.shape();
    REQUIRE(s.size() == 1);
    CHECK(s[0] == 100);

    auto const & strides = dd.strides();
    REQUIRE(strides.size() == 1);
    CHECK(strides[0] == 1);
}

TEST_CASE("DimensionDescriptor 2D construction", "[DimensionDescriptor]") {
    DimensionDescriptor dd({
        AxisDescriptor{"time", 50},
        AxisDescriptor{"channel", 4}
    });

    CHECK(dd.ndim() == 2);
    CHECK(dd.totalElements() == 200);
    CHECK(dd.is2D());
    CHECK(dd.isAtLeast(1));
    CHECK(dd.isAtLeast(2));
    CHECK_FALSE(dd.isAtLeast(3));

    auto s = dd.shape();
    REQUIRE(s.size() == 2);
    CHECK(s[0] == 50);
    CHECK(s[1] == 4);

    // Row-major strides: stride[0] = 4, stride[1] = 1
    auto const & strides = dd.strides();
    REQUIRE(strides.size() == 2);
    CHECK(strides[0] == 4);
    CHECK(strides[1] == 1);
}

TEST_CASE("DimensionDescriptor 3D construction", "[DimensionDescriptor]") {
    DimensionDescriptor dd({
        AxisDescriptor{"batch", 2},
        AxisDescriptor{"height", 3},
        AxisDescriptor{"width", 4}
    });

    CHECK(dd.ndim() == 3);
    CHECK(dd.totalElements() == 24);
    CHECK(dd.is3D());

    // Row-major strides: [3*4, 4, 1] = [12, 4, 1]
    auto const & strides = dd.strides();
    REQUIRE(strides.size() == 3);
    CHECK(strides[0] == 12);
    CHECK(strides[1] == 4);
    CHECK(strides[2] == 1);
}

TEST_CASE("DimensionDescriptor 4D construction (ND)", "[DimensionDescriptor]") {
    DimensionDescriptor dd({
        AxisDescriptor{"batch", 2},
        AxisDescriptor{"channel", 3},
        AxisDescriptor{"height", 4},
        AxisDescriptor{"width", 5}
    });

    CHECK(dd.ndim() == 4);
    CHECK(dd.totalElements() == 120);
    CHECK(dd.isAtLeast(4));

    auto const & strides = dd.strides();
    REQUIRE(strides.size() == 4);
    CHECK(strides[0] == 60);  // 3*4*5
    CHECK(strides[1] == 20);  // 4*5
    CHECK(strides[2] == 5);   // 5
    CHECK(strides[3] == 1);
}

// =============================================================================
// Axis Lookup Tests
// =============================================================================

TEST_CASE("DimensionDescriptor axis access by index", "[DimensionDescriptor]") {
    DimensionDescriptor dd({
        AxisDescriptor{"time", 100},
        AxisDescriptor{"channel", 8}
    });

    CHECK(dd.axis(0).name == "time");
    CHECK(dd.axis(0).size == 100);
    CHECK(dd.axis(1).name == "channel");
    CHECK(dd.axis(1).size == 8);

    CHECK_THROWS_AS(dd.axis(2), std::out_of_range);
}

TEST_CASE("DimensionDescriptor findAxis by name", "[DimensionDescriptor]") {
    DimensionDescriptor dd({
        AxisDescriptor{"time", 100},
        AxisDescriptor{"frequency", 64},
        AxisDescriptor{"channel", 8}
    });

    CHECK(dd.findAxis("time") == 0);
    CHECK(dd.findAxis("frequency") == 1);
    CHECK(dd.findAxis("channel") == 2);
    CHECK_FALSE(dd.findAxis("nonexistent").has_value());
}

// =============================================================================
// Flat Index Tests
// =============================================================================

TEST_CASE("DimensionDescriptor flatIndex for 2D", "[DimensionDescriptor]") {
    DimensionDescriptor dd({
        AxisDescriptor{"row", 3},
        AxisDescriptor{"col", 4}
    });

    // Row-major: flat = row * 4 + col
    CHECK(dd.flatIndex({0, 0}) == 0);
    CHECK(dd.flatIndex({0, 1}) == 1);
    CHECK(dd.flatIndex({0, 3}) == 3);
    CHECK(dd.flatIndex({1, 0}) == 4);
    CHECK(dd.flatIndex({2, 3}) == 11);
}

TEST_CASE("DimensionDescriptor flatIndex for 3D", "[DimensionDescriptor]") {
    DimensionDescriptor dd({
        AxisDescriptor{"a", 2},
        AxisDescriptor{"b", 3},
        AxisDescriptor{"c", 4}
    });

    // flat = a*12 + b*4 + c
    CHECK(dd.flatIndex({0, 0, 0}) == 0);
    CHECK(dd.flatIndex({0, 0, 1}) == 1);
    CHECK(dd.flatIndex({0, 1, 0}) == 4);
    CHECK(dd.flatIndex({1, 0, 0}) == 12);
    CHECK(dd.flatIndex({1, 2, 3}) == 23);
}

TEST_CASE("DimensionDescriptor flatIndex error cases", "[DimensionDescriptor]") {
    DimensionDescriptor dd({
        AxisDescriptor{"row", 3},
        AxisDescriptor{"col", 4}
    });

    // Wrong number of indices
    CHECK_THROWS_AS(dd.flatIndex({0}), std::invalid_argument);
    CHECK_THROWS_AS(dd.flatIndex({0, 0, 0}), std::invalid_argument);

    // Out of bounds
    CHECK_THROWS_AS(dd.flatIndex({3, 0}), std::out_of_range);
    CHECK_THROWS_AS(dd.flatIndex({0, 4}), std::out_of_range);
}

TEST_CASE("DimensionDescriptor flatIndex for scalar", "[DimensionDescriptor]") {
    DimensionDescriptor dd;

    // Scalar: no indices needed
    CHECK(dd.flatIndex({}) == 0);

    // Any indices should fail
    CHECK_THROWS_AS(dd.flatIndex({0}), std::invalid_argument);
}

// =============================================================================
// Column Names Tests
// =============================================================================

TEST_CASE("DimensionDescriptor column names", "[DimensionDescriptor]") {
    DimensionDescriptor dd({
        AxisDescriptor{"time", 100},
        AxisDescriptor{"channel", 3}
    });

    SECTION("No column names by default") {
        CHECK_FALSE(dd.hasColumnNames());
        CHECK(dd.columnNames().empty());
        CHECK_FALSE(dd.findColumn("anything").has_value());
    }

    SECTION("Set and find column names") {
        dd.setColumnNames({"magnitude", "phase", "frequency"});

        CHECK(dd.hasColumnNames());
        CHECK(dd.columnNames().size() == 3);

        CHECK(dd.findColumn("magnitude") == 0);
        CHECK(dd.findColumn("phase") == 1);
        CHECK(dd.findColumn("frequency") == 2);
        CHECK_FALSE(dd.findColumn("nonexistent").has_value());
    }

    SECTION("Wrong number of column names throws") {
        CHECK_THROWS_AS(
            dd.setColumnNames({"a", "b"}),  // 2 != last axis size 3
            std::invalid_argument);
        CHECK_THROWS_AS(
            dd.setColumnNames({"a", "b", "c", "d"}),  // 4 != 3
            std::invalid_argument);
    }
}

TEST_CASE("DimensionDescriptor column names on scalar throws", "[DimensionDescriptor]") {
    DimensionDescriptor dd;
    CHECK_THROWS_AS(dd.setColumnNames({"a"}), std::invalid_argument);
}

// =============================================================================
// Validation Tests
// =============================================================================

TEST_CASE("DimensionDescriptor rejects zero-size axes", "[DimensionDescriptor]") {
    CHECK_THROWS_AS(
        DimensionDescriptor({AxisDescriptor{"time", 0}}),
        std::invalid_argument);

    CHECK_THROWS_AS(
        DimensionDescriptor({
            AxisDescriptor{"time", 100},
            AxisDescriptor{"channel", 0}
        }),
        std::invalid_argument);
}

TEST_CASE("DimensionDescriptor rejects duplicate axis names", "[DimensionDescriptor]") {
    CHECK_THROWS_AS(
        DimensionDescriptor({
            AxisDescriptor{"time", 100},
            AxisDescriptor{"time", 50}
        }),
        std::invalid_argument);
}

TEST_CASE("DimensionDescriptor allows empty axis names", "[DimensionDescriptor]") {
    // Empty names are permitted (unnamed axes)
    DimensionDescriptor dd({
        AxisDescriptor{"", 10},
        AxisDescriptor{"", 20}
    });
    CHECK(dd.ndim() == 2);
    CHECK(dd.totalElements() == 200);
}

// =============================================================================
// Equality Tests
// =============================================================================

TEST_CASE("DimensionDescriptor equality", "[DimensionDescriptor]") {
    DimensionDescriptor a({
        AxisDescriptor{"time", 100},
        AxisDescriptor{"channel", 4}
    });
    DimensionDescriptor b({
        AxisDescriptor{"time", 100},
        AxisDescriptor{"channel", 4}
    });
    DimensionDescriptor c({
        AxisDescriptor{"time", 50},
        AxisDescriptor{"channel", 4}
    });

    CHECK(a == b);
    CHECK(a != c);

    // Column names affect equality
    a.setColumnNames({"a", "b", "c", "d"});
    CHECK(a != b);
    b.setColumnNames({"a", "b", "c", "d"});
    CHECK(a == b);
}

TEST_CASE("DimensionDescriptor default equality", "[DimensionDescriptor]") {
    DimensionDescriptor a;
    DimensionDescriptor b;
    CHECK(a == b);
}

// =============================================================================
// 1D special cases
// =============================================================================

TEST_CASE("DimensionDescriptor 1D column names", "[DimensionDescriptor]") {
    DimensionDescriptor dd({AxisDescriptor{"values", 5}});

    // For 1D, columns map to the single axis
    dd.setColumnNames({"a", "b", "c", "d", "e"});
    CHECK(dd.findColumn("c") == 2);
}

TEST_CASE("DimensionDescriptor single element per axis", "[DimensionDescriptor]") {
    DimensionDescriptor dd({
        AxisDescriptor{"a", 1},
        AxisDescriptor{"b", 1},
        AxisDescriptor{"c", 1}
    });

    CHECK(dd.totalElements() == 1);
    CHECK(dd.flatIndex({0, 0, 0}) == 0);
}
