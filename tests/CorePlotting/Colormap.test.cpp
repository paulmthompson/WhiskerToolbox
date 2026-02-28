#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Colormaps/Colormap.hpp"

#include <glm/glm.hpp>

using namespace CorePlotting::Colormaps;

// =============================================================================
// Basic colormap function tests
// =============================================================================

TEST_CASE("getColormap returns valid function for all presets", "[Colormaps]")
{
    for (auto preset : allPresets()) {
        auto cmap = getColormap(preset);
        REQUIRE(cmap);

        // Evaluate at 0, 0.5, and 1 — all should produce valid RGBA
        auto c0 = cmap(0.0f);
        auto c5 = cmap(0.5f);
        auto c1 = cmap(1.0f);

        // Alpha should always be 1.0
        REQUIRE(c0.a == 1.0f);
        REQUIRE(c5.a == 1.0f);
        REQUIRE(c1.a == 1.0f);

        // RGB channels should be in [0, 1]
        REQUIRE(c0.r >= 0.0f);
        REQUIRE(c0.r <= 1.0f);
        REQUIRE(c5.g >= 0.0f);
        REQUIRE(c5.g <= 1.0f);
        REQUIRE(c1.b >= 0.0f);
        REQUIRE(c1.b <= 1.0f);
    }
}

TEST_CASE("Grayscale colormap linearity", "[Colormaps]")
{
    auto cmap = getColormap(ColormapPreset::Grayscale);

    // At t=0: black
    auto c0 = cmap(0.0f);
    REQUIRE_THAT(static_cast<double>(c0.r), Catch::Matchers::WithinAbs(0.0, 0.01));
    REQUIRE_THAT(static_cast<double>(c0.g), Catch::Matchers::WithinAbs(0.0, 0.01));
    REQUIRE_THAT(static_cast<double>(c0.b), Catch::Matchers::WithinAbs(0.0, 0.01));

    // At t=1: white
    auto c1 = cmap(1.0f);
    REQUIRE_THAT(static_cast<double>(c1.r), Catch::Matchers::WithinAbs(1.0, 0.01));
    REQUIRE_THAT(static_cast<double>(c1.g), Catch::Matchers::WithinAbs(1.0, 0.01));
    REQUIRE_THAT(static_cast<double>(c1.b), Catch::Matchers::WithinAbs(1.0, 0.01));

    // At t=0.5: gray
    auto c5 = cmap(0.5f);
    REQUIRE_THAT(static_cast<double>(c5.r), Catch::Matchers::WithinAbs(0.5, 0.02));
    REQUIRE_THAT(static_cast<double>(c5.r), Catch::Matchers::WithinAbs(static_cast<double>(c5.g), 0.001));
    REQUIRE_THAT(static_cast<double>(c5.r), Catch::Matchers::WithinAbs(static_cast<double>(c5.b), 0.001));
}

TEST_CASE("Colormap clamping for out-of-range inputs", "[Colormaps]")
{
    auto cmap = getColormap(ColormapPreset::Inferno);

    // Negative values should clamp to t=0
    auto c_neg = cmap(-1.0f);
    auto c_zero = cmap(0.0f);
    REQUIRE(c_neg.r == c_zero.r);
    REQUIRE(c_neg.g == c_zero.g);
    REQUIRE(c_neg.b == c_zero.b);

    // Values > 1 should clamp to t=1
    auto c_over = cmap(2.0f);
    auto c_one = cmap(1.0f);
    REQUIRE(c_over.r == c_one.r);
    REQUIRE(c_over.g == c_one.g);
    REQUIRE(c_over.b == c_one.b);
}

// =============================================================================
// mapValue tests
// =============================================================================

TEST_CASE("mapValue normalises to colormap range", "[Colormaps]")
{
    auto cmap = getColormap(ColormapPreset::Grayscale);

    // Value at vmin → black
    auto c = mapValue(cmap, 0.0f, 0.0f, 100.0f);
    REQUIRE_THAT(static_cast<double>(c.r), Catch::Matchers::WithinAbs(0.0, 0.01));

    // Value at vmax → white
    c = mapValue(cmap, 100.0f, 0.0f, 100.0f);
    REQUIRE_THAT(static_cast<double>(c.r), Catch::Matchers::WithinAbs(1.0, 0.01));

    // Midpoint → gray
    c = mapValue(cmap, 50.0f, 0.0f, 100.0f);
    REQUIRE_THAT(static_cast<double>(c.r), Catch::Matchers::WithinAbs(0.5, 0.02));
}

TEST_CASE("mapValue handles degenerate range", "[Colormaps]")
{
    auto cmap = getColormap(ColormapPreset::Grayscale);

    // When vmin == vmax, should return midpoint color
    auto c = mapValue(cmap, 5.0f, 5.0f, 5.0f);
    REQUIRE_THAT(static_cast<double>(c.r), Catch::Matchers::WithinAbs(0.5, 0.02));
}

// =============================================================================
// mapValues batch test
// =============================================================================

TEST_CASE("mapValues processes batch correctly", "[Colormaps]")
{
    auto cmap = getColormap(ColormapPreset::Grayscale);

    std::vector<double> values = {0.0, 50.0, 100.0};
    auto colors = mapValues(cmap, values, 0.0f, 100.0f);

    REQUIRE(colors.size() == 3);
    // First → black
    REQUIRE_THAT(static_cast<double>(colors[0].r), Catch::Matchers::WithinAbs(0.0, 0.01));
    // Middle → gray
    REQUIRE_THAT(static_cast<double>(colors[1].r), Catch::Matchers::WithinAbs(0.5, 0.02));
    // Last → white
    REQUIRE_THAT(static_cast<double>(colors[2].r), Catch::Matchers::WithinAbs(1.0, 0.01));
}

// =============================================================================
// presetName / allPresets
// =============================================================================

TEST_CASE("presetName returns non-empty strings", "[Colormaps]")
{
    for (auto preset : allPresets()) {
        REQUIRE(!presetName(preset).empty());
    }
}

TEST_CASE("allPresets includes all expected presets", "[Colormaps]")
{
    auto presets = allPresets();
    REQUIRE(presets.size() == 7);
}

// =============================================================================
// Coolwarm diverging colormap
// =============================================================================

TEST_CASE("Coolwarm colormap is diverging", "[Colormaps]")
{
    auto cmap = getColormap(ColormapPreset::Coolwarm);

    // At t=0: should be bluish (B > R)
    auto c0 = cmap(0.0f);
    REQUIRE(c0.b > c0.r);

    // At t=0.5: should be near white
    auto c5 = cmap(0.5f);
    REQUIRE_THAT(static_cast<double>(c5.r), Catch::Matchers::WithinAbs(1.0, 0.05));
    REQUIRE_THAT(static_cast<double>(c5.g), Catch::Matchers::WithinAbs(1.0, 0.05));
    REQUIRE_THAT(static_cast<double>(c5.b), Catch::Matchers::WithinAbs(1.0, 0.05));

    // At t=1: should be reddish (R > B)
    auto c1 = cmap(1.0f);
    REQUIRE(c1.r > c1.b);
}
