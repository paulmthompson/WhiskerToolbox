#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CorePlotting/CoordinateTransform/AxisMapping.hpp"

#include <cmath>

using Catch::Matchers::WithinAbs;

// =============================================================================
// identityAxis
// =============================================================================

TEST_CASE("identityAxis: world == domain", "[AxisMapping]") {
    auto const m = CorePlotting::identityAxis("Voltage", 2);
    REQUIRE(m.isValid());
    REQUIRE(m.title == "Voltage");

    CHECK_THAT(m.worldToDomain(3.14), WithinAbs(3.14, 1e-9));
    CHECK_THAT(m.domainToWorld(3.14), WithinAbs(3.14, 1e-9));
}

TEST_CASE("identityAxis: label formatting", "[AxisMapping]") {
    auto const m = CorePlotting::identityAxis("", 1);
    CHECK(m.formatLabel(1.0) == "1");
    CHECK(m.formatLabel(2.5) == "2.5");
    CHECK(m.formatLabel(0.0) == "0");
}

// =============================================================================
// linearAxis
// =============================================================================

TEST_CASE("linearAxis: scale and offset", "[AxisMapping]") {
    auto const m = CorePlotting::linearAxis(2.0, 10.0, "Scaled");
    REQUIRE(m.isValid());

    // domain = world * 2 + 10
    CHECK_THAT(m.worldToDomain(0.0), WithinAbs(10.0, 1e-9));
    CHECK_THAT(m.worldToDomain(5.0), WithinAbs(20.0, 1e-9));
    CHECK_THAT(m.worldToDomain(-3.0), WithinAbs(4.0, 1e-9));

    // world = (domain - 10) / 2
    CHECK_THAT(m.domainToWorld(10.0), WithinAbs(0.0, 1e-9));
    CHECK_THAT(m.domainToWorld(20.0), WithinAbs(5.0, 1e-9));
}

TEST_CASE("linearAxis: roundtrip", "[AxisMapping]") {
    auto const m = CorePlotting::linearAxis(3.5, -7.0);
    double const original = 42.0;
    double const domain = m.worldToDomain(original);
    double const back = m.domainToWorld(domain);
    CHECK_THAT(back, WithinAbs(original, 1e-9));
}

// =============================================================================
// trialIndexAxis
// =============================================================================

TEST_CASE("trialIndexAxis: basic mapping", "[AxisMapping]") {
    auto const m = CorePlotting::trialIndexAxis(100);
    REQUIRE(m.isValid());
    REQUIRE(m.title == "Trial");

    // world -1 → trial 0
    CHECK_THAT(m.worldToDomain(-1.0), WithinAbs(0.0, 1e-9));
    // world +1 → trial 100
    CHECK_THAT(m.worldToDomain(1.0), WithinAbs(100.0, 1e-9));
    // world 0 → trial 50
    CHECK_THAT(m.worldToDomain(0.0), WithinAbs(50.0, 1e-9));
}

TEST_CASE("trialIndexAxis: inverse mapping", "[AxisMapping]") {
    auto const m = CorePlotting::trialIndexAxis(100);

    // trial 0 → world -1
    CHECK_THAT(m.domainToWorld(0.0), WithinAbs(-1.0, 1e-9));
    // trial 100 → world +1
    CHECK_THAT(m.domainToWorld(100.0), WithinAbs(1.0, 1e-9));
    // trial 50 → world 0
    CHECK_THAT(m.domainToWorld(50.0), WithinAbs(0.0, 1e-9));
}

TEST_CASE("trialIndexAxis: roundtrip", "[AxisMapping]") {
    auto const m = CorePlotting::trialIndexAxis(200);
    double const trial = 73.0;
    double const world = m.domainToWorld(trial);
    double const back = m.worldToDomain(world);
    CHECK_THAT(back, WithinAbs(trial, 1e-9));
}

TEST_CASE("trialIndexAxis: integer labels", "[AxisMapping]") {
    auto const m = CorePlotting::trialIndexAxis(100);
    CHECK(m.formatLabel(0.0) == "0");
    CHECK(m.formatLabel(42.0) == "42");
    CHECK(m.formatLabel(99.0) == "99");
    // Rounding
    CHECK(m.formatLabel(5.3) == "5");
    CHECK(m.formatLabel(5.7) == "6");
}

TEST_CASE("trialIndexAxis: convenience label()", "[AxisMapping]") {
    auto const m = CorePlotting::trialIndexAxis(100);
    // world 0 → domain 50 → label "50"
    CHECK(m.label(0.0) == "50");
    // world -1 → domain 0 → label "0"
    CHECK(m.label(-1.0) == "0");
}

// =============================================================================
// relativeTimeAxis
// =============================================================================

TEST_CASE("relativeTimeAxis: identity mapping", "[AxisMapping]") {
    auto const m = CorePlotting::relativeTimeAxis();
    REQUIRE(m.isValid());

    CHECK_THAT(m.worldToDomain(500.0), WithinAbs(500.0, 1e-9));
    CHECK_THAT(m.domainToWorld(-200.0), WithinAbs(-200.0, 1e-9));
}

TEST_CASE("relativeTimeAxis: label formatting", "[AxisMapping]") {
    auto const m = CorePlotting::relativeTimeAxis();
    CHECK(m.formatLabel(0.0) == "0");
    CHECK(m.formatLabel(500.0) == "+500");
    CHECK(m.formatLabel(-200.0) == "-200");
    CHECK(m.formatLabel(1.0) == "+1");
}

// =============================================================================
// analogAxis
// =============================================================================

TEST_CASE("analogAxis: gain and offset", "[AxisMapping]") {
    auto const m = CorePlotting::analogAxis(0.001, 0.0, "mV", 3);
    REQUIRE(m.isValid());

    CHECK_THAT(m.worldToDomain(1000.0), WithinAbs(1.0, 1e-9));
    CHECK_THAT(m.domainToWorld(1.0), WithinAbs(1000.0, 1e-9));
}

TEST_CASE("analogAxis: label formatting with unit", "[AxisMapping]") {
    auto const m = CorePlotting::analogAxis(1.0, 0.0, "mV", 2);
    CHECK(m.formatLabel(1.23) == "1.23 mV");
    CHECK(m.formatLabel(0.0) == "0 mV");
    CHECK(m.formatLabel(-0.5) == "-0.5 mV");
}

// =============================================================================
// Edge cases
// =============================================================================

TEST_CASE("AxisMapping: default constructed is invalid", "[AxisMapping]") {
    CorePlotting::AxisMapping m;
    CHECK_FALSE(m.isValid());
}

TEST_CASE("AxisMapping: copy semantics", "[AxisMapping]") {
    auto const original = CorePlotting::trialIndexAxis(50);
    auto copy = original;  // NOLINT
    CHECK(copy.isValid());
    CHECK_THAT(copy.worldToDomain(0.0), WithinAbs(25.0, 1e-9));
    CHECK(copy.title == "Trial");
}
